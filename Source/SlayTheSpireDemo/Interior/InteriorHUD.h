#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "InteriorHUD.generated.h"

UCLASS()
class SLAYTHESPIREDEMO_API AInteriorHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
