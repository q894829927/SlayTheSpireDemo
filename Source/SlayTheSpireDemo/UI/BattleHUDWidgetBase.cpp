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
	return IsValid(ViewModel) && ViewModel->SelectTargetById(TargetId);
}

bool UBattleHUDWidgetBase::ConfirmSelectedCard()
{
	return IsValid(ViewModel) && ViewModel->ConfirmSelectedCard();
}

bool UBattleHUDWidgetBase::EndTurn()
{
	return IsValid(ViewModel) && ViewModel->RequestEndTurn();
}

bool UBattleHUDWidgetBase::PlayPresentationRecord_Implementation(
	const FPresentationRecord& /*Record*/,
	const FPresentationPlaybackToken& /*Token*/
)
{
	return false;
}

void UBattleHUDWidgetBase::CancelPresentationRecordPlayback_Implementation()
{
}

void UBattleHUDWidgetBase::NotifyPresentationFinished(
	const FPresentationPlaybackToken& Token
)
{
	// Blueprint is required to complete asynchronously, but enforce that boundary
	// here as well. A Blueprint that accidentally calls this from inside
	// PlayPresentationRecord therefore cannot re-enter the Controller while it is
	// still offering the current Record.
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
	CancelPresentationRecordPlayback();

	if (IsValid(PresentationController))
	{
		TGuardValue<bool> SuppressCancellation(bSuppressPresentationCancellation, true);
		PresentationController->SkipPresentation();
	}
}

void UBattleHUDWidgetBase::NativeDestruct()
{
	if (IsValid(PresentationController))
	{
		PresentationController->NotifyWidgetLost(this);
	}
	PresentationController = nullptr;

	if (IsValid(ViewModel))
	{
		ViewModel->OnChanged.RemoveDynamic(this, &UBattleHUDWidgetBase::HandleViewModelChanged);
	}
	Super::NativeDestruct();
}

void UBattleHUDWidgetBase::HandleViewModelChanged()
{
	// During Controller-owned playback, the ViewModel advances only after a Record
	// completes or after a fail-safe collapse/timeout/unavailable transition. If
	// the change did not originate from normal completion or explicit Skip, any
	// still-running Blueprint visual is stale and must be stopped before the HUD
	// redraws the new frozen historical state.
	if (!bSuppressPresentationCancellation)
	{
		CancelPresentationRecordPlayback();
	}

	BP_OnViewModelChanged();
}
