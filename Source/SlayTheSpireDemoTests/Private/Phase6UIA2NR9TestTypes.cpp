#include "Phase6UIA2NR9TestTypes.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Engine/World.h"

void UPhase6UIA2NR9StatusProbe::NativeOnInitialized()
{
	if (!IsValid(Img_StatusIcon))
	{
		TestIcon = NewObject<UImage>(this);
		Img_StatusIcon = TestIcon;
	}
	if (!IsValid(Txt_StatusAmount))
	{
		TestAmount = NewObject<UTextBlock>(this);
		Txt_StatusAmount = TestAmount;
	}

	Super::NativeOnInitialized();
}

void UPhase6UIA2NR9HUDProbe::SetTestWorld(UWorld* InWorld)
{
	TestWorld = InWorld;
}

void UPhase6UIA2NR9HUDProbe::ConfigureStatusSurfaces(
	UWrapBox* InPlayerStatuses,
	UWrapBox* InEnemyStatuses)
{
	WB_PlayerStatuses = InPlayerStatuses;
	WB_EnemyStatuses = InEnemyStatuses;
	StatusWidgetClass = UPhase6UIA2NR9StatusProbe::StaticClass();
}

UWorld* UPhase6UIA2NR9HUDProbe::GetWorld() const
{
	return TestWorld.Get();
}

bool UPhase6UIA2NR9HUDProbe::InvokeBeginDirectForTesting(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	return BeginPresentationRecordPlayback_Implementation(Record, Token);
}

void UPhase6UIA2NR9HUDProbe::InvokeFinishForTesting(
	const FPresentationPlaybackToken& Token)
{
	FinishNativePresentation(Token);
}

void UPhase6UIA2NR9HUDProbe::InvokeCancelForTesting(
	const FPresentationPlaybackToken& Token)
{
	CancelPresentationRecordPlayback_Implementation(Token);
}

void UPhase6UIA2NR9HUDProbe::InvokeNativeDestructForTesting()
{
	NativeDestruct();
}
