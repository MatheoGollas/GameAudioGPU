

#pragma once


#include "CoreMinimal.h"
#include <RenderGraphFwd.h>
#include <Math/MathFwd.h>
#include <GlobalShader.h>
#include <RenderGraphBuilder.h>
#include <Components/SceneComponent.h>
#include <RHIGPUReadback.h>
//#include "Shader_Processor.generated.h"

class UNREALAUDIOGPU_SHADERS_API FShader_ProcessorShaderInterface
{
	public:
		static void AddPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, FVector3f InListenerPos, FVector3f InCharacterPos, FRDGBufferRef BufferRef, FRHIGPUBufferReadback* Readback, TArray<TWeakObjectPtr<USceneComponent>> Components);
};