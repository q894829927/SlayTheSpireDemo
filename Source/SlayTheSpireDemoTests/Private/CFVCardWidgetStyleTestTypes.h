#pragma once

#include "CoreMinimal.h"
#include "UI/BattleCardWidget.h"
#include "CFVCardWidgetStyleTestTypes.generated.h"

class UButton;
class UCardFaceStyleSet;
class UImage;
class URichTextBlock;
class UTextBlock;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UCFVCardWidgetStyleProbe : public UBattleCardWidget
{
	GENERATED_BODY()

public:
	void ConfigureCore(
		UButton* Button,
		UTextBlock* Name,
		UTextBlock* Cost,
		URichTextBlock* Description,
		UTextBlock* Type,
		UImage* Art)
	{
		Btn_Card = Button;
		Txt_CardName = Name;
		Txt_Cost = Cost;
		Txt_CardDescription = Description;
		Txt_CardType = Type;
		Img_CardArt = Art;
	}

	void ConfigureDecorative(
		UImage* Background,
		UImage* Frame,
		UImage* Banner,
		UImage* TypeLeft,
		UImage* TypeCenter,
		UImage* TypeRight,
		UImage* CostOrb)
	{
		Img_CardBackground = Background;
		Img_CardFrame = Frame;
		Img_CardBanner = Banner;
		Img_TypeLeft = TypeLeft;
		Img_TypeCenter = TypeCenter;
		Img_TypeRight = TypeRight;
		Img_CostOrb = CostOrb;
	}

	void SetStyleSetForTesting(UCardFaceStyleSet* StyleSet)
	{
		CardFaceStyleSet = StyleSet;
	}
};
