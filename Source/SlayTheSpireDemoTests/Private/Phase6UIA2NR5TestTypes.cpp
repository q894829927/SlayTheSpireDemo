#include "Phase6UIA2NR5TestTypes.h"

#include "Engine/World.h"

void UPhase6UIA2NR5HUDProbe::SetTestWorld(UWorld* InWorld)
{
	TestWorld = InWorld;
}

UWorld* UPhase6UIA2NR5HUDProbe::GetWorld() const
{
	return TestWorld.Get();
}

bool UPhase6UIA2NR5HUDProbe::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	if (!bAcceptSyntheticPlayback || Record.Type != EBattlePresentationRecordType::Damage)
	{
		return Super::BeginPresentationRecordPlayback_Implementation(Record, Token);
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	const float DurationSeconds = bForceTimerFailure ? 0.0f : 60.0f;
	if (!StartNativePresentationFinishTimer(DurationSeconds))
	{
		AbortNativePresentationStart();
		return false;
	}

	return true;
}

void UPhase6UIA2NR5HUDProbe::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& Token)
{
	++CancelDispatchCount;
	LastCancelDispatchToken = Token;
	Super::CancelPresentationRecordPlayback_Implementation(Token);
}

void UPhase6UIA2NR5HUDProbe::InvokeFinishForTesting(
	const FPresentationPlaybackToken& Token)
{
	FinishNativePresentation(Token);
}

void UPhase6UIA2NR5HUDProbe::InvokeCancelForTesting(
	const FPresentationPlaybackToken& Token)
{
	CancelPresentationRecordPlayback(Token);
}

void UPhase6UIA2NR5HUDProbe::InvokeNativeDestructForTesting()
{
	NativeDestruct();
}
