#include "Phase6UIA2NR10TestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UPhase6UIA2NR10HUDProbe::SetTestWorld(UWorld* InWorld)
{
	TestWorld = InWorld;
}

void UPhase6UIA2NR10HUDProbe::ConfigureTerminalSurfaces(
	UOverlay* InTerminalOverlay,
	UTextBlock* InOutcomeText,
	UTextBlock* InFeedbackText,
	UButton* InEndTurnButton)
{
	Overlay_Terminal = InTerminalOverlay;
	Txt_Outcome = InOutcomeText;
	Txt_Feedback = InFeedbackText;
	Btn_EndTurn = InEndTurnButton;
}

UWorld* UPhase6UIA2NR10HUDProbe::GetWorld() const
{
	return IsValid(TestWorld) ? TestWorld.Get() : Super::GetWorld();
}

bool UPhase6UIA2NR10HUDProbe::InvokeBeginDirectForTesting(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	return BeginPresentationRecordPlayback_Implementation(Record, Token);
}

void UPhase6UIA2NR10HUDProbe::InvokeFinishForTesting(
	const FPresentationPlaybackToken& Token)
{
	FinishNativePresentation(Token);
}

void UPhase6UIA2NR10HUDProbe::InvokeCancelForTesting(
	const FPresentationPlaybackToken& Token)
{
	CancelPresentationRecordPlayback_Implementation(Token);
}

void UPhase6UIA2NR10HUDProbe::InvokeNativeDestructForTesting()
{
	NativeDestruct();
}

void UPhase6UIA2NR10HUDProbe::RefreshTerminalForTesting()
{
	RefreshTerminalFromViewModel();
}

void UPhase6UIA2NR10HUDProbe::RefreshAvailabilityForTesting()
{
	RefreshPresentationAvailabilityFromViewModel();
}

void UPhase6UIA2NR10HUDProbe::RefreshInputForTesting()
{
	RefreshInputState();
}

#endif
