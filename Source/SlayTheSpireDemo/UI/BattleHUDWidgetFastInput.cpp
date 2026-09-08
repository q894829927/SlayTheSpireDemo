#include "BattleHUDWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "../Battle/BattleSelectionRequest.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"

bool UBattleHUDWidget::SelectCard(
	int32 RuntimeId,
	bool bAllowFastPresentationCatchUp
)
{
	if (RuntimeId == INDEX_NONE || !IsValid(ViewModel))
	{
		return UBattleHUDWidgetBase::SelectCard(RuntimeId);
	}

	FPendingCardSelectionReadView PendingView;
	if (ViewModel->TryGetPendingCardSelectionReadView(PendingView))
	{
		const bool bAccepted = ViewModel->SubmitPendingCardSelectionByRuntimeId(RuntimeId);

		FPendingCardSelectionReadView UpdatedView;
		const bool bStillPending = ViewModel->TryGetPendingCardSelectionReadView(UpdatedView);
		if (IsValid(HB_Hand))
		{
			for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
			{
				if (UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index)))
				{
					const int32 CardRuntimeId = CardWidget->GetRuntimeId();
					const bool bCandidate = bStillPending
						&& UpdatedView.CandidateRuntimeIds.Contains(CardRuntimeId);
					const bool bSelected = bStillPending
						&& ViewModel->IsPendingCardSelectionRuntimeIdSelected(CardRuntimeId);
					CardWidget->SetPendingSelectionPresentation(
						bStillPending,
						bCandidate,
						bSelected
					);
				}
			}
		}

		if (IsValid(Btn_Confirm))
		{
			Btn_Confirm->SetVisibility(
				bStillPending ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			Btn_Confirm->SetIsEnabled(
				bStillPending && ViewModel->CanConfirmPendingCardSelection());
		}
		if (IsValid(Btn_Cancel))
		{
			const bool bCanCancelPending = bStillPending
				&& ViewModel->CanCancelPendingCardSelection();
			Btn_Cancel->SetVisibility(
				bCanCancelPending ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			Btn_Cancel->SetIsEnabled(bCanCancelPending);
		}

		if (IsValid(Txt_Feedback))
		{
			if (bStillPending)
			{
				const int32 SelectedCount = ViewModel->GetPendingCardSelectionSelectedCount();
				const bool bReadyToConfirm = ViewModel->CanConfirmPendingCardSelection();
				Txt_Feedback->SetText(bReadyToConfirm
					? FText::Format(
						NSLOCTEXT("BattleHUDWidget", "PendingCardSelectionReady", "已选择卡牌 {0}/{1}，请确认"),
						FText::AsNumber(SelectedCount),
						FText::AsNumber(UpdatedView.RequiredCount))
					: FText::Format(
						NSLOCTEXT("BattleHUDWidget", "PendingCardSelectionProgress", "选择卡牌 {0}/{1}"),
						FText::AsNumber(SelectedCount),
						FText::AsNumber(UpdatedView.RequiredCount)));
			}
			else
			{
				RefreshFeedback();
			}
		}
		return bAccepted;
	}

	if (!bAllowFastPresentationCatchUp)
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

	if (ViewModel->HasPendingCardSelection())
	{
		// The click that fast-forwarded the preceding Presentation ends at the
		// interaction-state transition. It must not be retried as the first click
		// of the newly exposed player Selection.
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
