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
	PublishTransientPreviewState();

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
	PublishTransientPreviewState();
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
	PublishTransientPreviewState();
	Super::NativeDestruct();
}

void UBattleHUDCombatantPresentationWidgetBase::SetFocusInspectionActive(bool bActive)
{
	const bool bWasActive = IsTransientInspectionActive();
	bFocusInspectionActive = bActive;
	PublishTransientInspectionState(bWasActive);
	PublishTransientPreviewState();
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

void UBattleHUDCombatantPresentationWidgetBase::PublishTransientPreviewState()
{
	const int32 DesiredPreviewTargetId =
		IsTransientInspectionActive()
		&& bTargetSelectionActive
		&& bLegalTarget
		&& TargetId != INDEX_NONE
			? TargetId
			: INDEX_NONE;

	if (DesiredPreviewTargetId == PublishedPreviewTargetId)
	{
		return;
	}

	if (PublishedPreviewTargetId != INDEX_NONE)
	{
		PublishedPreviewTargetId = INDEX_NONE;
		OnPreviewCleared.Broadcast();
	}

	if (DesiredPreviewTargetId != INDEX_NONE)
	{
		PublishedPreviewTargetId = DesiredPreviewTargetId;
		OnPreviewRequested.Broadcast(DesiredPreviewTargetId);
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
	PublishTransientPreviewState();
}

bool UBattleHUDCombatantPresentationWidgetBase::RequestLegalTarget()
{
	if (!bTargetSelectionActive || !bLegalTarget || TargetId == INDEX_NONE)
	{
		return false;
	}

	const int32 RequestedTargetId = TargetId;

	// A committed target choice ends current transient inspection and Preview.
	// Clearing both before the synchronous target request guarantees the A3
	// surface is gone before authoritative RequestPlayCard/A2 playback begins.
	ClearTransientInspection();
	OnTargetRequested.Broadcast(RequestedTargetId);
	return true;
}
