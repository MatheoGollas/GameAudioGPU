


#include "AudioGPUSubsystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "HAL/Platform.h"
#include "RHIGPUReadback.h"
#include "Shader_Processor.h"
#include <RenderGraphFwd.h>

void UAudioGPUSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Readback = new FRHIGPUBufferReadback(TEXT("AudioGPUReadback"));
}

void UAudioGPUSubsystem::Deinitialize()
{
	Super::Deinitialize();
	delete Readback;
	Readback = nullptr;
}

bool UAudioGPUSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer);
}

bool UAudioGPUSubsystem::AddEmitterToBuffer(USceneComponent* InEmitter)
{
	if (!IsValid(InEmitter))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Emitter Component"));
		return false;
	}

	if (Emitters.Contains(InEmitter))
	{
		UE_LOG(LogTemp, Warning, TEXT("Emitter already exists in buffer"));
		return true;
	}

	Emitters.Add(InEmitter);

	return true;
}

bool UAudioGPUSubsystem::RemoveEmitterFromBuffer(USceneComponent* InEmitter)
{
	if (!IsValid(InEmitter))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Emitter Component"));
		return false;
	}

	if (!Emitters.Contains(InEmitter))
	{
		UE_LOG(LogTemp, Warning, TEXT("Emitter not in buffer"));
		return false;
	}

	Emitters.Remove(InEmitter);

	return true;
}

bool UAudioGPUSubsystem::UpdateEmitters()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid World"));
		return false;
	}

	if (Emitters.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("No emitters to update"));
		return true;
	}

	FVector3f CharacterPos;

	if (IsValid(CharacterComponent.Get()))
	{
		CharacterPos = FVector3f(CharacterComponent.Get()->GetComponentLocation());
	}
	else
	{
		ULocalPlayer* LocalPlayer = GetLocalPlayer();
		if (!IsValid(LocalPlayer))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid Local Player"));
			return false;
		}

		APlayerController* Controller = LocalPlayer->GetPlayerController(World);
		if (!IsValid(LocalPlayer))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid Local Player"));
			return false;
		}

		APawn* Pawn = Controller->GetPawn();
		CharacterPos = FVector3f(Pawn->GetActorLocation());
	}

	if (!IsValid(ListenerComponent.Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Listener Component"));
		return false;
	}
	FVector3f ListenerPos = FVector3f(ListenerComponent.Get()->GetComponentLocation());
	
	ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();

	TArray<TWeakObjectPtr<USceneComponent>> EmitterComponents = Emitters;

	FRHIGPUBufferReadback* ReadbackCopy = Readback;

	ENQUEUE_RENDER_COMMAND(AudioGPU)(
		[ListenerPos, CharacterPos, EmitterComponents, FeatureLevel, ReadbackCopy](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);

			TArray<FVector3f> EmitterPositions;
			for (TWeakObjectPtr<USceneComponent> cmpnt : EmitterComponents)
			{
				EmitterPositions.Add(FVector3f(cmpnt.Get()->GetComponentLocation()));
			}

			FRDGBufferRef BufferRef =
			CreateStructuredBuffer(GraphBuilder, TEXT("EmitterPosBuffer"), sizeof(FVector3f), EmitterPositions.Num(), EmitterPositions.GetData(), (uint64)EmitterPositions.Num() * sizeof(FVector3f));

			FShader_ProcessorShaderInterface::AddPass_RenderThread(GraphBuilder, GlobalShaderMap, ListenerPos, CharacterPos, BufferRef, ReadbackCopy, EmitterComponents);

			GraphBuilder.Execute();
		}
		);
	return true;
}

void UAudioGPUSubsystem::SetListener(USceneComponent* cmpnt)
{
	if (!IsValid(cmpnt))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Listener Component"));
		return;
	}
	ListenerComponent = cmpnt;
}

void UAudioGPUSubsystem::SetCharacter(USceneComponent* cmpnt)
{
	if (!IsValid(cmpnt))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Character Component"));
		return;
	}
	CharacterComponent = cmpnt;
}
