#pragma once

#include "CoreMinimal.h"
#include "AudioGPUSubsystem.h"
#include "RHIGPUReadback.h"

class UNREALAUDIOGPU_API TPE_ShaderProcessorShaderInterface
{
public:
	static void AddPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, FVector3f InListenerPos, FVector3f InCharacterPos, FRDGBufferRef BufferRef, FRHIGPUBufferReadback* Readback, TArray<TScriptInterface<IAudioEmitterGPU>> Components);
};
