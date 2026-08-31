#include "Phase6UIA2NR8TestTypes.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UPhase6UIA2NR8CardProbe::NativeOnInitialized()
{
	if (!IsValid(Btn_Card))
	{
		TestButton = NewObject<UButton>(this);
		TestName = NewObject<UTextBlock>(this);
		TestCost = NewObject<UTextBlock>(this);
		TestDescription = NewObject<UTextBlock>(this);
		TestType = NewObject<UTextBlock>(this);
		TestArt = NewObject<UImage>(this);
		Btn_Card = TestButton;
		Txt_CardName = TestName;
		Txt_Cost = TestCost;
		Txt_CardDescription = TestDescription;
		Txt_CardType = TestType;
		Img_CardArt = TestArt;
	}

	Super::NativeOnInitialized();
}

void UPhase6UIA2NR8HUDProbe::SetTestWorld(UWorld* InWorld)
{
	TestWorld = InWorld;
}

void UPhase6UIA2NR8HUDProbe::ConfigureCardSurfaces(
	UHorizontalBox* InHand,
	UOverlay* InPlayArea,
	UTextBlock* InDrawCount,
	UTextBlock* InDiscardCount,
	UTextBlock* InEnergy)
{
	HB_Hand = InHand;
	OV_PlayArea = InPlayArea;
	Txt_DrawCount = InDrawCount;
	Txt_DiscardCount = InDiscardCount;
	Txt_Energy = InEnergy;
	CardWidgetClass = UPhase6UIA2NR8CardProbe::StaticClass();
}

UWorld* UPhase6UIA2NR8HUDProbe::GetWorld() const
{
	return TestWorld.Get();
}

bool UPhase6UIA2NR8HUDProbe::InvokeBeginDirectForTesting(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	return BeginPresentationRecordPlayback_Implementation(Record, Token);
}

void UPhase6UIA2NR8HUDProbe::InvokeFinishForTesting(
	const FPresentationPlaybackToken& Token)
{
	FinishNativePresentation(Token);
}

void UPhase6UIA2NR8HUDProbe::InvokeCancelForTesting(
	const FPresentationPlaybackToken& Token)
{
	CancelPresentationRecordPlayback(Token);
}

void UPhase6UIA2NR8HUDProbe::InvokeNativeTickForTesting(float DeltaSeconds)
{
	NativeTick(GetCachedGeometry(), DeltaSeconds);
}

void UPhase6UIA2NR8HUDProbe::InvokeNativeDestructForTesting()
{
	NativeDestruct();
}
