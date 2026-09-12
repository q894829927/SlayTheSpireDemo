#include "BattleHUDWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "BattleHUDWidgetG9BInput.h"
#include "../Battle/BattleSelectionRequest.h"
#include "../Presentation/BattlePresentationController.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"

namespace
{
	struct FPendingFastCardCredential
	{
		FPresentationSessionToken SessionToken;
		int64 BattleId = 0;
		int64 ExpectedCatchUpRevision = 0;

		bool IsValid() const
		{
			return SessionToken.IsValid()
				&& BattleId > 0
				&& ExpectedCatchUpRevision > 0;
		}
	};

	// The existing Native HUD already owns the deferred RuntimeId and retry flag.
	// G8-B adds the exact immutable authority credential beside that legacy storage
	// without changing the UCLASS layout. Weak keys are pruned on every input and
	// retry; no UObject is kept alive by this shadow/deferred metadata.
	TMap<TWeakObjectPtr<UBattleHUDWidget>, FPendingFastCardCredential> GPendingFastCardCredentials;

	void PruneDeadFastCardCredentials()
	{
		for (auto It = GPendingFastCardCredentials.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	bool IsDisplayedGameplayCard(
		const UBattleHUDViewModel* ViewModel,
		int32 RuntimeId)
	{
		return IsValid(ViewModel)
			&& RuntimeId != INDEX_NONE
			&& ViewModel->HandCards.ContainsByPredicate(
				[RuntimeId](const FBattleHUDCardView& Card)
				{
					return Card.RuntimeId == RuntimeId && Card.bGameplayPlayable;
				});
	}
}

bool UBattleHUDWidget::SelectCard(
	int32 RuntimeId,
	bool bAllowFastPresentationCatchUp
)
{
	PruneDeadFastCardCredentials();
	if (RuntimeId == INDEX_NONE || !IsValid(ViewModel))
	{
		return UBattleHUDWidgetBase::SelectCard(RuntimeId);
	}

	// A stale retirement fence exists only to kill an older scheduled G8 retry.
	// If no such retry exists at this new physical click, it cannot constrain the
	// new input window.
	if (!bFastCardSelectionRetryScheduled)
	{
		BattleHUDWidgetG9BInput::ClearFastInputRetirementFence(this);
	}

	// Accepted EndTurn is decisive until it executes or becomes stale. Do not
	// schedule a new card future intent behind it.
	if (BattleHUDWidgetG9BInput::IsEnabled() && ViewModel->HasBufferedEndTurnG9())
	{
		return false;
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

	// Gameplay may already be authoritatively waiting for this card selection
	// while the displayed Presentation revision is still catching up to the
	// interactive boundary. In that window candidates are intentionally hidden,
	// but ordinary card-play input must also be blocked. Never reinterpret a
	// selection click as a new play command merely because the read view is gated.
	if (ViewModel->HasAuthoritativePendingCardSelection())
	{
		return false;
	}

	if (bAllowFastPresentationCatchUp)
	{
		switch (BattleHUDWidgetG9BInput::TryHandleBufferedCardClick(this, RuntimeId))
		{
		case EG9BCardClickDisposition::Buffered:
			// Exact sealed target exists: keep the current card Presentation alive.
			// G9 replays this one selection when that exact target becomes displayed.
			return true;
		case EG9BCardClickDisposition::Rejected:
			return false;
		case EG9BCardClickDisposition::NotHandled:
		default:
			break;
		}
	}

	if (!bAllowFastPresentationCatchUp)
	{
		return UBattleHUDWidgetBase::SelectCard(RuntimeId);
	}

	// Preserve the existing one-request behavior: another click before the next
	// ticker retry only replaces the RuntimeId; it cannot replace the session or
	// target revision credential captured by the first physical input boundary.
	if (bFastCardSelectionRetryScheduled)
	{
		if (GPendingFastCardCredentials.Contains(this))
		{
			PendingFastCardRuntimeId = RuntimeId;
			return true;
		}
		PendingFastCardRuntimeId = INDEX_NONE;
		bFastCardSelectionRetryScheduled = false;
	}

	FPresentationSessionToken SessionToken;
	int64 TargetBattleId = 0;
	int64 ExpectedCatchUpRevision = 0;
	const bool bHasSkippableDelay = IsValid(PresentationController)
		&& PresentationController->TryCaptureFastInputCatchUpTarget(
			SessionToken,
			TargetBattleId,
			ExpectedCatchUpRevision);
	const bool bCanFastCatchUpPresentation =
		bHasSkippableDelay
		&& ViewModel->Outcome == EBattleHUDOutcome::None
		&& ViewModel->InteractionState == EBattleHUDInteractionState::Resolving
		&& ViewModel->bInputLocked;
	if (!bCanFastCatchUpPresentation)
	{
		// No Controller-owned skippable chronology owns the delay. A detached
		// cosmetic tail alone must never trigger Skip; preserve the normal request.
		return UBattleHUDWidgetBase::SelectCard(RuntimeId);
	}

	FPendingFastCardCredential Credential;
	Credential.SessionToken = SessionToken;
	Credential.BattleId = TargetBattleId;
	Credential.ExpectedCatchUpRevision = ExpectedCatchUpRevision;
	if (!Credential.IsValid())
	{
		return UBattleHUDWidgetBase::SelectCard(RuntimeId);
	}

	PendingFastCardRuntimeId = RuntimeId;
	bFastCardSelectionRetryScheduled = true;
	GPendingFastCardCredentials.Add(this, Credential);

	// Ordinary catch-up keeps the same PresentationSessionToken. Controller Skip
	// synchronously collapses authoritative chronology; the next-tick retry still
	// has to prove that it reached the exact revision captured above.
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
	PruneDeadFastCardCredentials();

	// A later accepted EndTurn decisively retires this older future card click.
	// Consume the fence before reading/replaying its credential.
	if (BattleHUDWidgetG9BInput::ConsumeFastInputRetirementFence(this))
	{
		PendingFastCardRuntimeId = INDEX_NONE;
		bFastCardSelectionRetryScheduled = false;
		GPendingFastCardCredentials.Remove(this);
		return;
	}

	if (!bFastCardSelectionRetryScheduled)
	{
		return;
	}

	const int32 RuntimeId = PendingFastCardRuntimeId;
	PendingFastCardRuntimeId = INDEX_NONE;
	bFastCardSelectionRetryScheduled = false;

	FPendingFastCardCredential Credential;
	if (const FPendingFastCardCredential* Found = GPendingFastCardCredentials.Find(this))
	{
		Credential = *Found;
	}
	GPendingFastCardCredentials.Remove(this);

	if (RuntimeId == INDEX_NONE
		|| !Credential.IsValid()
		|| !IsValid(ViewModel)
		|| !IsValid(PresentationController))
	{
		return;
	}

	if (!PresentationController->IsCurrentPresentationSession(Credential.SessionToken)
		|| ViewModel->BattleId != Credential.BattleId
		|| ViewModel->StateRevision != Credential.ExpectedCatchUpRevision)
	{
		return;
	}

	if (ViewModel->HasAuthoritativePendingCardSelection())
	{
		// The click that fast-forwarded the preceding Presentation ends at the
		// interaction-state transition. A new pending selection is a different
		// authoritative surface and must consume a new physical input.
		return;
	}

	// G8-B retries only on the exact normal player-turn surface. A catch-up that
	// reaches a newer target-choice/confirm/terminal/revision boundary makes the
	// old physical click stale rather than replaying it into a different state.
	if (ViewModel->Outcome != EBattleHUDOutcome::None
		|| ViewModel->InteractionState != EBattleHUDInteractionState::Idle
		|| ViewModel->bInputLocked
		|| !IsDisplayedGameplayCard(ViewModel, RuntimeId))
	{
		return;
	}

	UBattleHUDWidgetBase::SelectCard(RuntimeId);
}
