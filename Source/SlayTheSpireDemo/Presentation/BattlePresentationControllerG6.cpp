#include "BattlePresentationController.h"

#include "../UI/BattleHUDWidgetBase.h"

namespace
{
	bool IsSameOfferedRecordIdentity(
		const FPresentationRecord& Left,
		const FPresentationRecord& Right)
	{
		return Left.BattleId == Right.BattleId
			&& Left.ResolutionId == Right.ResolutionId
			&& Left.PresentationSequence == Right.PresentationSequence
			&& Left.Type == Right.Type
			&& Left.Group.Kind == Right.Group.Kind
			&& Left.Group.GroupId == Right.Group.GroupId
			&& Left.Group.ExpectedMemberCount == Right.Group.ExpectedMemberCount;
	}
}

bool UBattlePresentationController::TryActivatePresentationGroupG6(
	const FPresentationRecord& LeaderRecord,
	const FPresentationPlaybackToken& OfferedSingleRecordToken)
{
	if (!bHasActiveEnvelope
		|| !bHasWorkingPresentationSnapshot
		|| !bWaitingForCompletion
		|| !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex)
		|| !IsSameOfferedRecordIdentity(
			LeaderRecord,
			ActiveEnvelope.Records[ActiveRecordIndex])
		|| !IsValid(Widget)
		|| OfferedSingleRecordToken != ActivePlaybackToken
		|| OfferedSingleRecordToken.UnitKind != EPresentationPlaybackUnitKind::SingleRecord
		|| OfferedSingleRecordToken.GroupId != 0
		|| OfferedSingleRecordToken.LocalPlaybackGeneration != LocalPlaybackGeneration
		|| LeaderRecord.BattleId != CurrentBattleId
		|| LeaderRecord.BattleId != OfferedSingleRecordToken.BattleId
		|| LeaderRecord.ResolutionId != OfferedSingleRecordToken.ResolutionId
		|| LeaderRecord.PresentationSequence != OfferedSingleRecordToken.PresentationSequence)
	{
		return false;
	}

	FPresentationGroupSemanticCandidate Candidate;
	if (!TryBuildSemanticPresentationGroupCandidate(
		WorkingPresentationSnapshot,
		ActiveEnvelope,
		ActiveRecordIndex,
		Candidate)
		|| !Candidate.IsValid()
		|| Candidate.MemberRecordIndices.IsEmpty()
		|| Candidate.MemberRecordIndices[0] != ActiveRecordIndex)
	{
		return false;
	}

	TArray<FPresentationRecord> Records;
	Records.Reserve(Candidate.MemberRecordIndices.Num());
	for (const int32 RecordIndex : Candidate.MemberRecordIndices)
	{
		if (!ActiveEnvelope.Records.IsValidIndex(RecordIndex))
		{
			return false;
		}
		Records.Add(ActiveEnvelope.Records[RecordIndex]);
	}

	const int64 NextGeneration = LocalPlaybackGeneration == MAX_int64
		? 1
		: LocalPlaybackGeneration + 1;
	FPresentationPlaybackToken GroupToken;
	GroupToken.BattleId = OfferedSingleRecordToken.BattleId;
	GroupToken.ResolutionId = OfferedSingleRecordToken.ResolutionId;
	GroupToken.PresentationSequence = OfferedSingleRecordToken.PresentationSequence;
	GroupToken.LocalPlaybackGeneration = NextGeneration;
	GroupToken.UnitKind = EPresentationPlaybackUnitKind::Group;
	GroupToken.GroupId = Candidate.Group.GroupId;

	// The Base Widget replaces its tracked leader unit transactionally. A visual
	// decline restores the exact offered SingleRecord token, so the caller may
	// immediately continue through the sealed G5 Begin path.
	if (!Widget->TryReplaceTrackedPresentationRecordWithGroup(
		OfferedSingleRecordToken,
		Records,
		Candidate.Group,
		Candidate.MemberRecordIndices,
		GroupToken))
	{
		return false;
	}

	LocalPlaybackGeneration = NextGeneration;
	ActivePlaybackToken = GroupToken;
	ActiveG6Group = Candidate.Group;
	ActiveG6GroupRecordIndices = Candidate.MemberRecordIndices;
	return true;
}

bool UBattlePresentationController::ConsumeVisuallyPresentedGroupRecordG6(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& OfferedSingleRecordToken)
{
	if (!bHasActiveEnvelope
		|| !bWaitingForCompletion
		|| !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex)
		|| !IsSameOfferedRecordIdentity(
			Record,
			ActiveEnvelope.Records[ActiveRecordIndex])
		|| OfferedSingleRecordToken != ActivePlaybackToken
		|| OfferedSingleRecordToken.UnitKind != EPresentationPlaybackUnitKind::SingleRecord
		|| Record.ResolutionId != ActiveEnvelope.ResolutionId)
	{
		return false;
	}

	if (G6VisuallyPresentedResolutionId != Record.ResolutionId)
	{
		// Lazy stale-scope cleanup covers timeout/collapse/replacement paths that
		// deliberately remain owned by the existing G3 recovery implementation.
		G6VisuallyPresentedResolutionId = 0;
		G6VisuallyPresentedRecordIndices.Reset();
		return false;
	}

	if (G6VisuallyPresentedRecordIndices.Remove(ActiveRecordIndex) == 0)
	{
		return false;
	}
	if (G6VisuallyPresentedRecordIndices.IsEmpty())
	{
		G6VisuallyPresentedResolutionId = 0;
	}
	return true;
}

void UBattlePresentationController::NotifyPresentationGroupFinishedG6(
	const FPresentationPlaybackToken& Token,
	const TArray<int32>& RecordIndices)
{
	if (!bHasActiveEnvelope
		|| !bWaitingForCompletion
		|| Token != ActivePlaybackToken
		|| Token.UnitKind != EPresentationPlaybackUnitKind::Group
		|| Token.LocalPlaybackGeneration != LocalPlaybackGeneration
		|| Token.BattleId != CurrentBattleId
		|| !ActiveG6Group.IsValid()
		|| ActiveG6Group.Kind != EPresentationGroupKind::SelectionDestination
		|| ActiveG6Group.GroupId != Token.GroupId
		|| RecordIndices != ActiveG6GroupRecordIndices
		|| RecordIndices.IsEmpty()
		|| RecordIndices[0] != ActiveRecordIndex)
	{
		return;
	}

	CancelActiveTimeout();
	G6VisuallyPresentedResolutionId = Token.ResolutionId;
	for (int32 MemberIndex = 1; MemberIndex < RecordIndices.Num(); ++MemberIndex)
	{
		const int32 RecordIndex = RecordIndices[MemberIndex];
		if (!ActiveEnvelope.Records.IsValidIndex(RecordIndex)
			|| RecordIndex <= ActiveRecordIndex)
		{
			// An accepted Base Widget Group should already have validated this. If
			// the retained index set is corrupted, use the existing ActiveEnvelope
			// recovery instead of partially advancing chronology.
			G6VisuallyPresentedResolutionId = 0;
			G6VisuallyPresentedRecordIndices.Reset();
			ResetG6ActiveGroupState();
			ReconcileActiveEnvelopeToFinalSnapshot();
			return;
		}
		G6VisuallyPresentedRecordIndices.Add(RecordIndex);
	}

	ResetG6ActiveGroupState();
	// Reduce only the leader now. CompleteActiveRecord publishes its exact
	// chronological snapshot and then advances through interleaved records. Future
	// Group members are reduced only when their own normal cursor is reached.
	CompleteActiveRecord();
}

void UBattlePresentationController::ResetG6ActiveGroupState()
{
	ActiveG6Group = FPresentationGroupTag{};
	ActiveG6GroupRecordIndices.Reset();
}

#if WITH_DEV_AUTOMATION_TESTS
int32 UBattlePresentationController::GetG6VisuallyPresentedRecordCountForTesting() const
{
	return G6VisuallyPresentedRecordIndices.Num();
}
#endif
