#include "BattleImmediatePreviewTextBlock.h"

#include "BattleHUDViewModel.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

void UBattleImmediatePreviewTextBlock::AttachToPreview(
	UBattleHUDViewModel* InViewModel,
	UOverlay* InHostOverlay
)
{
	if (BoundViewModel.Get() != InViewModel)
	{
		if (UBattleHUDViewModel* PreviousViewModel = BoundViewModel.Get())
		{
			PreviousViewModel->OnPreviewChanged.RemoveDynamic(
				this,
				&UBattleImmediatePreviewTextBlock::HandlePreviewChanged);
		}

		BoundViewModel = InViewModel;
		if (IsValid(InViewModel))
		{
			InViewModel->OnPreviewChanged.AddUniqueDynamic(
				this,
				&UBattleImmediatePreviewTextBlock::HandlePreviewChanged);
		}
	}

	HostOverlay = InHostOverlay;
	HandlePreviewChanged();
}

void UBattleImmediatePreviewTextBlock::DetachFromPreview()
{
	if (UBattleHUDViewModel* ViewModel = BoundViewModel.Get())
	{
		ViewModel->OnPreviewChanged.RemoveDynamic(
			this,
			&UBattleImmediatePreviewTextBlock::HandlePreviewChanged);
	}

	BoundViewModel.Reset();
	HostOverlay.Reset();
	SetText(FText::GetEmpty());
	RemoveFromParent();
}

void UBattleImmediatePreviewTextBlock::BeginDestroy()
{
	DetachFromPreview();
	Super::BeginDestroy();
}

void UBattleImmediatePreviewTextBlock::HandlePreviewChanged()
{
	UBattleHUDViewModel* ViewModel = BoundViewModel.Get();
	UOverlay* Overlay = HostOverlay.Get();
	if (!IsValid(ViewModel) || !IsValid(Overlay))
	{
		SetText(FText::GetEmpty());
		RemoveFromParent();
		return;
	}

	const FText PreviewText = ViewModel->GetImmediatePreviewDisplayText();
	if (PreviewText.IsEmpty())
	{
		SetText(FText::GetEmpty());
		RemoveFromParent();
		return;
	}

	SetText(PreviewText);
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (GetParent() != Overlay)
	{
		RemoveFromParent();
		if (UOverlaySlot* PreviewOverlaySlot = Overlay->AddChildToOverlay(this))
		{
			PreviewOverlaySlot->SetHorizontalAlignment(HAlign_Center);
			PreviewOverlaySlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}
