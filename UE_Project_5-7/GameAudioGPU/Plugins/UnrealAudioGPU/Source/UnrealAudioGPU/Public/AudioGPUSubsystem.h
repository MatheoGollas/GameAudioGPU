

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
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
	bool SetRenderTarget(UTextureRenderTarget2D* InRT);

	UFUNCTION(BlueprintCallable)
	bool DrawRenderTarget();
private:
	TWeakObjectPtr<UTextureRenderTarget2D> RT;
};
