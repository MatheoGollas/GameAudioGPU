


#include "Shader_Processor.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class FShader_Processor : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FShader_Processor, Global, UNREALAUDIOGPU_SHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FShader_Processor, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(uint32, Resolution)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector2f>, OutTex)

		//SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, OutBuffer)

	END_SHADER_PARAMETER_STRUCT()

	static const uint32 NUM_THREADS_X = 8;
	static const uint32 NUM_THREADS_Y = 8;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& Environment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, Environment);
		Environment.SetDefine(TEXT("NUM_THREADS_X"), NUM_THREADS_X);
		Environment.SetDefine(TEXT("NUM_THREADS_Y"), NUM_THREADS_Y);
	}
};

IMPLEMENT_GLOBAL_SHADER(FShader_Processor, "/UnrealAudioGPU_Shaders/UnrealAudioGPU.usf", "ReturnUV", SF_Compute);

void FShader_ProcessorShaderInterface::AddPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, uint32 InResolution, FRDGTextureRef InTextureRef) //FRDGBufferRef
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "UrealAudioGPU");

	TShaderMapRef<FShader_Processor> ComputeShader(InShaderMap);

	FShader_Processor::FParameters* PassParams = GraphBuilder.AllocParameters<FShader_Processor::FParameters>();

	PassParams->Resolution = InResolution;
	PassParams->OutTex = GraphBuilder.CreateUAV(InTextureRef);

	//const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(InResolution, FComputeShaderUtils::kGolden2DGroupSize);
	const FIntVector GroupCount(FMath::DivideAndRoundUp(InResolution, FShader_Processor::NUM_THREADS_X), FMath::DivideAndRoundUp(InResolution, FShader_Processor::NUM_THREADS_Y), 1);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ReturnUV"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParams,
		GroupCount);
}
