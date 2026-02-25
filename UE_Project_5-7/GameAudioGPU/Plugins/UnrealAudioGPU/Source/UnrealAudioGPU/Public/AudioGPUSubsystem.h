

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include <UObject/WeakObjectPtrTemplates.h>
#include "AudioGPUSubsystem.generated.h"

/**
 * 
 */
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
	bool UpdateEmitters();

private:
	TWeakObjectPtr<USceneComponent> ListenerComponent;

};
