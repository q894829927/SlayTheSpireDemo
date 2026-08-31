#include "Phase6UIA2NR6TestTypes.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"

void UPhase6UIA2NR6HUDProbe::SetTestWorld(UWorld* InWorld)
{
	TestWorld = InWorld;
}

void UPhase6UIA2NR6HUDProbe::ConfigureR6Surfaces(
	UTextBlock* InEnergyText,
	UTextBlock* InPlayerBlockText,
	UTextBlock* InEnemyBlockText,
	UTextBlock* InDrawCountText,
	UTextBlock* InDiscardCountText)
{
	Txt_Energy = InEnergyText;
	Txt_PlayerBlock = InPlayerBlockText;
	Txt_EnemyBlock = InEnemyBlockText;
	Txt_DrawCount = InDrawCountText;
	Txt_DiscardCount = InDiscardCountText;
}

UWorld* UPhase6UIA2NR6HUDProbe::GetWorld() const
{
	return TestWorld.Get();
}

void UPhase6UIA2NR6HUDProbe::InvokeFinishForTesting(
	const FPresentationPlaybackToken& Token)
{
	FinishNativePresentation(Token);
}

void UPhase6UIA2NR6HUDProbe::InvokeCancelForTesting(
	const FPresentationPlaybackToken& Token)
{
	CancelPresentationRecordPlayback(Token);
}

void UPhase6UIA2NR6HUDProbe::InvokeNativeDestructForTesting()
{
	NativeDestruct();
}

void UPhase6UIA2NR6HUDProbe::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& Token)
{
	++CancelDispatchCount;
	LastCancelDispatchToken = Token;
	Super::CancelPresentationRecordPlayback_Implementation(Token);
}
