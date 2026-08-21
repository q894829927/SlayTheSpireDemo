#include "BattleHUDWidgetBase.h"

#include "BattleHUDViewModel.h"
#include "../Presentation/BattlePresentationController.h"

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

void UBattleHUDWidgetBase::NotifyPresentationFinished(
	const FPresentationPlaybackToken& Token
)
{
	if (IsValid(PresentationController))
	{
		PresentationController->NotifyPresentationFinished(Token);
	}
}

void UBattleHUDWidgetBase::SkipPresentation()
{
	if (IsValid(PresentationController))
	{
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
	BP_OnViewModelChanged();
}
