#pragma once

#include "CoreMinimal.h"
#include <RenderGraphFwd.h>
#include <Math/MathFwd.h>
#include <GlobalShader.h>
#include <RenderGraphBuilder.h>
#include <Components/SceneComponent.h>
#include <RHIGPUReadback.h>
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "HAL/Platform.h"
#include "Async/TaskGraphInterfaces.h"
#include "RayTracingShaderBindingLayout.h"
#include "RayTracingPayloadType.h"
#include "SceneViewExtension.h"
//#include "PostProcess/PostProcessMaterial.h"
//#include "DeferredShadingRenderer.h"

class FSoundTracingRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSoundTracingRGS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FSoundTracingRGS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, SceneBVH)
		SHADER_PARAMETER(FVector3f, ListenerPos)
		SHADER_PARAMETER(FVector3f, CharacterPos)
		SHADER_PARAMETER(uint32, NumEmitters)
		//SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		//SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRayTracingUniformParameters, NaniteRayTracing)
		//SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(UAVStructuredBuffer<FVector2f>, soundTraceBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector>, EmitterPosBuffer)

	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::Default;
	}

	static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Parameters)
	{
		return RayTracing::GetShaderBindingLayout(Parameters.Platform);
	}
};

class FSoundTracingCHS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSoundTracingCHS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::Default;
	}

	static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Parameters)
	{
		return RayTracing::GetShaderBindingLayout(Parameters.Platform);
	}

	FSoundTracingCHS() = default;
	FSoundTracingCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}
};

class FSoundTracingMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSoundTracingMS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::Default;
	}

	static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Parameters)
	{
		return RayTracing::GetShaderBindingLayout(Parameters.Platform);
	}

	FSoundTracingMS() = default;
	FSoundTracingMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}
};

class FSoundTracingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSoundTracingCS);
	SHADER_USE_PARAMETER_STRUCT(FSoundTracingCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, SceneBVH)
		SHADER_PARAMETER_RDG_BUFFER_UAV(UAVStructuredBuffer<FVector2f>, soundTraceBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector>, EmitterPosBuffer)
		//SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		//SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER(FVector3f, ListenerPos)
		SHADER_PARAMETER(FVector3f, CharacterPos)
		SHADER_PARAMETER(uint32, NumEmitters)
		//SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRayTracingUniformParameters, NaniteRayTracing)
	END_SHADER_PARAMETER_STRUCT()

	static const uint32 NUM_THREADS = 32;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsRayTracingEnabledForProject(Parameters.Platform) && RHISupportsRayTracing(Parameters.Platform) && RHISupportsInlineRayTracing(Parameters.Platform);
		//return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
		//return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& Environment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, Environment);
		Environment.CompilerFlags.Add(CFLAG_Wave32);
		Environment.CompilerFlags.Add(CFLAG_InlineRayTracing);
		Environment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
		Environment.SetDefine(TEXT("NUM_THREADS"), NUM_THREADS);
	}
};

class UNREALAUDIOGPU_SHADERS_API FSoundTracingShaderInterface
{
public:
	static void AddPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, FVector3f InListenerPos, FRDGBufferRef BufferRef, FRHIGPUBufferReadback* Readback, FRDGBufferSRV* RayTracingScene);
};