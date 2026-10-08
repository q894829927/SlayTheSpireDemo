#include "Phase6UIA2NR8TestTypes.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "UI/BattleHandFanPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"

SLAYTHESPIREDEMO_API void CacheG9TestWidgetGeometry(UWidget*, const FGeometry&);

void UPhase6UIA2NR8CardProbe::NativeOnInitialized()
{
	if (!IsValid(Btn_Card))
	{
		TestButton = NewObject<UButton>(this);
		TestName = NewObject<UTextBlock>(this);
		TestCost = NewObject<UTextBlock>(this);
		TestDescription = NewObject<URichTextBlock>(this);
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
	// The panel retains each SObjectWidget wrapper before geometry is injected;
	// a standalone UserWidget TakeWidget temporary does not own its Slate tree.
	HB_Hand->TakeWidget();
	CacheG9TestWidgetGeometry(OV_PlayArea, FGeometry::MakeRoot(FVector2D(500,400), FSlateLayoutTransform()));
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

void UPhase6UIA2NR8HUDProbe::ConfigureFanForTesting(UBattleHandFanPanel* Fan)
{
	FanHand = Fan; HB_Hand = Fan;
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);
	WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
}

bool UPhase6UIA2NR8HUDProbe::NativePrepareDetachedCardDestination(const FPresentationRecord& Record,
	const FPlayedCardPresentationLifecycleToken& Life, FDetachedCardDestinationToken& OutToken)
{
	LastDetachedDestinationRecord = Record;
	if (bRejectDetachedPreparation) { OutToken = {}; return false; }
	const bool bPrepared = Super::NativePrepareDetachedCardDestination(Record,Life,OutToken);
	LastDetachedPreparation = OutToken;
	auto Hook = MoveTemp(DetachedPreparationHook); if (Hook) Hook();
	return bPrepared;
}

bool UPhase6UIA2NR8HUDProbe::NativeActivatePreparedDetachedCardDestination(const FDetachedCardDestinationToken& Token)
{
	if (bRejectDetachedActivation) return false;
	const bool bActivated = Super::NativeActivatePreparedDetachedCardDestination(Token);
	auto Hook = MoveTemp(DetachedActivationHook); if (Hook) Hook();
	return bActivated;
}
