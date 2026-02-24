


#include "AudioGPUSubsystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "RenderGraphBuilder.h"
#include "Shader_Processor.h"

void UAudioGPUSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UAudioGPUSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool UAudioGPUSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer);
}

bool UAudioGPUSubsystem::SetRenderTarget(UTextureRenderTarget2D* InRT)
{
	if(IsValid(InRT))
	{
		RT = InRT;
		return true;
	}
	return false;
}

bool UAudioGPUSubsystem::DrawRenderTarget()
{
	if (!RT.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Render Target"));
		return false;
	}

	FTextureRenderTargetResource* RTResource = RT.Get()->GameThread_GetRenderTargetResource();

	const int32 Resolution = RT.Get()->SizeX;
	if (Resolution <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Render Target Resolution"));
		return false;
	}

	if (RT.Get()->SizeY != Resolution)
	{
		UE_LOG(LogTemp, Error, TEXT("Render Target must be square"));
		return false;
	}

	if (RT.Get()->GetFormat() != EPixelFormat::PF_G16R16F)
	{
		UE_LOG(LogTemp, Error, TEXT("Render Target must be in PF_G16R16F format"));
		return false;
	}
	
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid World"));
		return false;
	}
	/*
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Local Player"));
		return false;
	}
	*/

	ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();

	ENQUEUE_RENDER_COMMAND(AudioGPU)(
		[Resolution, RTResource, FeatureLevel](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);

			FRHITexture* OutputTexture = RTResource->GetRenderTargetTexture();
			FRDGTextureRef RDGTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(OutputTexture, TEXT("AudioGPUOutput")));

			FShader_ProcessorShaderInterface::AddPass_RenderThread(GraphBuilder, GlobalShaderMap, Resolution, RDGTexture);
			GraphBuilder.Execute();
		}
		);
	return true;
}
