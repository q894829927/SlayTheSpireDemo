#include "BattleRelicWidget.h"

#include "BattleRelicTooltipWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UBattleRelicWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	bNativeBindingsValid = IsValid(Img_RelicIcon)
		&& IsValid(Txt_RelicCounter);
	if (!ensureMsgf(
		bNativeBindingsValid,
		TEXT("Native Battle Relic '%s' is missing Img_RelicIcon or Txt_RelicCounter."),
		*GetName()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[BattleRelic][Native] Invalid required bindings on '%s'."),
			*GetPathName());
	}
}

void UBattleRelicWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromRelicView();
}

void UBattleRelicWidget::NativeDestruct()
{
	HideRelicTooltip();
	Super::NativeDestruct();
}

void UBattleRelicWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateRelicTooltipPosition();
}

void UBattleRelicWidget::NativeOnMouseEnter(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	ShowRelicTooltip();
}

void UBattleRelicWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	HideRelicTooltip();
	Super::NativeOnMouseLeave(InMouseEvent);
}

void UBattleRelicWidget::SetRelicView(const FBattleHUDRelicView& View)
{
	NativeRelicView = View;
	RefreshFromRelicView();

	if (IsValid(ActiveTooltipWidget))
	{
		ActiveTooltipWidget->SetRelicView(NativeRelicView);
	}
}

void UBattleRelicWidget::RefreshFromRelicView()
{
	if (!bNativeBindingsValid)
	{
		return;
	}

	if (IsValid(NativeRelicView.Icon))
	{
		Img_RelicIcon->SetBrushFromTexture(NativeRelicView.Icon, true);
		Img_RelicIcon->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		Img_RelicIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Slay-the-Spire-style counter badge: data decides whether a badge exists,
	// and the badge shows only current progress (0, 1, 2, ...), never "/Max".
	if (NativeRelicView.bShowCounter)
	{
		Txt_RelicCounter->SetText(FText::AsNumber(NativeRelicView.Counter));
		Txt_RelicCounter->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Txt_RelicCounter->SetText(FText::GetEmpty());
		Txt_RelicCounter->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UBattleRelicTooltipWidget* UBattleRelicWidget::CreateRelicTooltipWidget() const
{
	if (TooltipWidgetClass == nullptr)
	{
		return nullptr;
	}

	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		return CreateWidget<UBattleRelicTooltipWidget>(OwningPlayer, TooltipWidgetClass);
	}
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		return CreateWidget<UBattleRelicTooltipWidget>(World, TooltipWidgetClass);
	}
	return nullptr;
}

void UBattleRelicWidget::ShowRelicTooltip()
{
	if (IsValid(ActiveTooltipWidget)
		|| TooltipWidgetClass == nullptr
		|| (NativeRelicView.DisplayName.IsEmpty() && NativeRelicView.Description.IsEmpty()))
	{
		return;
	}

	UBattleRelicTooltipWidget* Tooltip = CreateRelicTooltipWidget();
	if (!IsValid(Tooltip))
	{
		return;
	}

	Tooltip->SetRelicView(NativeRelicView);
	Tooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
	Tooltip->SetAlignmentInViewport(FVector2D::ZeroVector);
	Tooltip->AddToViewport(TooltipZOrder);
	ActiveTooltipWidget = Tooltip;
	UpdateRelicTooltipPosition();
}

void UBattleRelicWidget::HideRelicTooltip()
{
	if (IsValid(ActiveTooltipWidget))
	{
		ActiveTooltipWidget->RemoveFromParent();
	}
	ActiveTooltipWidget = nullptr;
}

void UBattleRelicWidget::UpdateRelicTooltipPosition()
{
	if (!IsValid(ActiveTooltipWidget))
	{
		return;
	}

	// GetMousePositionOnViewport and SetPositionInViewport(..., false) use the
	// same viewport-local/DPI-adjusted coordinate space, avoiding an extra DPI
	// transform while the tooltip follows the cursor.
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(this);
	ActiveTooltipWidget->SetPositionInViewport(MousePosition + TooltipCursorOffset, false);
}
