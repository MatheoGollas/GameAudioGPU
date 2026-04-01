#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Components/SceneComponent.h"
//#include "ST_ShaderProcessor.h"
#include <RenderGraphUtils.h>

#include "AudioGPUSubsystem.generated.h"

UCLASS()
class UNREALAUDIOGPU_API UAudioGPUSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	
	UFUNCTION(BlueprintCallable)
	bool AddEmitterToBuffer(TScriptInterface<IAudioEmitterGPU> InEmitter);

	UFUNCTION(BlueprintCallable)
	bool RemoveEmitterFromBuffer(TScriptInterface<IAudioEmitterGPU> InEmitter);

	UFUNCTION(BlueprintCallable)
	bool UpdateTPAEmitters();

	UFUNCTION(BlueprintCallable)
	void StartSoundTracing();

	UFUNCTION(BlueprintCallable)
	void StopSoundTracing();

	UFUNCTION(BlueprintCallable)
	void SetListener(USceneComponent* cmpnt);

	UFUNCTION(BlueprintCallable)
	void SetCharacter(USceneComponent* cmpnt);

	UFUNCTION(BlueprintCallable)
	void SetSoundTracingParams(const int NewNumRaysPerEmitter, const float NewShowerRadius, const float NewDefaultRayLength, const int frameSkips);

	UPROPERTY(BlueprintReadOnly)
	int RaysPerEmitter = 128;
	
	UPROPERTY(BlueprintReadOnly)
	float ShowerRadius = 50.0f;
	
	UPROPERTY(BlueprintReadOnly)
	float DefaultRayLength = 100000.0f;

	UPROPERTY(BlueprintReadOnly)
	int FrameSkips = 0;

	FRHIGPUBufferReadback* TPE_Readback = nullptr;
	FRHIGPUBufferReadback* ST_Readback = nullptr;
	TWeakObjectPtr<USceneComponent> ListenerComponent;
	TWeakObjectPtr<USceneComponent> CharacterComponent;
	TArray<TScriptInterface<IAudioEmitterGPU>> Emitters;
	
private:

	TSharedPtr<class FSoudTracingViewExtension, ESPMode::ThreadSafe> ST_ViewExtension;
};

USTRUCT(BlueprintType)
struct FSoundTraceResult
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector4f WPos_DidHit;
};

UENUM(BlueprintType)
enum SoundRayType
{
	Direct UMETA(DisplayName = "Direct hit to listener"),
	Shower   UMETA(DisplayName = "Shower pattern"),
	Hedgehog      UMETA(DisplayName = "HedgehogPattern")
};


UINTERFACE(MinimalAPI, Blueprintable)
class UAudioEmitterGPU : public UInterface
{
	GENERATED_BODY()
};

class IAudioEmitterGPU
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void ReceiveSoundTraceData(const TArray<FSoundTraceResult>& Data);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	const USceneComponent* GetComponent();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	const SoundRayType GetRayType();


};