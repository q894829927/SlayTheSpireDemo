#include "BattlePresentationRecorder.h"

bool FPresentationRecordWriter::IsAvailable() const
{
	return BattleId != 0
		&& ResolutionId != 0
		&& Recorder.IsValid();
}

bool FPresentationRecordWriter::Append(FPresentationRecord Record) const
{
	UBattlePresentationRecorder* ResolvedRecorder = Recorder.Get();
	return IsValid(ResolvedRecorder)
		&& ResolvedRecorder->AppendRecord(BattleId, ResolutionId, MoveTemp(Record));
}

void UBattlePresentationRecorder::ResetForBattle(uint64 InBattleId)
{
	BattleId = InBattleId;
	NextResolutionId = 1;
	NextPresentationSequence = 1;
	ClearActiveBuilder();

#if WITH_DEV_AUTOMATION_TESTS
	bForceNextAppendFailureForTesting = false;
	bForceNextSealFailureForTesting = false;
#endif
}

bool UBattlePresentationRecorder::BeginResolution(
	EPresentationResolutionOrigin Origin,
	FPresentationRecordWriter& OutWriter
)
{
	OutWriter = FPresentationRecordWriter{};
	if (BattleId == 0 || ActiveBuilder.bActive || NextResolutionId == 0)
	{
		return false;
	}

	ActiveBuilder = FActiveResolutionBuilder{};
	ActiveBuilder.bActive = true;
	ActiveBuilder.bValid = true;
	ActiveBuilder.BattleId = BattleId;
	ActiveBuilder.ResolutionId = NextResolutionId++;
	ActiveBuilder.Origin = Origin;

	OutWriter.Recorder = this;
	OutWriter.BattleId = ActiveBuilder.BattleId;
	OutWriter.ResolutionId = ActiveBuilder.ResolutionId;
	return true;
}

void UBattlePresentationRecorder::AbortResolution()
{
	ClearActiveBuilder();
}

bool UBattlePresentationRecorder::SealResolution(
	const FPresentationStateSnapshot& FinalSnapshot,
	FPresentationResolutionEnvelope& OutEnvelope
)
{
	OutEnvelope = FPresentationResolutionEnvelope{};
	if (!ActiveBuilder.bActive)
	{
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bForceNextSealFailureForTesting)
	{
		bForceNextSealFailureForTesting = false;
		ClearActiveBuilder();
		return false;
	}
#endif

	const bool bSnapshotIdentityMatches =
		FinalSnapshot.BattleId > 0
		&& static_cast<uint64>(FinalSnapshot.BattleId) == ActiveBuilder.BattleId
		&& FinalSnapshot.StateRevision > 0;

	if (!ActiveBuilder.bValid || !bSnapshotIdentityMatches)
	{
		ClearActiveBuilder();
		return false;
	}

	OutEnvelope.BattleId = static_cast<int64>(ActiveBuilder.BattleId);
	OutEnvelope.ResolutionId = static_cast<int64>(ActiveBuilder.ResolutionId);
	OutEnvelope.Origin = ActiveBuilder.Origin;
	OutEnvelope.FinalStateRevision = FinalSnapshot.StateRevision;
	OutEnvelope.Records = MoveTemp(ActiveBuilder.Records);
	OutEnvelope.FinalSnapshot = FinalSnapshot;

	ClearActiveBuilder();
	return true;
}

bool UBattlePresentationRecorder::AppendRecord(
	uint64 WriterBattleId,
	uint64 WriterResolutionId,
	FPresentationRecord Record
)
{
	if (!ActiveBuilder.bActive
		|| WriterBattleId != ActiveBuilder.BattleId
		|| WriterResolutionId != ActiveBuilder.ResolutionId)
	{
		return false;
	}

	if (!ActiveBuilder.bValid)
	{
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bForceNextAppendFailureForTesting)
	{
		bForceNextAppendFailureForTesting = false;
		InvalidateActiveBuilder();
		return false;
	}
#endif

	if (NextPresentationSequence == 0)
	{
		InvalidateActiveBuilder();
		return false;
	}

	if (Record.Type == EBattlePresentationRecordType::ResolutionFault)
	{
		const bool bAlreadyHasFault = ActiveBuilder.Records.ContainsByPredicate(
			[](const FPresentationRecord& Existing)
			{
				return Existing.Type == EBattlePresentationRecordType::ResolutionFault;
			}
		);
		if (bAlreadyHasFault)
		{
			InvalidateActiveBuilder();
			return false;
		}
	}

	Record.BattleId = static_cast<int64>(ActiveBuilder.BattleId);
	Record.ResolutionId = static_cast<int64>(ActiveBuilder.ResolutionId);
	Record.PresentationSequence = static_cast<int64>(NextPresentationSequence++);
	ActiveBuilder.Records.Add(MoveTemp(Record));
	return true;
}

bool UBattlePresentationRecorder::TryGetActiveWriter(FPresentationRecordWriter& OutWriter) const
{
	OutWriter = FPresentationRecordWriter{};
	if (!ActiveBuilder.bActive)
	{
		return false;
	}

	OutWriter.Recorder = const_cast<UBattlePresentationRecorder*>(this);
	OutWriter.BattleId = ActiveBuilder.BattleId;
	OutWriter.ResolutionId = ActiveBuilder.ResolutionId;
	return OutWriter.IsAvailable();
}

bool UBattlePresentationRecorder::HasActiveResolution() const
{
	return ActiveBuilder.bActive;
}

bool UBattlePresentationRecorder::IsActiveResolutionValid() const
{
	return ActiveBuilder.bActive && ActiveBuilder.bValid;
}

uint64 UBattlePresentationRecorder::GetActiveResolutionId() const
{
	return ActiveBuilder.bActive ? ActiveBuilder.ResolutionId : 0;
}

EPresentationResolutionOrigin UBattlePresentationRecorder::GetActiveOrigin() const
{
	return ActiveBuilder.Origin;
}

uint64 UBattlePresentationRecorder::GetBattleId() const
{
	return BattleId;
}

#if WITH_DEV_AUTOMATION_TESTS
void UBattlePresentationRecorder::SetForceNextAppendFailureForTesting(bool bForce)
{
	bForceNextAppendFailureForTesting = bForce;
}

void UBattlePresentationRecorder::SetForceNextSealFailureForTesting(bool bForce)
{
	bForceNextSealFailureForTesting = bForce;
}

int32 UBattlePresentationRecorder::GetActiveRecordCountForTesting() const
{
	return ActiveBuilder.bActive ? ActiveBuilder.Records.Num() : 0;
}
#endif

void UBattlePresentationRecorder::ClearActiveBuilder()
{
	ActiveBuilder = FActiveResolutionBuilder{};
}

void UBattlePresentationRecorder::InvalidateActiveBuilder()
{
	if (!ActiveBuilder.bActive)
	{
		return;
	}

	ActiveBuilder.bValid = false;
	ActiveBuilder.Records.Reset();
}
