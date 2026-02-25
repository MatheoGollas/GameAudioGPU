


#include "Shader_Processor.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include <RenderGraphFwd.h>
#include <Math/MathFwd.h>
#include <HAL/Platform.h>

class FShader_Processor : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FShader_Processor, Global, UNREALAUDIOGPU_SHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FShader_Processor, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(FVector3f, ListenerPos)
		SHADER_PARAMETER(FVector3f, CharacterPos)
		//SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector2f>, OutTex)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector>, EmitterPosBuffer)

	END_SHADER_PARAMETER_STRUCT()

	static const uint32 NUM_THREADS = 32;
	//static const uint32 NUM_THREADS_Y = 8;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		//return true;
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& Environment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, Environment);
		Environment.SetDefine(TEXT("NUM_THREADS"), NUM_THREADS);
	}
};

IMPLEMENT_GLOBAL_SHADER(FShader_Processor, "/UnrealAudioGPU_Shaders/UnrealAudioGPU.usf", "UpdateThirdPersonEmitters", SF_Compute);

void FShader_ProcessorShaderInterface::AddPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, FVector InListenerPos, FVector InCharacterPos, FRDGBufferRef BufferRef) //FRDGBufferRef
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "UrealAudioGPU");

	TShaderMapRef<FShader_Processor> ComputeShader(InShaderMap);

	FShader_Processor::FParameters* PassParams = GraphBuilder.AllocParameters<FShader_Processor::FParameters>();

	PassParams->ListenerPos = FVector3f(InListenerPos);
	PassParams->CharacterPos = FVector3f(InCharacterPos);
	PassParams->EmitterPosBuffer = GraphBuilder.CreateUAV(BufferRef);

	//const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(256, FComputeShaderUtils::kGolden2DGroupSize);
	const FIntVector GroupCount(FMath::DivideAndRoundUp(BufferRef->GetSize(), FShader_Processor::NUM_THREADS), 1, 1);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("UpdateThirdPersonEmitters"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParams,
		FIntVector(32,1,1));
}
