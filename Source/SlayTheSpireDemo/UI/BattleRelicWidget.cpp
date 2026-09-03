#include "BattleRelicWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UBattleRelicWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	bNativeBindingsValid = IsValid(Img_RelicIcon)
		&& IsValid(Txt_RelicName)
		&& IsValid(Txt_RelicCounter);
	if (!ensureMsgf(
		bNativeBindingsValid,
		TEXT("Native Battle Relic '%s' is missing Img_RelicIcon, Txt_RelicName or Txt_RelicCounter."),
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

void UBattleRelicWidget::SetRelicView(const FBattleHUDRelicView& View)
{
	NativeRelicView = View;
	RefreshFromRelicView();
}

void UBattleRelicWidget::RefreshFromRelicView()
{
	if (!bNativeBindingsValid)
	{
		return;
	}

	Txt_RelicName->SetText(NativeRelicView.DisplayName);
	SetToolTipText(NativeRelicView.Description);

	if (IsValid(NativeRelicView.Icon))
	{
		Img_RelicIcon->SetBrushFromTexture(NativeRelicView.Icon, true);
		Img_RelicIcon->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		Img_RelicIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (NativeRelicView.bShowCounter)
	{
		Txt_RelicCounter->SetText(FText::Format(
			NSLOCTEXT("BattleRelicWidget", "RelicCounterFormat", "{0}/{1}"),
			FText::AsNumber(NativeRelicView.Counter),
			FText::AsNumber(NativeRelicView.CounterMax)));
		Txt_RelicCounter->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Txt_RelicCounter->SetText(FText::GetEmpty());
		Txt_RelicCounter->SetVisibility(ESlateVisibility::Collapsed);
	}
}
