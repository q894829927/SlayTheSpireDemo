#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleHUDWidgetG9BEndTurnProxy.generated.h"

class UBattleHUDWidget;

// Native-only adapter used to replace the EndTurn button's old direct binding
// without changing the sealed ViewModel RequestEndTurn fallback contract.
UCLASS(Transient)
class SLAYTHESPIREDEMO_API UBattleHUDWidgetG9BEndTurnProxy : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UBattleHUDWidget* InWidget);

	UFUNCTION()
	void HandleClicked();

private:
	TWeakObjectPtr<UBattleHUDWidget> Widget;
};
