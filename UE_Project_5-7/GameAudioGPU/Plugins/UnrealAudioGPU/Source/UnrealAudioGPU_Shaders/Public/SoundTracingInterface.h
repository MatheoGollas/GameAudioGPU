

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "SoundTracingInterface.generated.h"

UINTERFACE(MinimalAPI)
class USoundTracingReceiver : public UInterface
{
	GENERATED_BODY()
};

class UNREALAUDIOGPU_SHADERS_API ISoundTracingReceiver
{
	GENERATED_BODY()
public:
	virtual void ReceiveSoundTracingData() = 0;
	
};