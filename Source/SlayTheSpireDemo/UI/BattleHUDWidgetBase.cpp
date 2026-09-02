#include "BattleHUDWidgetBase.h"

#include "BattleHUDViewModel.h"
#include "../Presentation/BattlePresentationController.h"
#include "Containers/Ticker.h"

void UBattleHUDWidgetBase::SetViewModel(UBattleHUDViewModel* InViewModel)
{
	if (ViewModel == InViewModel)
	{
		HandleViewModelChanged();
		return;
	}

	if (IsValid(ViewModel))
	{
		ViewModel->OnChanged.RemoveDynamic(this, &UBattleHUDWidgetBase::HandleViewModelChanged);
	}

	ViewModel = InViewModel;
	if (IsValid(ViewModel))
	{
		ViewModel->OnChanged.AddDynamic(this, &UBattleHUDWidgetBase::HandleViewModelChanged);
	}

	HandleViewModelChanged();
}

void UBattleHUDWidgetBase::SetPresentationController(
	UBattlePresentationController* InController
)
{
	PresentationController = InController;
}

bool UBattleHUDWidgetBase::SelectCard(int32 RuntimeId)
{
	return IsValid(ViewModel) && ViewModel->SelectCardByRuntimeId(RuntimeId);
}

void UBattleHUDWidgetBase::CancelSelection()
{
	if (IsValid(ViewModel))
	{
		ViewModel->CancelSelection();
	}
}

bool UBattleHUDWidgetBase::SelectTarget(int32 TargetId)
{
	if (!IsValid(ViewModel))
	{
		return false;
	}

	// A3 pre-commit ownership ends before the authoritative request is entered.
	// This broadcast synchronously removes the Native Preview surface; Gameplay
	// then revalidates the target through the unchanged SelectTargetById path.
	ViewModel->ClearPreviewTarget();
	return ViewModel->SelectTargetById(TargetId);
}

bool UBattleHUDWidgetBase::ConfirmSelectedCard()
{
	return IsValid(ViewModel) && ViewModel->ConfirmSelectedCard();
}

bool UBattleHUDWidgetBase::EndTurn()
{
	return IsValid(ViewModel) && ViewModel->RequestEndTurn();
}

FBattleHUDCardView UBattleHUDWidgetBase::MakePresentationCardView(
	const FPresentationCardSnapshot& Snapshot
) const
{
	FBattleHUDCardView View;
	View.RuntimeId = Snapshot.RuntimeId;
	View.CardId = Snapshot.CardId;
	View.DisplayName = Snapshot.DisplayName;
	View.Cost = Snapshot.Cost;
	View.CardType = Snapshot.CardType;
	View.TargetType = Snapshot.TargetType;
	View.Description = Snapshot.Description;
	View.CardArt = Snapshot.CardArt;
	View.bGameplayPlayable = false;
	View.UnplayableReason = FText::GetEmpty();
	return View;
}

FBattleHUDStatusView UBattleHUDWidgetBase::MakePresentationStatusView(
	const FStatusChangedPresentationPayload& StatusChanged
) const
{
	FBattleHUDStatusView View;

	View.StatusId = StatusChanged.StatusId;
	View.RuntimeSequence = StatusChanged.RuntimeSequence;

	View.DisplayName = StatusChanged.DisplayName;
	View.Description = StatusChanged.DescriptionAfter;
	View.Amount = StatusChanged.AmountAfter;

	View.bUseAtlasIcon = StatusChanged.bUseAtlasIcon;
	View.UVOffset = StatusChanged.UVOffset;
	View.UVScale = StatusChanged.UVScale;
	View.TrimOffset = StatusChanged.TrimOffset;
	View.TrimScale = StatusChanged.TrimScale;

	return View;
}

bool UBattleHUDWidgetBase::PlayPresentationRecord(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token
)
{
	// Controller guarantees one active Record at a time. Cancel defensively if a
	// replacement visual is offered anyway, then establish ownership before
	// entering Blueprint so even a synchronous misuse is associated with Token.
	CancelTrackedPresentationPlayback();
	TrackedPresentationPlaybackToken = Token;
	bHasTrackedPresentationPlayback = true;

	const bool bAccepted = BeginPresentationRecordPlayback(Record, Token);
	if (!bAccepted)
	{
		ClearTrackedPresentationPlayback(Token);
	}
	return bAccepted;
}

bool UBattleHUDWidgetBase::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& /*Record*/,
	const FPresentationPlaybackToken& /*Token*/
)
{
	return false;
}

void UBattleHUDWidgetBase::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& /*Token*/
)
{
}

void UBattleHUDWidgetBase::NotifyPresentationFinished(
	const FPresentationPlaybackToken& Token
)
{
	// Blueprint is required to complete asynchronously, but enforce that boundary
	// here as well. A Blueprint that accidentally calls this from inside the
	// playback event therefore cannot re-enter the Controller while it is still
	// offering the current Record.
	const TWeakObjectPtr<UBattleHUDWidgetBase> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis, Token](float /*DeltaTime*/)
			{
				if (UBattleHUDWidgetBase* Widget = WeakThis.Get())
				{
					Widget->ForwardPresentationFinished(Token);
				}
				return false;
			}
		),
		0.0f
	);
}

void UBattleHUDWidgetBase::ForwardPresentationFinished(
	const FPresentationPlaybackToken& Token
)
{
	// A stale completion must never clear a newer visual token.
	ClearTrackedPresentationPlayback(Token);

	if (!IsValid(PresentationController))
	{
		return;
	}

	// Controller completion synchronously advances the historical ViewModel.
	// That ViewModel change is the normal completion path, not a reason to ask
	// Blueprint to cancel the visual that has just finished.
	TGuardValue<bool> SuppressCancellation(bSuppressPresentationCancellation, true);
	PresentationController->NotifyPresentationFinished(Token);
}

void UBattleHUDWidgetBase::SkipPresentation()
{
	// Stop presentation-only visuals before Controller collapses to the newest
	// frozen FinalSnapshot. Stale callbacks remain harmless through token checks.
	CancelTrackedPresentationPlayback();

	if (IsValid(PresentationController))
	{
		TGuardValue<bool> SuppressCancellation(bSuppressPresentationCancellation, true);
		PresentationController->SkipPresentation();
	}
}

void UBattleHUDWidgetBase::NativeDestruct()
{
	// No Blueprint cancellation event is dispatched during destruction. The Widget
	// is already leaving the tree, while Controller NotifyWidgetLost provides the
	// authoritative playback catch-up/fail-safe behavior.
	bHasTrackedPresentationPlayback = false;
	TrackedPresentationPlaybackToken = FPresentationPlaybackToken{};

	// Stop observing ViewModel changes before NotifyWidgetLost. That controller
	// callback may synchronously SkipPresentation/collapse to a newer historical
	// snapshot and broadcast OnChanged; a Widget already leaving the tree must not
	// redraw Hand/Status/other UMG children during that teardown catch-up.
	if (IsValid(ViewModel))
	{
		ViewModel->OnChanged.RemoveDynamic(this, &UBattleHUDWidgetBase::HandleViewModelChanged);
	}

	if (IsValid(PresentationController))
	{
		PresentationController->NotifyWidgetLost(this);
	}
	PresentationController = nullptr;

	Super::NativeDestruct();
}

void UBattleHUDWidgetBase::HandleViewModelChanged()
{
	// During Controller-owned playback, the ViewModel advances only after a Record
	// completes or after a fail-safe collapse/timeout/unavailable transition. If
	// the change did not originate from normal completion or explicit Skip, a
	// tracked Blueprint visual belongs to abandoned historical work and must stop
	// before the HUD redraws the new frozen state.
	if (!bSuppressPresentationCancellation)
	{
		CancelTrackedPresentationPlayback();
	}

	NativeOnBattleHUDViewModelChanged();
}

void UBattleHUDWidgetBase::NativeOnBattleHUDViewModelChanged()
{
	// Preserve the sealed Legacy Blueprint contract by default. Native concrete
	// HUD classes may override this hook and intentionally omit Super to own refresh.
	BP_OnViewModelChanged();
}

void UBattleHUDWidgetBase::CancelTrackedPresentationPlayback()
{
	if (!bHasTrackedPresentationPlayback)
	{
		return;
	}

	const FPresentationPlaybackToken CancelledToken = TrackedPresentationPlaybackToken;
	bHasTrackedPresentationPlayback = false;
	TrackedPresentationPlaybackToken = FPresentationPlaybackToken{};

	// Clear ownership before entering Blueprint. A miswired cancellation callback
	// that later calls NotifyPresentationFinished therefore remains stale and cannot
	// erase a newer visual token established by another Record.
	CancelPresentationRecordPlayback(CancelledToken);
}

void UBattleHUDWidgetBase::ClearTrackedPresentationPlayback(
	const FPresentationPlaybackToken& Token
)
{
	if (bHasTrackedPresentationPlayback
		&& TrackedPresentationPlaybackToken == Token)
	{
		bHasTrackedPresentationPlayback = false;
		TrackedPresentationPlaybackToken = FPresentationPlaybackToken{};
	}
}
