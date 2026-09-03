#include "BattleRelicTooltipWidget.h"

#include "Components/TextBlock.h"

void UBattleRelicTooltipWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	bNativeBindingsValid = IsValid(Txt_RelicName)
		&& IsValid(Txt_RelicDescription);
	if (!ensureMsgf(
		bNativeBindingsValid,
		TEXT("Native Battle Relic Tooltip '%s' is missing Txt_RelicName or Txt_RelicDescription."),
		*GetName()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[BattleRelicTooltip][Native] Invalid required bindings on '%s'."),
			*GetPathName());
	}
}

void UBattleRelicTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshFromRelicView();
}

void UBattleRelicTooltipWidget::SetRelicView(const FBattleHUDRelicView& View)
{
	NativeRelicView = View;
	RefreshFromRelicView();
}

void UBattleRelicTooltipWidget::RefreshFromRelicView()
{
	if (!bNativeBindingsValid)
	{
		return;
	}

	Txt_RelicName->SetText(NativeRelicView.DisplayName);
	Txt_RelicDescription->SetText(NativeRelicView.Description);
}
