#include "SoundTracing.h"

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include <RenderGraphFwd.h>
#include "HAL/Platform.h"
#include "RHIGPUReadback.h"
#include "Async/TaskGraphInterfaces.h"
#include "RayTracingShaderBindingLayout.h"
#include "RayTracingPayloadType.h"
#include "SceneViewExtension.h"
//#include "PostProcess/PostProcessMaterial.h"
//#include "DeferredShadingRenderer.h"
#include <Math/MathFwd.h>
#include "SceneView.h"
#include "SceneInterface.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"
#include "RHIFwd.h"
#include "RendererInterface.h"
#include "RendererUtils.h"
#include "Runtime/Renderer/Private/SceneRendering.h"
#include "RHIResources.h"
#include "RenderGraphResources.h"
#include "SoundTracingInterface.h"
#include <Runtime/Renderer/Private/DeferredShadingRenderer.h>
//#include "RayTraceShaders.h"
//#include "ScenePrivate.h"


IMPLEMENT_GLOBAL_SHADER(FSoundTracingRGS, "/Plugin/UnrealAudioGPU/Private/SoundRayGen.usf", "SoundRayGenRGS", SF_RayGen);
IMPLEMENT_SHADER_TYPE(, FSoundTracingCHS, TEXT("/Plugin/UnrealAudioGPU/Private/SoundRayGen.usf"), TEXT("SoundRayGenCHS"), SF_RayHitGroup);
IMPLEMENT_SHADER_TYPE(, FSoundTracingMS, TEXT("/Plugin/UnrealAudioGPU/Private/SoundRayGen.usf"), TEXT("SoundRayGenMS"), SF_RayMiss);
IMPLEMENT_GLOBAL_SHADER(FSoundTracingCS, "/Plugin/UnrealAudioGPU/Private/SoundRayGen.usf", "SoundRayGenCS", SF_Compute);

TAutoConsoleVariable<int32> CVarRTEnable(
TEXT("r.RTFX"),
1,
TEXT("Enable or disable raytracing special effects."),
ECVF_RenderThreadSafe | ECVF_Scalability);

FDelegateHandle PrepareRayTracingHandle;

void PrepareRayTracingShaders(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	auto RayGenShader = View.ShaderMap->GetShader<FSoundTracingRGS>();
	OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
}

FSoundTracingShaderInterface::FSoundTracingShaderInterface(const FAutoRegister& AutoRegister, ISoundTracingReceiver* receiver) :
	FSceneViewExtensionBase(AutoRegister)
{
	Receivers.Add(receiver);

#if RHI_RAYTRACING
	FGlobalIlluminationPluginDelegates::FPrepareRayTracing& PRTDelegate = FGlobalIlluminationPluginDelegates::PrepareRayTracing();
	PrepareRayTracingHandle = PRTDelegate.AddStatic(PrepareRayTracingShaders);
#endif
}

bool FSoundTracingShaderInterface::RayTraceAvailable(const FSceneView& View)
{
	static const IConsoleVariable* CVarAllowPipeline = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.AllowPipeline"));
	if (CVarAllowPipeline)
	{
		// Get the value safely on the render thread
		if (GRHISupportsRayTracingShaders && CVarAllowPipeline->GetInt() == 1)
		{
			//RayTraceMethod = ERayTraceMethod::RGS;
			return true;
		}
	}
	static const IConsoleVariable* CVarAllowInline = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.AllowInline"));
	if (CVarAllowInline)
	{
		// Get the value safely on the render thread
		if (GRHISupportsInlineRayTracing && CVarAllowInline->GetInt() == 1)
		{
			//RayTraceMethod = ERayTraceMethod::Inline;
			//return true;
			return false;
		}
	}

	//RayTraceMethod = ERayTraceMethod::None;
	return false;
}

void FSoundTracingShaderInterface::PostTLASBuild_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	// Ensure we have raytracing scene
	FScene* Scene = InView.Family->GetSceneRenderer()->GetScene();
	if (!Scene) return;

	const FRayTracingScene& RayTracingScene = Scene->RayTracingScene;
	if (!RayTracingScene.IsCreated()) return;

	// Verify raytrace methods
	if (!RayTraceAvailable(InView)) return;

	// We are interested to renderer lasers only in a world that exists and can contain laser projectors
	if (!Scene->World) return;

	for (TObjectPtr<ISoundTracingReceiver> receiver : Receivers)
	{
		receiver.Get()->ReceiveSoundTracingData();
	}

	if (CVarRTEnable.GetValueOnRenderThread() == 1 /* && RayTraceMethod != ERayTraceMethod::None*/)
	{
		const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InView);
		AddPass_RenderThread(GraphBuilder, RayTracingScene, ViewInfo);
	}
}


void FSoundTracingShaderInterface::AddPass_RenderThread(FRDGBuilder& GraphBuilder, const FRayTracingScene& RayTracingScene, const FViewInfo& View)
{
	uint32 numElements = EmitterPositions.Num();

	FRDGBufferRef bufferRef = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(
			sizeof(FVector3f),
			numElements),
		TEXT("EmitterPosBuffer"));

	GraphBuilder.QueueBufferUpload(
		bufferRef,
		EmitterPositions.GetData(),
		sizeof(FVector3f) * numElements,
		ERDGInitialDataFlags::None);

	FRDGBufferSRVRef EmitterPosSRV = GraphBuilder.CreateSRV(bufferRef);

	RDG_GPU_STAT_SCOPE(GraphBuilder, FSoundTracing);
	RDG_EVENT_SCOPE(GraphBuilder, "SoundTracing");

	//TShaderMapRef<FSoundTracingCS> ComputeShader(InShaderMap);

	TShaderRef<FSoundTracingRGS> RayGenShader = InShaderMap->GetShader<FSoundTracingRGS>();
	TShaderRef<FSoundTracingCHS> ClosestHitShader = InShaderMap->GetShader<FSoundTracingCHS>();
	TShaderRef<FSoundTracingMS> MissShader = InShaderMap->GetShader<FSoundTracingMS>();

	FRayTracingPipelineStateInitializer Initializer;

	const FShaderBindingLayout* ShaderBindingLayout = RayTracing::GetShaderBindingLayout(InShaderMap->GetFirstSection()->GetShaderPlatform());
	if (ShaderBindingLayout)
	{
		Initializer.ShaderBindingLayout = &ShaderBindingLayout->RHILayout;
	}

	FRHIRayTracingShader* RayGenTable[] = { RayGenShader.GetRayTracingShader() };
	Initializer.SetRayGenShaderTable(RayGenTable);

	FRHIRayTracingShader* HitGroupTable[] = { ClosestHitShader.GetRayTracingShader() };
	Initializer.SetHitGroupTable(HitGroupTable);

	FRHIRayTracingShader* MissTable[] = { MissShader.GetRayTracingShader() };
	Initializer.SetMissShaderTable(MissTable);

	FRDGBufferUAVRef BufferUAVref = GraphBuilder.CreateUAV(bufferRef); // Write buffer for rays TODO: setup correctly


	FSoundTracingRGS::FParameters* PassParams = GraphBuilder.AllocParameters<FSoundTracingRGS::FParameters>();

	PassParams->ListenerPos = InListenerPos;
	PassParams->NumEmitters = numElements;
	PassParams->EmitterPosBuffer = EmitterPosSRV;
	PassParams->soundTraceBuffer = BufferUAVref;
	PassParams->SceneBVH = RayTracingScene.GetLayerView(ERayTracingSceneLayer::Base, View.GetRayTracingSceneViewHandle());
	//PassParams->NaniteRayTracing = Nanite::GetPublicGlobalRayTracingUniformBuffer();
	
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Sound Tracing", numElements, 1),
		PassParams,
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[PassParams, RayGenShader, numElements, &View]
		(FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			FRHIBatchedShaderParameters& GlobalResources = RHICmdList.GetScratchShaderParameters();
			SetShaderParameters(GlobalResources, RayGenShader, *PassParams);

			//FRHIUniformBuffer* SceneUniformBuffer = PassParams->Scene->GetRHI();;
			//FRHIUniformBuffer* NaniteRayTracingUniformBuffer = PassParams->NaniteRayTracing->GetRHI();

			/*TOptional<FScopedUniformBufferStaticBindings> StaticUniformBufferScope =
				RayTracing::BindStaticUniformBufferBindings(View, SceneUniformBuffer, NaniteRayTracingUniformBuffer, RHICmdList);*/

			FRayTracingPipelineState* Pipeline = View.MaterialRayTracingData.PipelineState;
			FShaderBindingTableRHIRef SBT = View.MaterialRayTracingData.ShaderBindingTable;

			RHICmdList.RayTraceDispatch(
				Pipeline,
				RayGenShader.GetRayTracingShader(),
				SBT,
				GlobalResources,
				numElements,
				1);
		});
}