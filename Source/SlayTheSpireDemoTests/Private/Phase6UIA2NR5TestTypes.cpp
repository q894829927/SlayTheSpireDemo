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
	const bool bAcceptSyntheticRecord = bAcceptSyntheticPlayback
		&& (Record.Type == EBattlePresentationRecordType::Damage
			|| (bAcceptSyntheticBlockPlayback
				&& Record.Type == EBattlePresentationRecordType::BlockChanged));
	if (!bAcceptSyntheticRecord)
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

bool UPhase6UIA2NR5HUDProbe::PrepareDetachedDamageVisual(
	const FPresentationSessionToken& SessionToken,
	const FPresentationRecord& Record,
	int64 SourceFinalStateRevision,
	float VisualDuration,
	FDetachedDamageToken& OutToken)
{
	OutToken = FDetachedDamageToken{};
	if (!bAcceptDetachedDamage
		|| !SessionToken.IsValid()
		|| Record.Type != EBattlePresentationRecordType::Damage
		|| Record.ResolutionId <= 0
		|| Record.PresentationSequence <= 0
		|| SourceFinalStateRevision <= 0
		|| !FMath::IsFinite(VisualDuration)
		|| VisualDuration <= 0.0f)
	{
		return false;
	}

	FDetachedDamageToken Token;
	Token.SessionToken = SessionToken;
	Token.SourceResolutionId = Record.ResolutionId;
	Token.PresentationSequence = Record.PresentationSequence;
	Token.LocalDamageVisualGeneration = NextSyntheticDetachedGeneration++;
	if (!Token.IsValid())
	{
		return false;
	}

	LastPreparedDetachedToken = Token;
	bHasPreparedDetachedDamage = true;
	bHasActiveDetachedDamage = false;
	++DetachedPrepareCount;
	OutToken = Token;
	return true;
}

bool UPhase6UIA2NR5HUDProbe::ActivatePreparedDetachedDamageVisual(
	const FDetachedDamageToken& Token)
{
	if (!bAcceptDetachedDamage
		|| !bHasPreparedDetachedDamage
		|| Token != LastPreparedDetachedToken)
	{
		return false;
	}
	bHasPreparedDetachedDamage = false;
	bHasActiveDetachedDamage = true;
	LastActivatedDetachedToken = Token;
	++DetachedActivateCount;
	return true;
}

bool UPhase6UIA2NR5HUDProbe::CancelDetachedDamageVisual(
	const FDetachedDamageToken& Token)
{
	if (!Token.IsValid())
	{
		return false;
	}
	const bool bMatchesPrepared = bHasPreparedDetachedDamage
		&& Token == LastPreparedDetachedToken;
	const bool bMatchesActive = bHasActiveDetachedDamage
		&& Token == LastActivatedDetachedToken;
	if (!bMatchesPrepared && !bMatchesActive)
	{
		return false;
	}
	bHasPreparedDetachedDamage = false;
	bHasActiveDetachedDamage = false;
	++DetachedCancelCount;
	return true;
}

int32 UPhase6UIA2NR5HUDProbe::CancelDetachedDamageVisualsForSession(
	const FPresentationSessionToken& SessionToken)
{
	int32 Cancelled = 0;
	if (bHasPreparedDetachedDamage
		&& LastPreparedDetachedToken.SessionToken == SessionToken)
	{
		bHasPreparedDetachedDamage = false;
		++Cancelled;
	}
	if (bHasActiveDetachedDamage
		&& LastActivatedDetachedToken.SessionToken == SessionToken)
	{
		bHasActiveDetachedDamage = false;
		++Cancelled;
	}
	DetachedCancelCount += Cancelled;
	return Cancelled;
}

void UPhase6UIA2NR5HUDProbe::CancelAllDetachedDamageVisuals()
{
	int32 Cancelled = 0;
	Cancelled += bHasPreparedDetachedDamage ? 1 : 0;
	Cancelled += bHasActiveDetachedDamage ? 1 : 0;
	bHasPreparedDetachedDamage = false;
	bHasActiveDetachedDamage = false;
	DetachedCancelCount += Cancelled;
}

void UPhase6UIA2NR5HUDProbe::PlayCommittedDamageCombatantCues(
	const FPresentationRecord& Record,
	const FPresentationSessionToken& SessionToken)
{
	if (bAcceptDetachedDamage
		&& Record.Type == EBattlePresentationRecordType::Damage
		&& SessionToken.IsValid())
	{
		++DetachedCueCount;
	}
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
