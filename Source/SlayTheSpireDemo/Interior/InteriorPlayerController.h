#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InteriorPlayerController.generated.h"

UCLASS()
class SLAYTHESPIREDEMO_API AInteriorPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
