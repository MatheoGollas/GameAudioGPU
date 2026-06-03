#include "ST_ShaderProcessor.h"

#include <Runtime/Renderer/Private/ScenePrivate.h>
#include <Runtime/Renderer/Private/RayTracing/RayTracingScene.h>
#include "RayTracingShaderBindingLayout.h"
#include "SceneViewExtension.h"
#include <SceneRendering.h>
#include "Components/SceneComponent.h"
#include "AudioGPUSubsystem.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RayTracingPayloadType.h"
#include <Runtime/Renderer/Private/DeferredShadingRenderer.h>
#include <bit>

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
	if(++currentFrameSkips < frameSkips) return;
	else currentFrameSkips = 0;

	if (Subsystem == nullptr) return;
	if (Subsystem->ST_Readback == nullptr) return;
	if (!Subsystem->ST_Readback->IsReady() && waitingForReadback) return;

	FScene* Scene = InView.Family->GetSceneRenderer()->GetScene();
	if (!Scene) return;

	const FRayTracingScene& RayTracingScene = Scene->RayTracingScene;
	if (!RayTracingScene.IsCreated()) return;

	if (!RayTraceAvailable(InView)) return;
	if (!Scene->World) return;

	SCOPED_NAMED_EVENT(SoundTracing_Setup, FColor::Red);

	if (CVarRTEnable.GetValueOnRenderThread() == 1)
	{
		const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InView);
		AddPass_RenderThread(GraphBuilder, RayTracingScene, ViewInfo);
	}
}


void FSoudTracingViewExtension::AddPass_RenderThread(FRDGBuilder& GraphBuilder, const FRayTracingScene& RayTracingScene, const FViewInfo& View)
{
	USceneComponent* characterComp = Subsystem->CharacterComponent.Get();
	if (!IsValid(characterComp)) return;

	FVector3f charPos = FVector3f(characterComp->GetComponentLocation());

	TArray<TScriptInterface<IAudioEmitterGPU>>* EmittersCopy = &Subsystem->Emitters;
	TArray<TScriptInterface<IAudioEmitterGPU>> Patterns;
	TArray<TScriptInterface<IAudioEmitterGPU>> Singles;
	const int32 NumObjects = EmittersCopy->Num();

	if (NumObjects < 1) return;

	const int32 RaysPerObject = Subsystem->RaysPerEmitter; // also equals numThreadsY
	
	TArray<FVector4f> PatternEmitterPositions;
	TArray<FVector4f> SingleEmitterPositions;
	TArray<FVector4f> OutputData;

	for (TScriptInterface<IAudioEmitterGPU> emitter : *EmittersCopy)
	{
		UObject* obj = emitter.GetObject();
		if (!IsValid(obj)) continue;
		
		const USceneComponent* comp = emitter.GetInterface()->Execute_GetComponent(obj);

		FVector pos = FVector::ZeroVector;
		if (IsValid(comp)) pos = comp->GetComponentLocation();
			
		SoundRayType type = emitter.GetInterface()->Execute_GetRayType(obj);
		float type_float = std::bit_cast<float>(uint32(type));
		TArray<FVector4f>* targetArray;
		TArray<TScriptInterface<IAudioEmitterGPU>>* targetArray2;
		switch (type)
		{
			case SoundRayType::Direct:
				targetArray = &SingleEmitterPositions;
				targetArray2 = &Singles;
			break;

			default:
				targetArray = &PatternEmitterPositions;
				targetArray2 = &Patterns;
			break;
		}
		targetArray->Add(FVector4f(pos.X, pos.Y, pos.Z, type_float));
		targetArray2->Add(emitter);
	}

	int numPatternEmitters = PatternEmitterPositions.Num();
	int numSingleEmitters = SingleEmitterPositions.Num();
	int numThreadsForSingles = FMath::CeilToInt(numSingleEmitters/ (float)RaysPerObject);
	int numThreadsX = numPatternEmitters + numThreadsForSingles;

	int outputBufferArraySize = numPatternEmitters * RaysPerObject + numSingleEmitters;
	OutputData.Init(FVector4f::Zero(), outputBufferArraySize);

	PatternEmitterPositions.Append(SingleEmitterPositions); // -> PatternEmitterPosistions is now the global input buffer
	int numGlobalInputBuffer = PatternEmitterPositions.Num();

	FRDGBufferRef EmitterPositionBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(
			sizeof(FVector4f),
			numGlobalInputBuffer),
		TEXT("EmitterPosBuffer"));

	GraphBuilder.QueueBufferUpload(
		EmitterPositionBuffer,
		PatternEmitterPositions.GetData(),
		sizeof(FVector4f) * numGlobalInputBuffer,
		ERDGInitialDataFlags::None);

	FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(
			sizeof(FVector4f),
			outputBufferArraySize),
		TEXT("OutputBuffer"));

	GraphBuilder.QueueBufferUpload(
		OutputBuffer,
		OutputData.GetData(),
		sizeof(FVector4f) * outputBufferArraySize,
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

	Params->CharacterPos = charPos;
	Params->NumThreadsX = numThreadsX;
	Params->NumRaysPerEmitter = RaysPerObject;
	Params->NumPatternEmitters = numPatternEmitters;
	Params->NumSingleEmitters = numSingleEmitters;
	Params->ShowerRadius = Subsystem->ShowerRadius;
	Params->defaultRayLength = Subsystem->DefaultRayLength;
	Params->EmitterPosBuffer = PosBufferSRV;
	Params->OutputBuffer = OutputBufferUAV;
	Params->SceneBVH = RayTracingScene.GetLayerView(ERayTracingSceneLayer::Base, View.GetRayTracingSceneViewHandle());
	Params->NaniteRayTracing = Nanite::GetPublicGlobalRayTracingUniformBuffer();
	Params->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
	Params->ViewUniformBuffer = View.ViewUniformBuffer;
	
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Sound Tracing RGS "),
		Params,
		ERDGPassFlags::AsyncCompute | ERDGPassFlags::NeverCull,
		[Params, RayGenShader, &View, NumObjects, RaysPerObject]
		(FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			SCOPED_DRAW_EVENT(RHICmdList, SoundTracingShader)
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
				RaysPerObject);
		});
	
	waitingForReadback = true;

	{
		const uint32 NumOfBytes = sizeof(FVector4f) * outputBufferArraySize;

		FRHIGPUBufferReadback* readback = Subsystem->ST_Readback;
		if (readback == nullptr) return;

		AddEnqueueCopyPass(GraphBuilder, readback, OutputBuffer, NumOfBytes);

		if (readback->IsReady())
		{
			FVector4f* buffer = (FVector4f*)readback->Lock(NumOfBytes);

			TArray<TPair<TScriptInterface<IAudioEmitterGPU>, TArray<FVector4f>>> PendingUpdates;
			PendingUpdates.Reserve(numGlobalInputBuffer);

			for (int i = 0; i < numPatternEmitters; i++)
			{
				TArray<FVector4f> outputData;
				for (int j = 0; j < RaysPerObject; j++)
				{
					int index = i * RaysPerObject + j;
					outputData.Add(FVector4f(buffer[index]));
				}
				PendingUpdates.Emplace(Patterns[i], outputData);
			}

			for (int i = 0; i < numSingleEmitters; i++)
			{
				TArray<FVector4f> outputData;
				int index = numPatternEmitters * RaysPerObject + i;
				outputData.Init(FVector4f(buffer[index]),1);
				PendingUpdates.Emplace(Singles[i], outputData);
			}
			
			readback->Unlock();
			waitingForReadback = false;

			AsyncTask(ENamedThreads::GameThread, [PendingUpdates = MoveTemp(PendingUpdates)]()
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(SoundTracing_ProcessReadback);
					for (const TPair<TScriptInterface<IAudioEmitterGPU>, TArray<FVector4f>>& Pair : PendingUpdates)
					{
						const TScriptInterface<IAudioEmitterGPU> emitter = Pair.Key;

						UObject* obj = emitter.GetObject();
						if (!IsValid(obj)) continue;

						TArray<FSoundTraceResult> resultArray;

						for(FVector4f data : Pair.Value)
						{
							FSoundTraceResult result;
							result.WPos_DidHit = data;
							resultArray.Add(result);
						}

						emitter.GetInterface()->Execute_ReceiveSoundTraceData(emitter.GetObject(), resultArray);
					}
				});
		}
	};
}