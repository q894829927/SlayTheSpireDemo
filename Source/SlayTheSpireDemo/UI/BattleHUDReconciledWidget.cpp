#include "BattleHUDReconciledWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "Components/HorizontalBox.h"

void UBattleHUDReconciledWidget::NativeDestruct()
{
	if (UBattleHUDViewModel* BoundViewModel = OwnershipBoundViewModel.Get())
	{
		if (BoundViewModel->SynchronizeCardPresentationSurfaces.IsBoundToObject(this)) BoundViewModel->SynchronizeCardPresentationSurfaces.Unbind();
	}
	OwnershipBoundViewModel.Reset();

	Super::NativeDestruct();
}

void UBattleHUDReconciledWidget::BeforeNativeHUDRefresh(EBattleHUDDirtyFlags DirtyFlags)
{
	Super::BeforeNativeHUDRefresh(DirtyFlags);
	EnsureOwnershipDelegateBinding();
}

void UBattleHUDReconciledWidget::AfterFormalHandCommit()
{
	Super::AfterFormalHandCommit();
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
		if (PreviousViewModel->SynchronizeCardPresentationSurfaces.IsBoundToObject(this)) PreviousViewModel->SynchronizeCardPresentationSurfaces.Unbind();
	}
	OwnershipBoundViewModel = DesiredViewModel;
	if (DesiredViewModel != nullptr)
	{
		DesiredViewModel->SynchronizeCardPresentationSurfaces.BindUObject(this, &UBattleHUDReconciledWidget::HandleCardPresentationOwnershipChanged);
	}
}

void UBattleHUDReconciledWidget::HandleCardPresentationOwnershipChanged(const TArray<int32>& RuntimeIds)
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
			const bool bExplicitNonHandOwner = IsFormalHandVisualSuppressed(RuntimeId)
				|| (ViewModel->TryGetCardPresentationOwnershipEntry(RuntimeId, Entry) && Entry.Owner != ECardPresentationOwner::Hand);
			CardWidget->SetVisibility(bExplicitNonHandOwner ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
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
		if (IsFormalHandVisualSuppressed(CardWidget->GetRuntimeId())
			|| (ViewModel->TryGetCardPresentationOwnershipEntry(CardWidget->GetRuntimeId(), Entry) && Entry.Owner != ECardPresentationOwner::Hand))
		{
			CardWidget->SetVisibility(ESlateVisibility::Hidden);
			CardWidget->SetIsEnabled(false);
		}
	}
}
