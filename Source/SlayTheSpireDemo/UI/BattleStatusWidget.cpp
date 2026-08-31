#include "BattleStatusWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

void UBattleStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	bNativeBindingsValid = IsValid(Img_StatusIcon) && IsValid(Txt_StatusAmount);
	if (!ensureMsgf(
		bNativeBindingsValid,
		TEXT("Native Battle Status '%s' is missing Img_StatusIcon or Txt_StatusAmount."),
		*GetName()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[BattleStatus][Native] Invalid required bindings on '%s'."),
			*GetPathName());
	}
}

void UBattleStatusWidget::SetStatusView(const FBattleHUDStatusView& View)
{
	NativeStatusView = View;

	if (!bNativeBindingsValid)
	{
		return;
	}

	Txt_StatusAmount->SetText(FText::AsNumber(View.Amount));

	if (!View.bUseAtlasIcon)
	{
		Img_StatusIcon->SetVisibility(ESlateVisibility::Collapsed);
		NativeStatusIconMID = nullptr;
		return;
	}

	Img_StatusIcon->SetVisibility(ESlateVisibility::Visible);
	NativeStatusIconMID = Img_StatusIcon->GetDynamicMaterial();
	if (!IsValid(NativeStatusIconMID))
	{
		return;
	}

	SetAtlasVector2D(TEXT("UVOffset"), View.UVOffset);
	SetAtlasVector2D(TEXT("UVScale"), View.UVScale);
	SetAtlasVector2D(TEXT("TrimOffset"), View.TrimOffset);
	SetAtlasVector2D(TEXT("TrimScale"), View.TrimScale);
}

void UBattleStatusWidget::SetAtlasVector2D(FName ParameterName, const FVector2D& Value)
{
	if (IsValid(NativeStatusIconMID))
	{
		NativeStatusIconMID->SetVectorParameterValue(
			ParameterName,
			FLinearColor(Value.X, Value.Y, 0.0f, 0.0f));
	}
}
