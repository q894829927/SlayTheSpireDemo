#include "Phase6UIA2NR4TestTypes.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"

void UPhase6UIA2NR4CardProbe::ConfigureSurfaces(
	UButton* InButton,
	UTextBlock* InName,
	UTextBlock* InCost,
	URichTextBlock* InDescription,
	UTextBlock* InType,
	UImage* InArt)
{
	Btn_Card = InButton;
	Txt_CardName = InName;
	Txt_Cost = InCost;
	Txt_CardDescription = InDescription;
	Txt_CardType = InType;
	Img_CardArt = InArt;
}

void UPhase6UIA2NR4CardProbe::InvokeCardClickForTesting()
{
	HandleCardClicked();
}

void UPhase6UIA2NR4RequestSink::HandleCardRequested(int32 RuntimeId)
{
	++CallCount;
	LastRuntimeId = RuntimeId;
}
