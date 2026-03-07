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
//#include "SceneViewExtension.h"
//#include "PostProcess/PostProcessMaterial.h"
//#include "DeferredShadingRenderer.h"
#include <Math/MathFwd.h>


IMPLEMENT_GLOBAL_SHADER(FSoundTracingRGS, "/Plugin/UnrealAudioGPU/Private/SoundRayGen.usf", "SoundRayGenRGS", SF_RayGen);
IMPLEMENT_SHADER_TYPE(FSoundTracingCHS, TEXT("/Plugin/UnrealAudioGPU/Private/SoundRayGen.usf"), TEXT("SoundRayGenCHS"), SF_RayHitGroup);
IMPLEMENT_SHADER_TYPE(FSoundTracingMS, TEXT("/Plugin/UnrealAudioGPU/Private/SoundRayGen.usf"), TEXT("SoundRayGenMS"), SF_RayMiss);
IMPLEMENT_GLOBAL_SHADER(FSoundTracingCS, "/Plugin/UnrealAudioGPU/Private/SoundRayGen.usf", "SoundRayGenCS", SF_Compute);

TAutoConsoleVariable<int32> CVarRTEnable(
TEXT("r.RTFX"),
1,
TEXT("Enable or disable raytracing special effects."),
ECVF_RenderThreadSafe | ECVF_Scalability);

FDelegateHandle PrepareRayTracingHandle;


void FSoundTracingShaderInterface::AddPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, FVector3f InListenerPos, FRDGBufferRef BufferRef, FRHIGPUBufferReadback* Readback, FRDGBufferSRV* RayTracingScene)
{
	//ensure(IsInRenderingThread());

	RDG_GPU_STAT_SCOPE(GraphBuilder, FSoundTracing);
	RDG_EVENT_SCOPE(GraphBuilder, "SoundTracing");

	TShaderMapRef<FSoundTracingCS> ComputeShader(InShaderMap);

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

	FSoundTracingRGS::FParameters* PassParams = GraphBuilder.AllocParameters<FSoundTracingRGS::FParameters>();

	FRDGBufferSRVRef BufferSRVref = GraphBuilder.CreateSRV(BufferRef); // Read buffer for emitter positions
	FRDGBufferUAVRef BufferUAVref = GraphBuilder.CreateUAV(BufferRef); // Write buffer for rays TODO: setup correctly

	uint32 numElements = BufferRef->Desc.NumElements;

	PassParams->ListenerPos = InListenerPos;
	PassParams->NumEmitters = numElements;
	PassParams->EmitterPosBuffer = BufferSRVref;
	PassParams->soundTraceBuffer = BufferUAVref;
	PassParams->SceneBVH = RayTracingScene;
	//PassParams->NaniteRayTracing = Nanite::GetPublicGlobalRayTracingUniformBuffer();

	//PassParams->ObjectLocations = LocationBufferSRV;
	//PassParams->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);


	//const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(256, FComputeShaderUtils::kGolden2DGroupSize);
	/*const FIntVector GroupCount(FMath::DivideAndRoundUp(numElements, FSoundTracingCS::NUM_THREADS), 1, 1);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SoundRayGen"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParams,
		GroupCount);

	{
		const uint32 NumOfBytes = sizeof(FVector3f) * numElements;

		AddEnqueueCopyPass(GraphBuilder, Readback, BufferRef, NumOfBytes);
	};*/


	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Sound Tracing", numElements, 1),
		PassParams,
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[PassParams, RayGenShader, numElements]
		(FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			FRHIBatchedShaderParameters& GlobalResources = RHICmdList.GetScratchShaderParameters();
			SetShaderParameters(GlobalResources, RayGenShader, *PassParams);

			FRHIUniformBuffer* SceneUniformBuffer;
			FRHIUniformBuffer* NaniteRayTracingUniformBuffer;

			FRayTracingPipelineState* Pipeline;
			FShaderBindingTableRHIRef SBT;

			RHICmdList.RayTraceDispatch(
				Pipeline,
				RayGenShader.GetRayTracingShader(),
				SBT,
				GlobalResources,
				numElements,
				1);
		});
}
/*
	// Create the structured buffer for the origin/target world coordinates
	FRDGBufferRef LaserPositionBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(
			sizeof(FLaserData),
			NumObjects),
		TEXT("LaserObjectPositionBuffer"));

	// RDG needs to make copy, data is updated inside the subsystem loop
	GraphBuilder.QueueBufferUpload(
		LaserPositionBuffer,
		LaserDataBuffer.GetData(),
		sizeof(FLaserData) * NumObjects,
		ERDGInitialDataFlags::None);
*/
