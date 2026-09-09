#include "BattleHUDReconciledWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "Components/HorizontalBox.h"

void UBattleHUDReconciledWidget::NativeDestruct()
{
	if (UBattleHUDViewModel* BoundViewModel = OwnershipBoundViewModel.Get())
	{
		BoundViewModel->OnCardPresentationOwnershipChanged.RemoveAll(this);
	}
	OwnershipBoundViewModel.Reset();
	UnbindReconciledHandDelegates();
	ReconciledHandBattleId = 0;
	Super::NativeDestruct();
}

void UBattleHUDReconciledWidget::NativeOnBattleHUDViewModelChanged()
{
	EnsureOwnershipDelegateBinding();
	if (!IsValid(ViewModel))
	{
		return;
	}

	const EBattleHUDDirtyFlags DirtyFlags = GetCurrentNativeViewModelDirtyFlags();
	if (DirtyFlags == EBattleHUDDirtyFlags::All)
	{
		RefreshHUDFromViewModel();
		return;
	}

	if (EnumHasAnyFlags(DirtyFlags, EBattleHUDDirtyFlags::Hand))
	{
		RefreshHand();
	}
	if (EnumHasAnyFlags(
		DirtyFlags,
		EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Input))
	{
		RefreshCombatants();
	}
	if (EnumHasAnyFlags(DirtyFlags, EBattleHUDDirtyFlags::Statuses))
	{
		RefreshStatusRows();
	}
	if (EnumHasAnyFlags(DirtyFlags, EBattleHUDDirtyFlags::Energy))
	{
		RefreshEnergy();
	}
	if (EnumHasAnyFlags(DirtyFlags, EBattleHUDDirtyFlags::PileCounts))
	{
		RefreshPileCounts();
	}
	if (EnumHasAnyFlags(DirtyFlags, EBattleHUDDirtyFlags::Input))
	{
		RefreshInputState();
	}
	if (EnumHasAnyFlags(DirtyFlags, EBattleHUDDirtyFlags::Feedback))
	{
		RefreshFeedback();
	}
	if (EnumHasAnyFlags(DirtyFlags, EBattleHUDDirtyFlags::Intent))
	{
		RefreshEnemyIntent();
	}
	if (EnumHasAnyFlags(DirtyFlags, EBattleHUDDirtyFlags::Terminal))
	{
		RefreshTerminalFromViewModel();
	}
	if (EnumHasAnyFlags(DirtyFlags, EBattleHUDDirtyFlags::PresentationAvailability))
	{
		RefreshPresentationAvailabilityFromViewModel();
	}
}

void UBattleHUDReconciledWidget::RefreshHand()
{
	if (!IsValid(ViewModel) || !IsValid(HB_Hand) || CardWidgetClass == nullptr)
	{
		return;
	}

	if (ReconciledHandBattleId != 0 && ReconciledHandBattleId != ViewModel->BattleId)
	{
		UnbindReconciledHandDelegates();
		HB_Hand->ClearChildren();
	}
	ReconciledHandBattleId = ViewModel->BattleId;

	TSet<int32> DesiredRuntimeIds;
	DesiredRuntimeIds.Reserve(ViewModel->HandCards.Num());
	for (const FBattleHUDCardView& CardView : ViewModel->HandCards)
	{
		if (CardView.RuntimeId == INDEX_NONE || DesiredRuntimeIds.Contains(CardView.RuntimeId))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[BattleHUD][G0-B] Hand reconcile rejected invalid/duplicate RuntimeId %d."),
				CardView.RuntimeId);
			Super::RefreshHand();
			ApplyExplicitCardPresentationOwnershipToFormalHand();
			return;
		}
		DesiredRuntimeIds.Add(CardView.RuntimeId);
	}

	TMap<int32, UBattleCardWidget*> ExistingByRuntimeId;
	for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
	{
		UBattleCardWidget* Existing = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index));
		if (!IsValid(Existing)
			|| Existing->GetRuntimeId() == INDEX_NONE
			|| ExistingByRuntimeId.Contains(Existing->GetRuntimeId()))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[BattleHUD][G0-B] Hand reconcile found malformed formal Hand children."));
			Super::RefreshHand();
			ApplyExplicitCardPresentationOwnershipToFormalHand();
			return;
		}
		ExistingByRuntimeId.Add(Existing->GetRuntimeId(), Existing);
	}

	TArray<UBattleCardWidget*> DesiredWidgets;
	DesiredWidgets.Reserve(ViewModel->HandCards.Num());
	for (const FBattleHUDCardView& CardView : ViewModel->HandCards)
	{
		UBattleCardWidget* CardWidget = nullptr;
		if (UBattleCardWidget** Existing = ExistingByRuntimeId.Find(CardView.RuntimeId))
		{
			CardWidget = *Existing;
		}
		else
		{
			CardWidget = CreateWidget<UBattleCardWidget>(GetOwningPlayer(), CardWidgetClass);
		}
		if (!IsValid(CardWidget))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[BattleHUD][G0-B] Hand reconcile could not create RuntimeId %d (%s)."),
				CardView.RuntimeId,
				*CardView.CardId.ToString());
			Super::RefreshHand();
			ApplyExplicitCardPresentationOwnershipToFormalHand();
			return;
		}
		DesiredWidgets.Add(CardWidget);
	}

	UnbindReconciledHandDelegates();
	HB_Hand->ClearChildren();

	UBattleHUDWidget* BaseHUD = static_cast<UBattleHUDWidget*>(this);
	for (int32 Index = 0; Index < ViewModel->HandCards.Num(); ++Index)
	{
		UBattleCardWidget* CardWidget = DesiredWidgets[Index];
		const FBattleHUDCardView& CardView = ViewModel->HandCards[Index];
		CardWidget->SetCardView(CardView);
		CardWidget->OnBattleCardRequested.AddUniqueDynamic(
			BaseHUD,
			&UBattleHUDWidget::HandleCardRequested);
		HB_Hand->AddChildToHorizontalBox(CardWidget);
	}

	ApplyExplicitCardPresentationOwnershipToFormalHand();
}

void UBattleHUDReconciledWidget::EnsureOwnershipDelegateBinding()
{
	UBattleHUDViewModel* DesiredViewModel = IsValid(ViewModel) ? ViewModel.Get() : nullptr;
	if (OwnershipBoundViewModel.Get() == DesiredViewModel)
	{
		return;
	}

	if (UBattleHUDViewModel* PreviousViewModel = OwnershipBoundViewModel.Get())
	{
		PreviousViewModel->OnCardPresentationOwnershipChanged.RemoveAll(this);
	}
	OwnershipBoundViewModel = DesiredViewModel;
	if (DesiredViewModel != nullptr)
	{
		DesiredViewModel->OnCardPresentationOwnershipChanged.AddUObject(
			this,
			&UBattleHUDReconciledWidget::HandleCardPresentationOwnershipChanged);
	}
}

void UBattleHUDReconciledWidget::HandleCardPresentationOwnershipChanged(
	const TArray<int32>& RuntimeIds)
{
	if (!IsValid(ViewModel) || !IsValid(HB_Hand))
	{
		return;
	}

	for (const int32 RuntimeId : RuntimeIds)
	{
		for (UWidget* Child : HB_Hand->GetAllChildren())
		{
			UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(Child);
			if (!IsValid(CardWidget) || CardWidget->GetRuntimeId() != RuntimeId)
			{
				continue;
			}

			FCardPresentationOwnershipEntry Entry;
			const bool bExplicitNonHandOwner =
				ViewModel->TryGetCardPresentationOwnershipEntry(RuntimeId, Entry)
				&& Entry.Owner != ECardPresentationOwner::Hand;
			CardWidget->SetVisibility(
				bExplicitNonHandOwner
					? ESlateVisibility::Hidden
					: ESlateVisibility::Visible);
			CardWidget->SetIsEnabled(!bExplicitNonHandOwner);
			break;
		}
	}
}

void UBattleHUDReconciledWidget::ApplyExplicitCardPresentationOwnershipToFormalHand()
{
	if (!IsValid(ViewModel) || !IsValid(HB_Hand))
	{
		return;
	}

	for (UWidget* Child : HB_Hand->GetAllChildren())
	{
		UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(Child);
		if (!IsValid(CardWidget))
		{
			continue;
		}

		FCardPresentationOwnershipEntry Entry;
		if (ViewModel->TryGetCardPresentationOwnershipEntry(CardWidget->GetRuntimeId(), Entry)
			&& Entry.Owner != ECardPresentationOwner::Hand)
		{
			CardWidget->SetVisibility(ESlateVisibility::Hidden);
			CardWidget->SetIsEnabled(false);
		}
	}
}

void UBattleHUDReconciledWidget::UnbindReconciledHandDelegates()
{
	if (!IsValid(HB_Hand))
	{
		return;
	}

	UBattleHUDWidget* BaseHUD = static_cast<UBattleHUDWidget*>(this);
	for (UWidget* Child : HB_Hand->GetAllChildren())
	{
		if (UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(Child))
		{
			CardWidget->OnBattleCardRequested.RemoveDynamic(
				BaseHUD,
				&UBattleHUDWidget::HandleCardRequested);
		}
	}
}
