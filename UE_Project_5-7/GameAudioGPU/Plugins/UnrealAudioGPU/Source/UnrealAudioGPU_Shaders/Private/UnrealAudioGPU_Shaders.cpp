// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioGPU_Shaders.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FUnrealAudioGPU_ShadersModule"

void FUnrealAudioGPU_ShadersModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("UnrealAudioGPU"))->GetBaseDir(),TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/UnrealAudioGPU"), PluginShaderDir);
}

void FUnrealAudioGPU_ShadersModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealAudioGPU_ShadersModule, UnrealAudioGPU_Shaders)