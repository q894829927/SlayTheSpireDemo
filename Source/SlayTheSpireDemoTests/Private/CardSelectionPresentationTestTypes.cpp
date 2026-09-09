#include "CardSelectionPresentationTestTypes.h"

#include "Phase6UIA2NR8TestTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

void UCardSelectionPresentationHUDProbe::SetTestWorld(UWorld* InWorld)
{
	TestWorld = InWorld;
}

void UCardSelectionPresentationHUDProbe::ConfigureSelectionSurfaces(
	UBattleHUDViewModel* InViewModel,
	UHorizontalBox* InHand,
	UOverlay* InPlayArea,
	UTextBlock* InDrawCount,
	UTextBlock* InDiscardCount,
	UTextBlock* InExhaustCount)
{
	ViewModel = InViewModel;
	HB_Hand = InHand;
	OV_PlayArea = InPlayArea;
	Txt_DrawCount = InDrawCount;
	Txt_DiscardCount = InDiscardCount;
	Txt_ExhaustCount = InExhaustCount;
	CardWidgetClass = UPhase6UIA2NR8CardProbe::StaticClass();
}

void UCardSelectionPresentationHUDProbe::InvokeNativeTickForTesting(float DeltaSeconds)
{
	NativeTick(GetCachedGeometry(), DeltaSeconds);
}

UWorld* UCardSelectionPresentationHUDProbe::GetWorld() const
{
	return TestWorld.Get();
}

void UCardSelectionPresentationHUDProbe::BindConfirmButtonForTesting(UButton* Button)
{
	Btn_Confirm = Button;
	NativeConstruct();
}
