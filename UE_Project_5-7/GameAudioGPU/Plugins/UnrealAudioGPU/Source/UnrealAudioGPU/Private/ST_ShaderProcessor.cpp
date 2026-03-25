#include "ST_ShaderProcessor.h"

#include <Runtime/Renderer/Private/ScenePrivate.h>
#include <Runtime/Renderer/Private/RayTracing/RayTracingScene.h>
#include "RayTracingShaderBindingLayout.h"
#include "SceneViewExtension.h"
#include <SceneRendering.h>
#include "Components/SceneComponent.h"
#include "AudioGPUSubsystem.h"
//
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RayTracingPayloadType.h"
#include <Runtime/Renderer/Private/DeferredShadingRenderer.h>

IMPLEMENT_GLOBAL_SHADER(FSoundTracingRGS, "/Plugin/UnrealAudioGPU/Private/SoundTracing.usf", "SoundTracingRGS", SF_RayGen);
IMPLEMENT_SHADER_TYPE(, FSoundTracingCHS, TEXT("/Plugin/UnrealAudioGPU/Private/SoundTracing.usf"), TEXT("SoundTracingCHS"), SF_RayHitGroup);
IMPLEMENT_SHADER_TYPE(, FSoundTracingMS, TEXT("/Plugin/UnrealAudioGPU/Private/SoundTracing.usf"), TEXT("SoundTracingMS"), SF_RayMiss);

TAutoConsoleVariable<int32> CVarRTEnable(
	TEXT("r.RTFX"),
	1,
	TEXT("Enable or disable raytracing special effects."),
	ECVF_RenderThreadSafe | ECVF_Scalability);

FDelegateHandle PrepareRayTracingHandle;

void PrepareRayTracingShaders(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
{
	TShaderRef<FSoundTracingRGS> RayGenShader = View.ShaderMap->GetShader<FSoundTracingRGS>();
	OutRayGenShaders.Add(RayGenShader.GetRayTracingShader());
}

FSoudTracingViewExtension::FSoudTracingViewExtension(const FAutoRegister& AutoRegister, UAudioGPUSubsystem* subsystem) : FSceneViewExtensionBase(AutoRegister)
{
	Subsystem = subsystem;
#if RHI_RAYTRACING
	FGlobalIlluminationPluginDelegates::FPrepareRayTracing& PRTDelegate = FGlobalIlluminationPluginDelegates::PrepareRayTracing();
	PrepareRayTracingHandle = PRTDelegate.AddStatic(PrepareRayTracingShaders);
#endif
}

bool FSoudTracingViewExtension::RayTraceAvailable(const FSceneView& View)
{
	static const IConsoleVariable* CVarAllowPipeline = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.AllowPipeline"));
	if (CVarAllowPipeline)
		if (GRHISupportsRayTracingShaders && CVarAllowPipeline->GetInt() == 1) return true;
	return false;
}

void FSoudTracingViewExtension::PostTLASBuild_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	FScene* Scene = InView.Family->GetSceneRenderer()->GetScene();
	if (!Scene) return;

	const FRayTracingScene& RayTracingScene = Scene->RayTracingScene;
	if (!RayTracingScene.IsCreated()) return;

	if (!RayTraceAvailable(InView)) return;
	if (!Scene->World) return;



	if (CVarRTEnable.GetValueOnRenderThread() == 1)
	{
		const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InView);
		AddPass_RenderThread(GraphBuilder, RayTracingScene, ViewInfo);
	}
}


void FSoudTracingViewExtension::AddPass_RenderThread(FRDGBuilder& GraphBuilder, const FRayTracingScene& RayTracingScene, const FViewInfo& View)
{
	TArray<TScriptInterface<IAudioEmitterGPU>> EmittersCopy = Subsystem->Emitters;
	const int32 NumObjects = EmittersCopy.Num();

	if (NumObjects < 1) return;
	
	TArray<FVector3f> EmitterPositions;
	TArray<FVector4f> OutputData;

	for (TScriptInterface<IAudioEmitterGPU> emitter : EmittersCopy)
	{
		EmitterPositions.Add(FVector3f(emitter.GetInterface()->Execute_GetComponent(emitter.GetObject())->GetComponentLocation()));
		OutputData.Add(FVector4f::One());
	}

	FRDGBufferRef EmitterPositionBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(
			sizeof(FVector3f),
			NumObjects),
		TEXT("EmitterPosBuffer"));

	GraphBuilder.QueueBufferUpload(
		EmitterPositionBuffer,
		EmitterPositions.GetData(),
		sizeof(FVector3f) * NumObjects,
		ERDGInitialDataFlags::None);

	FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(
			sizeof(FVector4f),
			NumObjects),
		TEXT("OutputBuffer"));

	GraphBuilder.QueueBufferUpload(
		OutputBuffer,
		OutputData.GetData(),
		sizeof(FVector4f) * NumObjects,
		ERDGInitialDataFlags::None);

	FRDGBufferSRVRef PosBufferSRV = GraphBuilder.CreateSRV(EmitterPositionBuffer); // SRV -> read only in shader
	FRDGBufferUAVRef OutputBufferUAV = GraphBuilder.CreateUAV(OutputBuffer); // UAV -> read/write in shader (only write in our context)

	TShaderRef<FSoundTracingRGS> RayGenShader = View.ShaderMap->GetShader<FSoundTracingRGS>();
	TShaderRef<FSoundTracingCHS> ClosestHitShader = View.ShaderMap->GetShader<FSoundTracingCHS>();
	TShaderRef<FSoundTracingMS> MissShader = View.ShaderMap->GetShader<FSoundTracingMS>();

	FRayTracingPipelineStateInitializer Initializer;

	const FShaderBindingLayout* ShaderBindingLayout = RayTracing::GetShaderBindingLayout(View.GetShaderPlatform());
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


	FSoundTracingRGS::FParameters* Params = GraphBuilder.AllocParameters<FSoundTracingRGS::FParameters>();

	Params->CharacterPos = FVector3f(Subsystem->CharacterComponent.Get()->GetComponentLocation());
	Params->NumEmitters = NumObjects;
	Params->EmitterPosBuffer = PosBufferSRV;
	Params->OutputBuffer = OutputBufferUAV;
	Params->SceneBVH = RayTracingScene.GetLayerView(ERayTracingSceneLayer::Base, View.GetRayTracingSceneViewHandle());
	Params->NaniteRayTracing = Nanite::GetPublicGlobalRayTracingUniformBuffer();
	Params->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
	Params->ViewUniformBuffer = View.ViewUniformBuffer;
	
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Sound Tracing RGS "),
		Params,
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[Params, RayGenShader, &View, NumObjects]
		(FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			FRHIBatchedShaderParameters& GlobalResources = RHICmdList.GetScratchShaderParameters();
			SetShaderParameters(GlobalResources, RayGenShader, *Params);

			FRHIUniformBuffer* SceneUniformBuffer = Params->Scene->GetRHI();
			FRHIUniformBuffer* NaniteRayTracingUniformBuffer = Params->NaniteRayTracing->GetRHI();

			TOptional<FScopedUniformBufferStaticBindings> StaticUniformBufferScope =
				RayTracing::BindStaticUniformBufferBindings(View, SceneUniformBuffer, NaniteRayTracingUniformBuffer, RHICmdList);

			FRayTracingPipelineState* Pipeline = View.MaterialRayTracingData.PipelineState;
			FShaderBindingTableRHIRef SBT = View.MaterialRayTracingData.ShaderBindingTable;

			RHICmdList.RayTraceDispatch(
				Pipeline,
				RayGenShader.GetRayTracingShader(),
				SBT,
				GlobalResources,
				NumObjects,
				1);
		});
	
	{
		const uint32 NumOfBytes = sizeof(FVector4f) * NumObjects;

		FRHIGPUBufferReadback* readback = Subsystem->ST_Readback;
		if (readback == nullptr) return;

		AddEnqueueCopyPass(GraphBuilder, readback, OutputBuffer, NumOfBytes);

		if (readback->IsReady())
		{
			FVector4f* buffer = (FVector4f*)readback->Lock(NumOfBytes);

			const int maxIndex = (int)NumObjects;

			TArray<TPair<TScriptInterface<IAudioEmitterGPU>, FVector4f>> PendingUpdates;

			PendingUpdates.Reserve(maxIndex);

			for (int i = 0; i < maxIndex; i++)
			{
				FSoundTraceResult result = FSoundTraceResult(buffer[i]);
				PendingUpdates.Emplace(EmittersCopy[i], FVector4f(buffer[i]));
			}
			
			readback->Unlock();

			AsyncTask(ENamedThreads::GameThread, [PendingUpdates = MoveTemp(PendingUpdates)]()
				{
					for (const auto& Pair : PendingUpdates)
					{
						const TScriptInterface<IAudioEmitterGPU> emitter = Pair.Key;

						UObject* obj = emitter.GetObject();
						if (!IsValid(obj)) continue;

						FSoundTraceResult result;
						result.WPos_DidHit = Pair.Value;

						emitter.GetInterface()->Execute_ReceiveSoundTraceData(emitter.GetObject(), result);
					}
				});
		}
	};
}