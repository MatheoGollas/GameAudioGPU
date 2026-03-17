// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioGPU.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FUnrealAudioGPUModule"

void FUnrealAudioGPUModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("UnrealAudioGPU"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/UnrealAudioGPU"), PluginShaderDir);
}

void FUnrealAudioGPUModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealAudioGPUModule, UnrealAudioGPU)