#include "BattlePresentationController.h"

namespace
{
	bool AreGroupTagsEquivalent(
		const FPresentationGroupTag& A,
		const FPresentationGroupTag& B
	)
	{
		return A.Kind == B.Kind
			&& A.GroupId == B.GroupId
			&& A.ExpectedMemberCount == B.ExpectedMemberCount;
	}

	bool TryGetEligibleSelectionDestinationRuntimeId(
		const FPresentationRecord& Record,
		int32& OutRuntimeId
	)
	{
		OutRuntimeId = INDEX_NONE;
		if (Record.Type != EBattlePresentationRecordType::CardZoneChanged
			|| Record.CardZoneChanged.FromZone != ECardZone::Hand
			|| Record.CardZoneChanged.Card.RuntimeId == INDEX_NONE
			|| Record.CardZoneChanged.Card.CardId.IsNone())
		{
			return false;
		}

		switch (Record.CardZoneChanged.ToZone)
		{
		case ECardZone::DiscardPile:
		case ECardZone::DrawPile:
		case ECardZone::ExhaustPile:
			OutRuntimeId = Record.CardZoneChanged.Card.RuntimeId;
			return true;
		default:
			return false;
		}
	}

	enum class EExactCardInteraction : uint8
	{
		None,
		ExactCard,
		Unsupported
	};

	EExactCardInteraction ClassifyExactCardInteraction(
		const FPresentationRecord& Record,
		int32& OutRuntimeId
	)
	{
		OutRuntimeId = INDEX_NONE;
		switch (Record.Type)
		{
		case EBattlePresentationRecordType::CardZoneChanged:
			if (Record.CardZoneChanged.Card.RuntimeId == INDEX_NONE)
			{
				return EExactCardInteraction::Unsupported;
			}
			OutRuntimeId = Record.CardZoneChanged.Card.RuntimeId;
			return EExactCardInteraction::ExactCard;

		case EBattlePresentationRecordType::CardPlayed:
			if (Record.CardPlayed.Card.RuntimeId == INDEX_NONE)
			{
				return EExactCardInteraction::Unsupported;
			}
			OutRuntimeId = Record.CardPlayed.Card.RuntimeId;
			return EExactCardInteraction::ExactCard;

		case EBattlePresentationRecordType::Damage:
		case EBattlePresentationRecordType::BlockChanged:
		case EBattlePresentationRecordType::EnergyChanged:
		case EBattlePresentationRecordType::DeckShuffled:
		case EBattlePresentationRecordType::StatusChanged:
			return EExactCardInteraction::None;

		case EBattlePresentationRecordType::Victory:
		case EBattlePresentationRecordType::Defeat:
		case EBattlePresentationRecordType::ResolutionFault:
		case EBattlePresentationRecordType::None:
		default:
			return EExactCardInteraction::Unsupported;
		}
	}
}

bool UBattlePresentationController::TryBuildSemanticPresentationGroupCandidate(
	const FPresentationStateSnapshot& Baseline,
	const FPresentationResolutionEnvelope& Envelope,
	int32 LeaderRecordIndex,
	FPresentationGroupSemanticCandidate& OutCandidate
)
{
	OutCandidate = FPresentationGroupSemanticCandidate{};
	if (!Envelope.Records.IsValidIndex(LeaderRecordIndex)
		|| Baseline.BattleId <= 0
		|| Envelope.BattleId <= 0
		|| Envelope.ResolutionId <= 0
		|| Envelope.FinalStateRevision <= 0
		|| Baseline.BattleId != Envelope.BattleId
		|| Envelope.FinalSnapshot.BattleId != Envelope.BattleId
		|| Envelope.FinalSnapshot.StateRevision != Envelope.FinalStateRevision)
	{
		return false;
	}

	const FPresentationRecord& Leader = Envelope.Records[LeaderRecordIndex];
	if (!Leader.Group.IsValid()
		|| Leader.Group.Kind != EPresentationGroupKind::SelectionDestination
		|| Leader.Group.ExpectedMemberCount <= 1)
	{
		return false;
	}

	const FPresentationGroupDeclaration* Declaration = nullptr;
	int32 MatchingDeclarationCount = 0;
	for (const FPresentationGroupDeclaration& CandidateDeclaration : Envelope.PresentationGroups)
	{
		if (CandidateDeclaration.Group.GroupId != Leader.Group.GroupId)
		{
			continue;
		}
		++MatchingDeclarationCount;
		Declaration = &CandidateDeclaration;
	}
	if (MatchingDeclarationCount != 1
		|| Declaration == nullptr
		|| !AreGroupTagsEquivalent(Declaration->Group, Leader.Group)
		|| Declaration->Group.Kind != EPresentationGroupKind::SelectionDestination
		|| Declaration->CanonicalSelectedRuntimeIds.Num() != Leader.Group.ExpectedMemberCount)
	{
		return false;
	}

	TSet<int32> CanonicalRuntimeIds;
	for (const int32 RuntimeId : Declaration->CanonicalSelectedRuntimeIds)
	{
		if (RuntimeId == INDEX_NONE || CanonicalRuntimeIds.Contains(RuntimeId))
		{
			return false;
		}
		CanonicalRuntimeIds.Add(RuntimeId);
	}

	TArray<int32> MemberRecordIndices;
	TArray<int32> MemberRuntimeIds;
	TSet<int32> SeenMemberRuntimeIds;
	for (int32 RecordIndex = 0; RecordIndex < Envelope.Records.Num(); ++RecordIndex)
	{
		const FPresentationRecord& Record = Envelope.Records[RecordIndex];
		if (Record.Group.Kind == EPresentationGroupKind::None
			|| Record.Group.GroupId != Leader.Group.GroupId)
		{
			continue;
		}
		if (!AreGroupTagsEquivalent(Record.Group, Leader.Group))
		{
			return false;
		}

		int32 RuntimeId = INDEX_NONE;
		if (!TryGetEligibleSelectionDestinationRuntimeId(Record, RuntimeId)
			|| SeenMemberRuntimeIds.Contains(RuntimeId))
		{
			return false;
		}
		SeenMemberRuntimeIds.Add(RuntimeId);
		MemberRecordIndices.Add(RecordIndex);
		MemberRuntimeIds.Add(RuntimeId);
	}

	if (MemberRecordIndices.Num() != Leader.Group.ExpectedMemberCount
		|| MemberRuntimeIds.Num() != Leader.Group.ExpectedMemberCount
		|| MemberRecordIndices.IsEmpty()
		|| MemberRecordIndices[0] != LeaderRecordIndex)
	{
		return false;
	}
	for (const int32 RuntimeId : Declaration->CanonicalSelectedRuntimeIds)
	{
		if (!SeenMemberRuntimeIds.Contains(RuntimeId))
		{
			return false;
		}
	}

	const int32 LastMemberRecordIndex = MemberRecordIndices.Last();
	int64 PreviousPresentationSequence = 0;
	for (int32 RecordIndex = LeaderRecordIndex; RecordIndex <= LastMemberRecordIndex; ++RecordIndex)
	{
		const FPresentationRecord& Record = Envelope.Records[RecordIndex];
		if (Record.BattleId != Envelope.BattleId
			|| Record.ResolutionId != Envelope.ResolutionId
			|| Record.PresentationSequence <= 0
			|| (PreviousPresentationSequence > 0
				&& Record.PresentationSequence <= PreviousPresentationSequence))
		{
			return false;
		}
		PreviousPresentationSequence = Record.PresentationSequence;
	}

	// Use the production reducer itself as the G2 chronological dry-run. Only
	// temporary Controller reducer state is swapped; no ViewModel or Widget is
	// touched, and every field is restored before the result is observed.
	const FPresentationStateSnapshot SavedWorkingSnapshot = WorkingPresentationSnapshot;
	const FPresentationResolutionEnvelope SavedActiveEnvelope = ActiveEnvelope;
	const bool bSavedHasWorkingSnapshot = bHasWorkingPresentationSnapshot;

	WorkingPresentationSnapshot = Baseline;
	ActiveEnvelope = Envelope;
	bHasWorkingPresentationSnapshot = true;

	bool bDryRunSucceeded = true;
	for (int32 RecordIndex = LeaderRecordIndex; RecordIndex <= LastMemberRecordIndex; ++RecordIndex)
	{
		if (!ApplyRecordToWorkingSnapshot(Envelope.Records[RecordIndex]))
		{
			bDryRunSucceeded = false;
			break;
		}
	}

	WorkingPresentationSnapshot = SavedWorkingSnapshot;
	ActiveEnvelope = SavedActiveEnvelope;
	bHasWorkingPresentationSnapshot = bSavedHasWorkingSnapshot;
	if (!bDryRunSucceeded)
	{
		return false;
	}

	TMap<int32, int32> MemberIndexByRuntimeId;
	TSet<int32> MemberRecordIndexSet;
	for (int32 MemberIndex = 0; MemberIndex < MemberRuntimeIds.Num(); ++MemberIndex)
	{
		MemberIndexByRuntimeId.Add(MemberRuntimeIds[MemberIndex], MemberRecordIndices[MemberIndex]);
		MemberRecordIndexSet.Add(MemberRecordIndices[MemberIndex]);
	}

	for (int32 RecordIndex = LeaderRecordIndex + 1; RecordIndex < LastMemberRecordIndex; ++RecordIndex)
	{
		if (MemberRecordIndexSet.Contains(RecordIndex))
		{
			continue;
		}

		int32 TouchedRuntimeId = INDEX_NONE;
		const EExactCardInteraction Interaction = ClassifyExactCardInteraction(
			Envelope.Records[RecordIndex],
			TouchedRuntimeId
		);
		if (Interaction == EExactCardInteraction::Unsupported)
		{
			return false;
		}
		if (Interaction != EExactCardInteraction::ExactCard)
		{
			continue;
		}

		const int32* FutureMemberRecordIndex = MemberIndexByRuntimeId.Find(TouchedRuntimeId);
		if (FutureMemberRecordIndex != nullptr && *FutureMemberRecordIndex > RecordIndex)
		{
			// This includes records tagged for another group. Group metadata never
			// exempts an exact-card interference with our still-future member.
			return false;
		}
	}

	OutCandidate.Group = Declaration->Group;
	OutCandidate.CanonicalSelectedRuntimeIds = Declaration->CanonicalSelectedRuntimeIds;
	OutCandidate.MemberRecordIndices = MoveTemp(MemberRecordIndices);
	OutCandidate.MemberRuntimeIds = MoveTemp(MemberRuntimeIds);
	return OutCandidate.IsValid();
}

#if WITH_DEV_AUTOMATION_TESTS
bool UBattlePresentationController::TryBuildSemanticPresentationGroupCandidateForTesting(
	const FPresentationStateSnapshot& Baseline,
	const FPresentationResolutionEnvelope& Envelope,
	int32 LeaderRecordIndex,
	FPresentationGroupSemanticCandidate& OutCandidate
)
{
	return TryBuildSemanticPresentationGroupCandidate(
		Baseline,
		Envelope,
		LeaderRecordIndex,
		OutCandidate
	);
}
#endif
