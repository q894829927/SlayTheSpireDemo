#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleCardWidget.generated.h"

/**
 * Native ownership boundary for the card Widget.
 *
 * R2 intentionally keeps this as a shell. Card view, request delegates and
 * input behavior are introduced by R4; the type exists here so the Native HUD
 * can expose a typed class selector without depending on a concrete WBP.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleCardWidget : public UUserWidget
{
	GENERATED_BODY()
};
