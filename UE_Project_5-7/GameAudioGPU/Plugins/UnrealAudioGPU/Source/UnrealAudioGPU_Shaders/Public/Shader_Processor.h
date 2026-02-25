

#pragma once


#include "CoreMinimal.h"
#include <RenderGraphFwd.h>
#include <Math/MathFwd.h>
#include <GlobalShader.h>
#include <RenderGraphBuilder.h>

class UNREALAUDIOGPU_SHADERS_API FShader_ProcessorShaderInterface
{
	public:
		static void AddPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, FVector InListenerPos, FVector InCharacterPos, FRDGBufferRef BufferRef);
};