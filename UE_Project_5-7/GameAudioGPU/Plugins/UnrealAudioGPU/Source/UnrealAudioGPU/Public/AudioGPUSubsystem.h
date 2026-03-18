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
	bool AddEmitterToBuffer(USceneComponent* InEmitter);

	UFUNCTION(BlueprintCallable)
	bool RemoveEmitterFromBuffer(USceneComponent* InEmitter);

	UFUNCTION(BlueprintCallable)
	bool UpdateTPAEmitters();

	UFUNCTION(BlueprintCallable)
	bool SoundTraceUpdate();

	UFUNCTION(BlueprintCallable)
	void SetListener(USceneComponent* cmpnt);

	UFUNCTION(BlueprintCallable)
	void SetCharacter(USceneComponent* cmpnt);

	FRHIGPUBufferReadback* TPE_Readback = nullptr;
	FRHIGPUBufferReadback* ST_Readback = nullptr;
	TWeakObjectPtr<USceneComponent> ListenerComponent;
	TWeakObjectPtr<USceneComponent> CharacterComponent;
	TArray<TWeakObjectPtr<USceneComponent>> Emitters;
	
private:

	TSharedPtr<class FSoudTracingViewExtension, ESPMode::ThreadSafe> ST_ViewExtension;
};
