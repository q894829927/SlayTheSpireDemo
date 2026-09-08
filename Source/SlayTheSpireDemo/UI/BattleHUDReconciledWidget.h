#pragma once

#include "CoreMinimal.h"
#include "BattleHUDWidget.h"
#include "BattleHUDReconciledWidget.generated.h"

class UBattleHUDViewModel;

/**
 * Generic G0 Native HUD reconciliation layer.
 *
 * Historical ViewModel publications carry exact dirty surfaces, formal Hand
 * children retain RuntimeId identity, and transient card ownership changes are
 * consumed independently of historical snapshot publication. This layer is
 * intentionally Selection-agnostic.
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

	void EnsureOwnershipDelegateBinding();
	void HandleCardPresentationOwnershipChanged(const TArray<int32>& RuntimeIds);
	void ApplyExplicitCardPresentationOwnershipToFormalHand();
	void UnbindReconciledHandDelegates();

	TWeakObjectPtr<UBattleHUDViewModel> OwnershipBoundViewModel;
};
