#pragma once

#include "CoreMinimal.h"
#include "BattleHUDWidget.h"
#include "BattleHUDReconciledWidget.generated.h"

/**
 * Generic G0 Native HUD reconciliation layer.
 *
 * Historical ViewModel publications carry exact dirty surfaces, and formal Hand
 * children retain RuntimeId identity across historical reconciliations. This
 * layer is intentionally Selection-agnostic; Selection subclasses add only their
 * transient interaction/presentation behavior on top.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDReconciledWidget : public UBattleHUDWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeDestruct() override;
	virtual void NativeOnBattleHUDViewModelChanged() override;
	virtual void RefreshHand() override;

private:
	UFUNCTION()
	void HandleReconciledCardRequested(int32 RuntimeId);

	void UnbindReconciledHandDelegates();
};
