#pragma once

#include "CoreMinimal.h"
#include "AudioGPUSubsystem.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RayTracingPayloadType.h"
#include "RayTracingShaderBindingLayout.h"
#include "SceneViewExtension.h"
#include <Runtime/Renderer/Private/RayTracing/RayTracingScene.h>
#include <Runtime/Renderer/Private/DeferredShadingRenderer.h>

class FSoundTracingRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSoundTracingRGS);
	SHADER_USE_ROOT_PARAMETER_STRUCT(FSoundTracingRGS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, CharacterPos)
		SHADER_PARAMETER(uint32, NumEmitters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector>, EmitterPosBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, SceneBVH)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteRayTracingUniformParameters, NaniteRayTracing)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		//TODO: add more parameters as the shader evolves
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::Minimal;
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
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::Minimal;
	}

	static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Parameters)
	{
		return RayTracing::GetShaderBindingLayout(Parameters.Platform);
	}

	FSoundTracingCHS() = default;
	FSoundTracingCHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer){}
};

class FSoundTracingMS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSoundTracingMS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::Minimal;
	}

	static const FShaderBindingLayout* GetShaderBindingLayout(const FShaderPermutationParameters& Parameters)
	{
		return RayTracing::GetShaderBindingLayout(Parameters.Platform);
	}

	FSoundTracingMS() = default;
	FSoundTracingMS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer){}
};




class UNREALAUDIOGPU_API FSoudTracingViewExtension : public FSceneViewExtensionBase
{
public:
	FSoudTracingViewExtension(const FAutoRegister& AutoRegister, UAudioGPUSubsystem* subsystem);

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {};

	virtual void PostTLASBuild_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs) override {};
	static void RenderDiffuseIndirectLight_RenderThread(const FScene& Scene, const FViewInfo& View, FRDGBuilder& GraphBuilder, FGlobalIlluminationPluginResources& Resources) {};

	virtual ESceneViewExtensionFlags GetFlags() const override { return ESceneViewExtensionFlags::SubscribesToPostTLASBuild; }

private:
	TObjectPtr<UAudioGPUSubsystem> Subsystem;
	// TArray<ISoundTracingReceiver*> Receivers; // -> ref to where to send the data 
	FGlobalShaderMap* InShaderMap;
	FRHIGPUBufferReadback* Readback;
	bool RayTraceAvailable(const FSceneView& View);
	void AddPass_RenderThread(FRDGBuilder& GraphBuilder, const FRayTracingScene& RayTracingScene, const FViewInfo& View);
};