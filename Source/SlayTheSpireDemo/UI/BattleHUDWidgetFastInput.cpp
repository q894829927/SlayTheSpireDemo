#include "BattleHUDWidget.h"

#include "BattleHUDViewModel.h"
#include "Containers/Ticker.h"

bool UBattleHUDWidget::SelectCard(
	int32 RuntimeId,
	bool bAllowFastPresentationCatchUp
)
{
	if (!bAllowFastPresentationCatchUp
		|| RuntimeId == INDEX_NONE
		|| !IsValid(ViewModel))
	{
		return UBattleHUDWidgetBase::SelectCard(RuntimeId);
	}

	// Keep at most one deferred UI request. Additional rapid clicks before the
	// next ticker turn replace the pending RuntimeId instead of forming a second
	// Gameplay command queue.
	if (bFastCardSelectionRetryScheduled)
	{
		PendingFastCardRuntimeId = RuntimeId;
		return true;
	}

	const bool bCanFastCatchUpPresentation =
		HasActiveNativePresentation()
		&& ViewModel->Outcome == EBattleHUDOutcome::None
		&& ViewModel->InteractionState == EBattleHUDInteractionState::Resolving
		&& ViewModel->bInputLocked;
	if (!bCanFastCatchUpPresentation)
	{
		// No Native visual owns the delay. Preserve the normal authoritative path,
		// including a genuine Gameplay ActionQueue ResolutionBusy rejection.
		return UBattleHUDWidgetBase::SelectCard(RuntimeId);
	}

	PendingFastCardRuntimeId = RuntimeId;
	bFastCardSelectionRetryScheduled = true;

	// Use the already-sealed exact-token catch-up path. Base Skip cancels only the
	// tracked presentation visual first; Controller Skip then synchronously
	// collapses to the newest frozen FinalSnapshot and refreshes live input bindings.
	// Gameplay state and ActionQueue authority are untouched.
	SkipPresentation();

	const TWeakObjectPtr<UBattleHUDWidget> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis](float /*DeltaTime*/)
			{
				if (UBattleHUDWidget* Widget = WeakThis.Get())
				{
					Widget->RetryPendingFastCardSelection();
				}
				return false;
			}),
		0.0f);

	return true;
}

void UBattleHUDWidget::RetryPendingFastCardSelection()
{
	if (!bFastCardSelectionRetryScheduled)
	{
		return;
	}

	const int32 RuntimeId = PendingFastCardRuntimeId;
	PendingFastCardRuntimeId = INDEX_NONE;
	bFastCardSelectionRetryScheduled = false;

	if (RuntimeId == INDEX_NONE || !IsValid(ViewModel))
	{
		return;
	}

	// Do not replace a terminal/unavailable surface with generic busy feedback.
	// Every other state retries exactly once through the unchanged ViewModel and
	// BattleManager request boundary. If Gameplay is genuinely still busy, that
	// authoritative retry still returns ResolutionBusy normally.
	if (ViewModel->Outcome != EBattleHUDOutcome::None
		|| ViewModel->InteractionState == EBattleHUDInteractionState::Terminal
		|| ViewModel->InteractionState == EBattleHUDInteractionState::PresentationUnavailable)
	{
		return;
	}

	UBattleHUDWidgetBase::SelectCard(RuntimeId);
}
