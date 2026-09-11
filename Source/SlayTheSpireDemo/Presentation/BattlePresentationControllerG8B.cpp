#include "BattlePresentationController.h"

#include "../Battle/BattleManager.h"
#include "../UI/BattleHUDViewModel.h"

bool UBattlePresentationController::HasSkippablePresentationDelay() const
{
	if (!IsPresentationOwnedMode()
		|| !ActivePresentationSessionToken.IsValid())
	{
		return false;
	}

	// G8-B has no compatibility debt yet. Only authoritative chronology that the
	// existing Skip path can collapse counts as a skippable delay. Detached
	// cosmetic instances are intentionally absent from this query.
	return bWaitingForCompletion
		|| bHasActiveEnvelope
		|| PlaybackQueue.Num() > 0;
}

bool UBattlePresentationController::TryCaptureFastInputCatchUpTarget(
	FPresentationSessionToken& OutSessionToken,
	int64& OutBattleId,
	int64& OutExpectedCatchUpRevision) const
{
	OutSessionToken = FPresentationSessionToken{};
	OutBattleId = 0;
	OutExpectedCatchUpRevision = 0;

	if (!HasSkippablePresentationDelay())
	{
		return false;
	}

	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle)
		|| !Battle->IsPresentationAvailable()
		|| !Battle->IsCommittedPresentationRecordingEnabledForBattle())
	{
		return false;
	}

	FPresentationStateSnapshot LatestBaseline;
	if (!Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline)
		|| LatestBaseline.BattleId <= 0
		|| LatestBaseline.BattleId != CurrentBattleId
		|| LatestBaseline.StateRevision <= 0)
	{
		return false;
	}

	OutSessionToken = ActivePresentationSessionToken;
	OutBattleId = LatestBaseline.BattleId;
	OutExpectedCatchUpRevision = LatestBaseline.StateRevision;
	return OutSessionToken.IsValid();
}
