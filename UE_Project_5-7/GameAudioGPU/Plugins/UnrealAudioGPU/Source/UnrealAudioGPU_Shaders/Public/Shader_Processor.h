

#pragma once


#include "CoreMinimal.h"

class UNREALAUDIOGPU_SHADERS_API FShader_ProcessorShaderInterface
{
	public:
		static void AddPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, uint32 InResolution, FRDGTextureRef InTextureRef);
};