#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDTypes.h"
#include "BattleHUDCombatantPresentationWidgetBase.generated.h"

class UBattleHUDCombatantPresentationWidgetBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FBattleHUDCombatantPresentationEvent,
	UBattleHUDCombatantPresentationWidgetBase*, Presentation
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FBattleHUDCombatantTargetRequested,
	int32, TargetId
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FBattleHUDCombatantPreviewRequested,
	int32, TargetId
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBattleHUDCombatantPreviewCleared);

/**
 * Presentation-only interaction contract for one visible combatant.
 *
 * The widget never decides target legality and never submits gameplay requests.
 * Its owner supplies the current ViewModel snapshot plus legal-target mapping,
 * then handles the emitted inspection/preview/target events.
 */
UCLASS(Abstract, Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDCombatantPresentationWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Combatant Presentation")
	void SetPresentationData(
		const FBattleHUDCombatantView& InCombatantView,
		bool bInTargetSelectionActive,
		bool bInLegalTarget,
		int32 InTargetId,
		bool bInTargetHighlighted = false
	);

	// Wire the character hit-area Button's OnHovered / OnUnhovered events here.
	// Focus is tracked automatically through NativeOnAdded/RemovedFromFocusPath.
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Combatant Presentation")
	void SetPointerInspectionActive(bool bActive);

	// Explicit optional pin request for a future touch/accessibility policy.
	// Normal primary click does not call this function.
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Combatant Presentation")
	bool RequestPinnedInspection();

	// Intended for the transparent hit-area Button's OnClicked event. It emits
	// only a gameplay-provided legal TargetId during target selection. Outside
	// target selection, normal primary click is intentionally a no-op.
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Combatant Presentation")
	bool RequestPrimaryInteraction();

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Combatant Presentation")
	bool IsTransientInspectionActive() const;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combatant Presentation")
	FBattleHUDCombatantView CombatantView;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combatant Presentation")
	bool bTargetSelectionActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combatant Presentation")
	bool bLegalTarget = false;

	// Visual selection affordance only. This remains independent from
	// bLegalTarget so later non-interactive emphasis never becomes target authority.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combatant Presentation")
	bool bTargetHighlighted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combatant Presentation")
	int32 TargetId = INDEX_NONE;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation")
	FBattleHUDCombatantPresentationEvent OnInspectRequested;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation")
	FBattleHUDCombatantPresentationEvent OnInspectCleared;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation")
	FBattleHUDCombatantPresentationEvent OnInspectPinRequested;

	// A3 PreviewTarget ownership is intentionally distinct from inspection even
	// though the same pointer/focus state may nominate both transient surfaces.
	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation|Preview")
	FBattleHUDCombatantPreviewRequested OnPreviewRequested;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation|Preview")
	FBattleHUDCombatantPreviewCleared OnPreviewCleared;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation")
	FBattleHUDCombatantTargetRequested OnTargetRequested;

protected:
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle HUD|Combatant Presentation", meta = (DisplayName = "Combatant Presentation Changed"))
	void BP_OnPresentationChanged();

private:
	void SetFocusInspectionActive(bool bActive);
	void PublishTransientInspectionState(bool bWasActive);
	void PublishTransientPreviewState();
	void ClearTransientInspection();
	bool RequestLegalTarget();

	bool bPointerInspectionActive = false;
	bool bFocusInspectionActive = false;
	int32 PublishedPreviewTargetId = INDEX_NONE;
};
