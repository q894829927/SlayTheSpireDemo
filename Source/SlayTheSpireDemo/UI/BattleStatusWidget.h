#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleStatusWidget.generated.h"

/**
 * Native ownership boundary for the status Widget.
 *
 * R2 intentionally keeps this as a shell. Frozen status view and exact
 * identity access are introduced by R9; this type only enables typed Native
 * HUD class selection and WBP reparenting.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleStatusWidget : public UUserWidget
{
	GENERATED_BODY()
};
