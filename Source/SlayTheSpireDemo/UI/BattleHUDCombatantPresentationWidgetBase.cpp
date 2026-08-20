#include "BattleHUDCombatantPresentationWidgetBase.h"

void UBattleHUDCombatantPresentationWidgetBase::SetPresentationData(
	const FBattleHUDCombatantView& InCombatantView,
	bool bInTargetSelectionActive,
	bool bInLegalTarget,
	int32 InTargetId,
	bool bInTargetHighlighted
)
{
	CombatantView = InCombatantView;
	bTargetSelectionActive = bInTargetSelectionActive;
	bLegalTarget = bInTargetSelectionActive && bInLegalTarget && InTargetId != INDEX_NONE;
	TargetId = bLegalTarget ? InTargetId : INDEX_NONE;
	bTargetHighlighted = bLegalTarget || bInTargetHighlighted;

	BP_OnPresentationChanged();

	// Hover/focus may remain stationary while a new StateRevision arrives.
	// Republish the latest coherent View so the active inspector cannot go stale.
	if (IsTransientInspectionActive())
	{
		OnInspectRequested.Broadcast(this);
	}
}

void UBattleHUDCombatantPresentationWidgetBase::SetPointerInspectionActive(bool bActive)
{
	const bool bWasActive = IsTransientInspectionActive();
	bPointerInspectionActive = bActive;
	PublishTransientInspectionState(bWasActive);
}

bool UBattleHUDCombatantPresentationWidgetBase::RequestPinnedInspection()
{
	if (bTargetSelectionActive)
	{
		return false;
	}

	OnInspectPinRequested.Broadcast(this);
	return true;
}

bool UBattleHUDCombatantPresentationWidgetBase::RequestPrimaryInteraction()
{
	return bTargetSelectionActive && RequestLegalTarget();
}

bool UBattleHUDCombatantPresentationWidgetBase::IsTransientInspectionActive() const
{
	return bPointerInspectionActive || bFocusInspectionActive;
}

void UBattleHUDCombatantPresentationWidgetBase::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	SetFocusInspectionActive(true);
}

void UBattleHUDCombatantPresentationWidgetBase::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	SetFocusInspectionActive(false);
}

void UBattleHUDCombatantPresentationWidgetBase::NativeDestruct()
{
	const bool bWasActive = IsTransientInspectionActive();
	bPointerInspectionActive = false;
	bFocusInspectionActive = false;
	PublishTransientInspectionState(bWasActive);
	Super::NativeDestruct();
}

void UBattleHUDCombatantPresentationWidgetBase::SetFocusInspectionActive(bool bActive)
{
	const bool bWasActive = IsTransientInspectionActive();
	bFocusInspectionActive = bActive;
	PublishTransientInspectionState(bWasActive);
}

void UBattleHUDCombatantPresentationWidgetBase::PublishTransientInspectionState(bool bWasActive)
{
	const bool bIsActive = IsTransientInspectionActive();
	if (bIsActive == bWasActive)
	{
		return;
	}

	if (bIsActive)
	{
		OnInspectRequested.Broadcast(this);
	}
	else
	{
		OnInspectCleared.Broadcast(this);
	}
}

void UBattleHUDCombatantPresentationWidgetBase::ClearTransientInspection()
{
	const bool bWasActive = IsTransientInspectionActive();
	bPointerInspectionActive = false;
	bFocusInspectionActive = false;

	if (bWasActive)
	{
		OnInspectCleared.Broadcast(this);
	}
}

bool UBattleHUDCombatantPresentationWidgetBase::RequestLegalTarget()
{
	if (!bTargetSelectionActive || !bLegalTarget || TargetId == INDEX_NONE)
	{
		return false;
	}

	const int32 RequestedTargetId = TargetId;

	// A committed target choice ends the current transient inspection. Clearing
	// this before the synchronous request prevents the resulting StateRevision
	// refresh from reopening the inspector while the pointer remains stationary.
	ClearTransientInspection();
	OnTargetRequested.Broadcast(RequestedTargetId);
	return true;
}
