#include "CardSelectionPresentationTestTypes.h"

#include "Phase6UIA2NR8TestTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
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
	UTextBlock* InDrawCount)
{
	ViewModel = InViewModel;
	HB_Hand = InHand;
	OV_PlayArea = InPlayArea;
	Txt_DrawCount = InDrawCount;
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
