#include "BattlePresentationRecorder.h"

bool FPresentationRecordWriter::IsAvailable() const
{
	UBattlePresentationRecorder* ResolvedRecorder = Recorder.Get();
	return BattleId != 0
		&& ResolutionId != 0
		&& IsValid(ResolvedRecorder)
		&& ResolvedRecorder->IsWriterCurrentAndValid(BattleId, ResolutionId);
}

bool FPresentationRecordWriter::Append(FPresentationRecord Record) const
{
	UBattlePresentationRecorder* ResolvedRecorder = Recorder.Get();
	return IsAvailable()
		&& ResolvedRecorder->AppendRecord(BattleId, ResolutionId, MoveTemp(Record));
}

bool FPresentationRecordWriter::InvalidateCurrentResolution() const
{
	UBattlePresentationRecorder* ResolvedRecorder = Recorder.Get();
	return IsAvailable()
		&& ResolvedRecorder->InvalidateWriterResolution(BattleId, ResolutionId);
}

bool FPresentationRecordWriter::TryAcceptSelectionOutcome() const
{
	if (BattleId == 0 || SelectionBoundaryRevision <= 0)
	{
		return false;
	}

	if (IsAvailable())
	{
		return TryRecordSelectionOutcome(SelectionBoundaryRevision);
	}

	return DirectOutcomeAcceptance.IsBound()
		&& DirectOutcomeAcceptance.Execute(BattleId, SelectionBoundaryRevision);
}

bool FPresentationRecordWriter::TryRecordSelectionOutcome(int64 InSelectionBoundaryRevision) const
{
	UBattlePresentationRecorder* ResolvedRecorder = Recorder.Get();
	return IsAvailable()
		&& ResolvedRecorder->RecordSelectionOutcome(BattleId, ResolutionId, InSelectionBoundaryRevision);
}

bool FPresentationRecordWriter::TryAllocatePresentationGroupId(int64& OutGroupId) const
{
	OutGroupId = 0;
	UBattlePresentationRecorder* ResolvedRecorder = Recorder.Get();
	return IsAvailable()
		&& ResolvedRecorder->AllocatePresentationGroupId(BattleId, ResolutionId, OutGroupId);
}

bool FPresentationRecordWriter::TryDeclarePresentationGroup(const FPresentationGroupDeclaration& Declaration) const
{
	UBattlePresentationRecorder* ResolvedRecorder = Recorder.Get();
	return IsAvailable()
		&& ResolvedRecorder->DeclarePresentationGroup(BattleId, ResolutionId, Declaration);
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
	if (BattleId == 0 || NextResolutionId == 0)
	{
		return false;
	}

	if (ActiveBuilder.bActive)
	{
		ClearActiveBuilder();
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
	OutEnvelope.PresentationGroups = MoveTemp(ActiveBuilder.PresentationGroups);
	OutEnvelope.SelectionOutcomes = MoveTemp(ActiveBuilder.SelectionOutcomes);
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
	if (!IsWriterCurrentAndValid(WriterBattleId, WriterResolutionId))
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

	// Group metadata is optional. A stale/malformed tag degrades to ordinary
	// serial history instead of invalidating an otherwise trustworthy Record.
	if (Record.Group.Kind != EPresentationGroupKind::None)
	{
		const FPresentationGroupDeclaration* Declaration = ActiveBuilder.PresentationGroups.FindByPredicate(
			[&Record](const FPresentationGroupDeclaration& Candidate)
			{
				return Candidate.Group.GroupId == Record.Group.GroupId;
			}
		);
		if (Declaration == nullptr
			|| Declaration->Group.Kind != Record.Group.Kind
			|| Declaration->Group.ExpectedMemberCount != Record.Group.ExpectedMemberCount)
		{
			Record.Group = FPresentationGroupTag{};
		}
	}

	// ResolutionFault, Victory and Defeat are all terminal presentation facts.
	// Once any terminal record is accepted, no later record may follow. A repeated
	// terminal or any ordinary append after terminal invalidates the whole
	// unpublished batch instead of allowing an ambiguous history to seal.
	if (ActiveBuilder.bTerminalRecordAppended)
	{
		InvalidateActiveBuilder();
		return false;
	}

	Record.BattleId = static_cast<int64>(ActiveBuilder.BattleId);
	Record.ResolutionId = static_cast<int64>(ActiveBuilder.ResolutionId);
	Record.PresentationSequence = static_cast<int64>(NextPresentationSequence++);
	ActiveBuilder.bTerminalRecordAppended = IsTerminalRecordType(Record.Type);
	ActiveBuilder.Records.Add(MoveTemp(Record));
	return true;
}

bool UBattlePresentationRecorder::TryGetActiveWriter(FPresentationRecordWriter& OutWriter) const
{
	OutWriter = FPresentationRecordWriter{};
	if (!ActiveBuilder.bActive || !ActiveBuilder.bValid)
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

bool UBattlePresentationRecorder::IsTerminalRecordType(EBattlePresentationRecordType Type)
{
	return Type == EBattlePresentationRecordType::ResolutionFault
		|| Type == EBattlePresentationRecordType::Victory
		|| Type == EBattlePresentationRecordType::Defeat;
}

bool UBattlePresentationRecorder::IsWriterCurrentAndValid(
	uint64 WriterBattleId,
	uint64 WriterResolutionId
) const
{
	return ActiveBuilder.bActive
		&& ActiveBuilder.bValid
		&& WriterBattleId != 0
		&& WriterResolutionId != 0
		&& WriterBattleId == ActiveBuilder.BattleId
		&& WriterResolutionId == ActiveBuilder.ResolutionId;
}

bool UBattlePresentationRecorder::InvalidateWriterResolution(
	uint64 WriterBattleId,
	uint64 WriterResolutionId
)
{
	if (!IsWriterCurrentAndValid(WriterBattleId, WriterResolutionId))
	{
		return false;
	}

	InvalidateActiveBuilder();
	return true;
}

bool UBattlePresentationRecorder::RecordSelectionOutcome(
	uint64 WriterBattleId,
	uint64 WriterResolutionId,
	int64 SelectionBoundaryRevision
)
{
	if (!IsWriterCurrentAndValid(WriterBattleId, WriterResolutionId)
		|| SelectionBoundaryRevision <= 0)
	{
		return false;
	}

	const FSelectionPresentationOutcomeReceipt* Existing = ActiveBuilder.SelectionOutcomes.FindByPredicate(
		[SelectionBoundaryRevision](const FSelectionPresentationOutcomeReceipt& Receipt)
		{
			return Receipt.SelectionBoundaryRevision == SelectionBoundaryRevision;
		}
	);
	if (Existing != nullptr)
	{
		return Existing->BattleId == static_cast<int64>(WriterBattleId)
			&& Existing->Mode == ESelectionPresentationOutcomeMode::RecordedResolution
			&& Existing->ResolutionId == static_cast<int64>(WriterResolutionId);
	}

	FSelectionPresentationOutcomeReceipt Receipt;
	Receipt.BattleId = static_cast<int64>(WriterBattleId);
	Receipt.SelectionBoundaryRevision = SelectionBoundaryRevision;
	Receipt.Mode = ESelectionPresentationOutcomeMode::RecordedResolution;
	Receipt.ResolutionId = static_cast<int64>(WriterResolutionId);
	if (!Receipt.IsValid())
	{
		return false;
	}

	ActiveBuilder.SelectionOutcomes.Add(MoveTemp(Receipt));
	return true;
}

bool UBattlePresentationRecorder::AllocatePresentationGroupId(
	uint64 WriterBattleId,
	uint64 WriterResolutionId,
	int64& OutGroupId
)
{
	OutGroupId = 0;
	if (!IsWriterCurrentAndValid(WriterBattleId, WriterResolutionId)
		|| ActiveBuilder.NextPresentationGroupId <= 0)
	{
		return false;
	}

	OutGroupId = ActiveBuilder.NextPresentationGroupId;
	if (ActiveBuilder.NextPresentationGroupId == MAX_int64)
	{
		ActiveBuilder.NextPresentationGroupId = 0;
	}
	else
	{
		++ActiveBuilder.NextPresentationGroupId;
	}
	return true;
}

bool UBattlePresentationRecorder::DeclarePresentationGroup(
	uint64 WriterBattleId,
	uint64 WriterResolutionId,
	const FPresentationGroupDeclaration& Declaration
)
{
	if (!IsWriterCurrentAndValid(WriterBattleId, WriterResolutionId)
		|| !Declaration.Group.IsValid()
		|| Declaration.Group.Kind != EPresentationGroupKind::SelectionDestination
		|| Declaration.Group.ExpectedMemberCount <= 0
		|| Declaration.Group.ExpectedMemberCount != Declaration.CanonicalSelectedRuntimeIds.Num()
		|| Declaration.Group.GroupId >= ActiveBuilder.NextPresentationGroupId)
	{
		return false;
	}

	TSet<int32> SeenRuntimeIds;
	for (const int32 RuntimeId : Declaration.CanonicalSelectedRuntimeIds)
	{
		if (RuntimeId == INDEX_NONE || SeenRuntimeIds.Contains(RuntimeId))
		{
			return false;
		}
		SeenRuntimeIds.Add(RuntimeId);
	}

	if (ActiveBuilder.PresentationGroups.ContainsByPredicate(
		[&Declaration](const FPresentationGroupDeclaration& Existing)
		{
			return Existing.Group.GroupId == Declaration.Group.GroupId;
		}))
	{
		return false;
	}

	ActiveBuilder.PresentationGroups.Add(Declaration);
	return true;
}

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
	ActiveBuilder.PresentationGroups.Reset();
	ActiveBuilder.SelectionOutcomes.Reset();
}
