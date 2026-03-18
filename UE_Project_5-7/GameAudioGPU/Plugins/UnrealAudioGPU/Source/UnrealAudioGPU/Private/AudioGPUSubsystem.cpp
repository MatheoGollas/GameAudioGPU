#include "AudioGPUSubsystem.h"


#include "RHIGPUReadback.h"
#include "TPE_ShaderProcessor.h"
#include "ST_ShaderProcessor.h"
#include "SceneViewExtension.h"


void UAudioGPUSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TPE_Readback = new FRHIGPUBufferReadback(TEXT("AudioGPUReadback_TPE"));
	ST_Readback = new FRHIGPUBufferReadback(TEXT("AudioGPUReadback_ST"));

	ST_ViewExtension = FSceneViewExtensions::NewExtension<FSoudTracingViewExtension>(this);
}

void UAudioGPUSubsystem::Deinitialize()
{
	Super::Deinitialize();
	delete TPE_Readback;
	TPE_Readback = nullptr;
	delete ST_Readback;
	ST_Readback = nullptr;

	{
		ST_ViewExtension->IsActiveThisFrameFunctions.Empty();
		FSceneViewExtensionIsActiveFunctor IsActiveFunctor;

		IsActiveFunctor.IsActiveFunction = [](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
			{
				return TOptional<bool>(false);
			};

		ST_ViewExtension->IsActiveThisFrameFunctions.Add(IsActiveFunctor);
	}

	ST_ViewExtension.Reset();
	ST_ViewExtension = nullptr;
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

bool UAudioGPUSubsystem::UpdateTPAEmitters()
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

	FRHIGPUBufferReadback* ReadbackCopy = TPE_Readback;

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

			TPE_ShaderProcessorShaderInterface::AddPass_RenderThread(GraphBuilder, GlobalShaderMap, ListenerPos, CharacterPos, BufferRef, ReadbackCopy, EmitterComponents);

			GraphBuilder.Execute();
		}
		);
	return true;
}

bool UAudioGPUSubsystem::SoundTraceUpdate()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid World"));
		return false;
	}

	if (Emitters.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("No emitters to trace"));
		return true;
	}

	if (!IsValid(ListenerComponent.Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid Listener Component"));
		return false;
	}
	/*FVector3f ListenerPos = FVector3f(ListenerComponent.Get()->GetComponentLocation());

	FSceneInterface* Scene = World->Scene;

	ERHIFeatureLevel::Type FeatureLevel = Scene->GetFeatureLevel();

	TArray<TWeakObjectPtr<USceneComponent>> EmitterComponents = Emitters;

	FRHIGPUBufferReadback* ReadbackCopy = ST_Readback;*/

	return true;
}
