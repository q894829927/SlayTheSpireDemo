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

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[BattleRelicTooltipDiag] MouseEnter Widget='%s' RelicId='%s' TooltipClass='%s' Visibility=%d Enabled=%d."),
		*GetPathName(),
		*NativeRelicView.RelicId.ToString(),
		*GetNameSafe(TooltipWidgetClass.Get()),
		static_cast<int32>(GetVisibility()),
		GetIsEnabled() ? 1 : 0);

	ShowRelicTooltip();
}

void UBattleRelicWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[BattleRelicTooltipDiag] MouseLeave Widget='%s' RelicId='%s' ActiveTooltip=%d."),
		*GetPathName(),
		*NativeRelicView.RelicId.ToString(),
		IsValid(ActiveTooltipWidget) ? 1 : 0);

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
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BattleRelicTooltipDiag] Create skipped: TooltipWidgetClass is null on Widget='%s'."),
			*GetPathName());
		return nullptr;
	}

	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		UBattleRelicTooltipWidget* Created =
			CreateWidget<UBattleRelicTooltipWidget>(OwningPlayer, TooltipWidgetClass);
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BattleRelicTooltipDiag] Create via OwningPlayer Widget='%s' Class='%s' Result='%s'."),
			*GetPathName(),
			*GetNameSafe(TooltipWidgetClass.Get()),
			*GetNameSafe(Created));
		return Created;
	}
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		UBattleRelicTooltipWidget* Created =
			CreateWidget<UBattleRelicTooltipWidget>(World, TooltipWidgetClass);
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BattleRelicTooltipDiag] Create via World Widget='%s' Class='%s' Result='%s'."),
			*GetPathName(),
			*GetNameSafe(TooltipWidgetClass.Get()),
			*GetNameSafe(Created));
		return Created;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[BattleRelicTooltipDiag] Create failed: no OwningPlayer and no valid World for Widget='%s'."),
		*GetPathName());
	return nullptr;
}

void UBattleRelicWidget::ShowRelicTooltip()
{
	if (IsValid(ActiveTooltipWidget))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BattleRelicTooltipDiag] Show skipped: tooltip already active Widget='%s' Tooltip='%s'."),
			*GetPathName(),
			*GetNameSafe(ActiveTooltipWidget));
		return;
	}

	if (TooltipWidgetClass == nullptr)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BattleRelicTooltipDiag] Show skipped: TooltipWidgetClass is null Widget='%s' RelicId='%s'."),
			*GetPathName(),
			*NativeRelicView.RelicId.ToString());
		return;
	}

	if (NativeRelicView.DisplayName.IsEmpty() && NativeRelicView.Description.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BattleRelicTooltipDiag] Show skipped: frozen DisplayName and Description are both empty Widget='%s' RelicId='%s'."),
			*GetPathName(),
			*NativeRelicView.RelicId.ToString());
		return;
	}

	UBattleRelicTooltipWidget* Tooltip = CreateRelicTooltipWidget();
	if (!IsValid(Tooltip))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BattleRelicTooltipDiag] Show failed: CreateRelicTooltipWidget returned invalid Widget='%s' RelicId='%s'."),
			*GetPathName(),
			*NativeRelicView.RelicId.ToString());
		return;
	}

	Tooltip->SetRelicView(NativeRelicView);
	Tooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
	Tooltip->SetAlignmentInViewport(FVector2D::ZeroVector);
	Tooltip->AddToViewport(TooltipZOrder);
	ActiveTooltipWidget = Tooltip;
	UpdateRelicTooltipPosition();

	const FVector2D DesiredSize = Tooltip->GetDesiredSize();
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[BattleRelicTooltipDiag] Show success Tooltip='%s' InViewport=%d Visibility=%d DesiredSize=(%.1f, %.1f) NameEmpty=%d DescriptionEmpty=%d."),
		*GetPathNameSafe(Tooltip),
		Tooltip->IsInViewport() ? 1 : 0,
		static_cast<int32>(Tooltip->GetVisibility()),
		DesiredSize.X,
		DesiredSize.Y,
		NativeRelicView.DisplayName.IsEmpty() ? 1 : 0,
		NativeRelicView.Description.IsEmpty() ? 1 : 0);
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
