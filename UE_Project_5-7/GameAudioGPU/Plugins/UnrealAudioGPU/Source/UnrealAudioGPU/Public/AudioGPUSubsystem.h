

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include <UObject/WeakObjectPtrTemplates.h>
#include <Containers/Array.h>
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
	bool AddEmitterToBuffer(USceneComponent* InEmitter/*, uint32& index*/);

	UFUNCTION(BlueprintCallable)
	bool RemoveEmitterFromBuffer(USceneComponent* InEmitter/*, uint32& index*/);

	UFUNCTION(BlueprintCallable)
	bool UpdateEmitters();

	UFUNCTION(BlueprintCallable)
	void SetListener(USceneComponent* cmpnt);

	UFUNCTION(BlueprintCallable)
	void SetCharacter(USceneComponent* cmpnt);

	FRHIGPUBufferReadback* Readback = nullptr;

private:
	TWeakObjectPtr<USceneComponent> ListenerComponent;
	TWeakObjectPtr<USceneComponent> CharacterComponent;
	TArray<TWeakObjectPtr<USceneComponent>> Emitters;
	//uint32 currentKey = 0u;
};
