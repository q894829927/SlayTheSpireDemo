#include "BattlePresentationController.h"

#if WITH_DEV_AUTOMATION_TESTS
void UBattlePresentationController::ExpireActivePlaybackForTesting()
{
	HandleActiveTimeout(0.0f);
}

bool UBattlePresentationController::TryGetWorkingSnapshotForTesting(
	FPresentationStateSnapshot& OutSnapshot
) const
{
	OutSnapshot = FPresentationStateSnapshot{};
	if (!bHasWorkingPresentationSnapshot)
	{
		return false;
	}
	OutSnapshot = WorkingPresentationSnapshot;
	return true;
}

bool UBattlePresentationController::ReduceEnvelopeForTesting(
	const FPresentationStateSnapshot& Baseline,
	const FPresentationResolutionEnvelope& Envelope,
	FPresentationStateSnapshot& OutReducedSnapshot
)
{
	OutReducedSnapshot = FPresentationStateSnapshot{};
	if (Baseline.BattleId <= 0
		|| Envelope.BattleId != Baseline.BattleId
		|| Envelope.FinalSnapshot.BattleId != Envelope.BattleId
		|| Envelope.FinalStateRevision != Envelope.FinalSnapshot.StateRevision)
	{
		return false;
	}

	const FPresentationResolutionEnvelope SavedActiveEnvelope = ActiveEnvelope;
	const FPresentationStateSnapshot SavedWorkingSnapshot = WorkingPresentationSnapshot;
	const bool bSavedHasWorkingSnapshot = bHasWorkingPresentationSnapshot;

	ActiveEnvelope = Envelope;
	WorkingPresentationSnapshot = Baseline;
	bHasWorkingPresentationSnapshot = true;

	bool bAppliedAllRecords = true;
	for (const FPresentationRecord& Record : Envelope.Records)
	{
		if (!ApplyRecordToWorkingSnapshot(Record))
		{
			bAppliedAllRecords = false;
			break;
		}
	}

	if (bAppliedAllRecords)
	{
		OutReducedSnapshot = WorkingPresentationSnapshot;
	}

	ActiveEnvelope = SavedActiveEnvelope;
	WorkingPresentationSnapshot = SavedWorkingSnapshot;
	bHasWorkingPresentationSnapshot = bSavedHasWorkingSnapshot;
	return bAppliedAllRecords;
}
#endif
