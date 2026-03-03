


#include "Shader_Processor.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include <RenderGraphFwd.h>
#include "HAL/Platform.h"
#include "RHIGPUReadback.h"
#include "Async/TaskGraphInterfaces.h"
#include <Math/MathFwd.h>

class FShader_Processor : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FShader_Processor, Global, UNREALAUDIOGPU_SHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FShader_Processor, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(FVector3f, ListenerPos)
		SHADER_PARAMETER(FVector3f, CharacterPos)
		SHADER_PARAMETER(uint32, NumEmitters)
		//SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector2f>, OutTex)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector>, EmitterPosBuffer)

	END_SHADER_PARAMETER_STRUCT()

	static const uint32 NUM_THREADS = 32;
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& Environment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, Environment);

		Environment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
		Environment.SetDefine(TEXT("NUM_THREADS"), NUM_THREADS);
	}
};

IMPLEMENT_GLOBAL_SHADER(FShader_Processor, "/UnrealAudioGPU_Shaders/UnrealAudioGPU.usf", "UpdateThirdPersonEmitters", SF_Compute);

void FShader_ProcessorShaderInterface::AddPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, FVector3f InListenerPos, FVector3f InCharacterPos, FRDGBufferRef BufferRef, FRHIGPUBufferReadback* Readback, TArray<TWeakObjectPtr<USceneComponent>> Components) //FRDGBufferRef
{
	ensure(IsInRenderingThread());

	RDG_GPU_STAT_SCOPE(GraphBuilder, FShader_Processor);
	RDG_EVENT_SCOPE(GraphBuilder, "UrealAudioGPU");

	TShaderMapRef<FShader_Processor> ComputeShader(InShaderMap);

	FShader_Processor::FParameters* PassParams = GraphBuilder.AllocParameters<FShader_Processor::FParameters>();

	FRDGBufferUAVRef BufferUAVref = GraphBuilder.CreateUAV(BufferRef);

	PassParams->ListenerPos = InListenerPos;
	PassParams->CharacterPos = InCharacterPos;
	PassParams->NumEmitters = Components.Num();
	PassParams->EmitterPosBuffer = BufferUAVref;

	uint32 numElements = BufferRef->Desc.NumElements;
	//const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(256, FComputeShaderUtils::kGolden2DGroupSize);
	const FIntVector GroupCount(FMath::DivideAndRoundUp(numElements, FShader_Processor::NUM_THREADS), 1, 1);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("UpdateThirdPersonEmitters"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParams,
		GroupCount);

	{
		const uint32 NumOfBytes = sizeof(FVector3f) * numElements;

		AddEnqueueCopyPass(GraphBuilder, Readback, BufferRef, NumOfBytes);

		if (Readback->IsReady())
		{
			FVector3f* buffer = (FVector3f*)Readback->Lock(NumOfBytes);

			const int maxIndex = FMath::Min(Components.Num(), (int)numElements);

			TArray<TPair<TWeakObjectPtr<USceneComponent>, FVector>> PendingUpdates;
			PendingUpdates.Reserve(maxIndex);

			for (int i = 0; i < maxIndex; i++)
			{
				if (Components[i].IsValid())
				{
					PendingUpdates.Emplace(Components[i], FVector(buffer[i]));
				}
			}

			Readback->Unlock();

			AsyncTask(ENamedThreads::GameThread, [PendingUpdates = MoveTemp(PendingUpdates)]()
			{
				for (const auto& Pair : PendingUpdates)
				{
					USceneComponent* cmpnt = Pair.Key.Get();
					if (cmpnt == nullptr)
					{
						continue;
					}

					const TArray<TObjectPtr<USceneComponent>>& Children = cmpnt->GetAttachChildren();
					for (USceneComponent* Child : Children)
					{
						if (Child != nullptr)
						{
							Child->SetWorldLocation(Pair.Value);
						}
					}
				}
			});
		}
	};
}
