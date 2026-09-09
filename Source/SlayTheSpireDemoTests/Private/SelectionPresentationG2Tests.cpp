#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Presentation/BattlePresentationController.h"

namespace SelectionPresentationG2Test
{
	constexpr int64 BattleId = 8201;
	constexpr int64 ResolutionId = 8202;

	FName MakeCardId(int32 RuntimeId)
	{
		return FName(*FString::Printf(TEXT("G2Card_%d"), RuntimeId));
	}

	FPresentationCardSnapshot MakeCardSnapshot(int32 RuntimeId)
	{
		FPresentationCardSnapshot Snapshot;
		Snapshot.RuntimeId = RuntimeId;
		Snapshot.CardId = MakeCardId(RuntimeId);
		Snapshot.DisplayName = FText::FromName(Snapshot.CardId);
		return Snapshot;
	}

	FBattleHUDCardView MakeHandCard(int32 RuntimeId)
	{
		FBattleHUDCardView Card;
		Card.RuntimeId = RuntimeId;
		Card.CardId = MakeCardId(RuntimeId);
		Card.DisplayName = FText::FromName(Card.CardId);
		return Card;
	}

	FPresentationStateSnapshot MakeBaseline(const TArray<int32>& RuntimeIds)
	{
		FPresentationStateSnapshot Snapshot;
		Snapshot.BattleId = BattleId;
		Snapshot.StateRevision = 100;
		Snapshot.BattleState = EBattleState::PlayerTurn;
		Snapshot.Energy = 3;
		Snapshot.MaxEnergy = 3;
		Snapshot.Player.PresentationId = TEXT("Player");
		Snapshot.Player.HP = 80;
		Snapshot.Enemy.PresentationId = TEXT("Enemy");
		Snapshot.Enemy.HP = 100;
		for (const int32 RuntimeId : RuntimeIds)
		{
			Snapshot.HandCards.Add(MakeHandCard(RuntimeId));
		}
		return Snapshot;
	}

	FPresentationGroupDeclaration MakeDeclaration(
		int64 GroupId,
		const TArray<int32>& RuntimeIds
	)
	{
		FPresentationGroupDeclaration Declaration;
		Declaration.Group.Kind = EPresentationGroupKind::SelectionDestination;
		Declaration.Group.GroupId = GroupId;
		Declaration.CanonicalSelectedRuntimeIds = RuntimeIds;
		Declaration.Group.ExpectedMemberCount = RuntimeIds.Num();
		return Declaration;
	}

	FPresentationRecord MakeZoneRecord(
		int64 Sequence,
		int32 RuntimeId,
		ECardZone FromZone,
		ECardZone ToZone,
		int32 FromIndex,
		int32 ToIndex,
		const FPresentationGroupTag* Group = nullptr
	)
	{
		FPresentationRecord Record;
		Record.BattleId = BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::CardZoneChanged;
		Record.CardZoneChanged.Card = MakeCardSnapshot(RuntimeId);
		Record.CardZoneChanged.FromZone = FromZone;
		Record.CardZoneChanged.ToZone = ToZone;
		Record.CardZoneChanged.FromIndex = FromIndex;
		Record.CardZoneChanged.ToIndex = ToIndex;
		if (Group != nullptr)
		{
			Record.Group = *Group;
		}
		return Record;
	}

	FPresentationRecord MakeDamageRecord(int64 Sequence)
	{
		FPresentationRecord Record;
		Record.BattleId = BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::Damage;
		Record.Damage.SourcePresentationId = TEXT("Player");
		Record.Damage.TargetPresentationId = TEXT("Enemy");
		Record.Damage.HPBefore = 100;
		Record.Damage.HPAfter = 99;
		Record.Damage.BlockBefore = 0;
		Record.Damage.BlockAfter = 0;
		return Record;
	}

	FPresentationResolutionEnvelope MakeEnvelope(
		const FPresentationStateSnapshot& Baseline,
		const TArray<FPresentationRecord>& Records,
		const FPresentationGroupDeclaration* Declaration
	)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::PlayCard;
		Envelope.FinalStateRevision = Baseline.StateRevision + 1;
		Envelope.Records = Records;
		if (Declaration != nullptr)
		{
			Envelope.PresentationGroups.Add(*Declaration);
		}
		Envelope.FinalSnapshot = Baseline;
		Envelope.FinalSnapshot.StateRevision = Envelope.FinalStateRevision;
		return Envelope;
	}
}

using namespace SelectionPresentationG2Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG2ContiguousGroupTest,
	"SlayTheSpireDemo.SelectionPresentation.G2.ContiguousGroup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG2ContiguousGroupTest::RunTest(const FString& Parameters)
{
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>();
	const FPresentationStateSnapshot Baseline = MakeBaseline({ 11, 12, 13 });
	const FPresentationGroupDeclaration Declaration = MakeDeclaration(1, { 11, 12, 13 });
	TArray<FPresentationRecord> Records;
	Records.Add(MakeZoneRecord(1, 11, ECardZone::Hand, ECardZone::ExhaustPile, 0, 0, &Declaration.Group));
	Records.Add(MakeZoneRecord(2, 12, ECardZone::Hand, ECardZone::ExhaustPile, 0, 1, &Declaration.Group));
	Records.Add(MakeZoneRecord(3, 13, ECardZone::Hand, ECardZone::ExhaustPile, 0, 2, &Declaration.Group));
	const FPresentationResolutionEnvelope Envelope = MakeEnvelope(Baseline, Records, &Declaration);

	FPresentationGroupSemanticCandidate Candidate;
	TestTrue(TEXT("Complete contiguous group is semantically eligible"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, Envelope, 0, Candidate));
	TestTrue(TEXT("Candidate is valid"), Candidate.IsValid());
	TestEqual(TEXT("All three member records are discovered"), Candidate.MemberRecordIndices.Num(), 3);
	TestFalse(TEXT("Second member cannot become a new leader"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, Envelope, 1, Candidate));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG2NonContiguousGroupTest,
	"SlayTheSpireDemo.SelectionPresentation.G2.NonContiguousUnrelated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG2NonContiguousGroupTest::RunTest(const FString& Parameters)
{
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>();
	const FPresentationStateSnapshot Baseline = MakeBaseline({ 21, 29, 22, 23 });
	const FPresentationGroupDeclaration Declaration = MakeDeclaration(2, { 21, 22, 23 });
	TArray<FPresentationRecord> Records;
	Records.Add(MakeZoneRecord(1, 21, ECardZone::Hand, ECardZone::ExhaustPile, 0, 0, &Declaration.Group));
	Records.Add(MakeZoneRecord(2, 29, ECardZone::Hand, ECardZone::DiscardPile, 0, 0));
	Records.Add(MakeDamageRecord(3));
	Records.Add(MakeZoneRecord(4, 22, ECardZone::Hand, ECardZone::ExhaustPile, 0, 1, &Declaration.Group));
	Records.Add(MakeZoneRecord(5, 23, ECardZone::Hand, ECardZone::ExhaustPile, 0, 2, &Declaration.Group));
	const FPresentationResolutionEnvelope Envelope = MakeEnvelope(Baseline, Records, &Declaration);

	FPresentationGroupSemanticCandidate Candidate;
	TestTrue(TEXT("Unrelated card move and Damage do not reject non-contiguous group"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, Envelope, 0, Candidate));
	if (Candidate.MemberRecordIndices.Num() == 3)
	{
		TestEqual(TEXT("First member index"), Candidate.MemberRecordIndices[0], 0);
		TestEqual(TEXT("Second member skips unrelated records"), Candidate.MemberRecordIndices[1], 3);
		TestEqual(TEXT("Third member index"), Candidate.MemberRecordIndices[2], 4);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG2FutureMemberInterferenceTest,
	"SlayTheSpireDemo.SelectionPresentation.G2.FutureMemberInterference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG2FutureMemberInterferenceTest::RunTest(const FString& Parameters)
{
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>();
	FPresentationStateSnapshot Baseline = MakeBaseline({ 31, 32, 33 });
	Baseline.DrawCount = 0;
	const FPresentationGroupDeclaration Declaration = MakeDeclaration(3, { 31, 32, 33 });
	FPresentationGroupTag ForeignGroup;
	ForeignGroup.Kind = EPresentationGroupKind::SelectionDestination;
	ForeignGroup.GroupId = 99;
	ForeignGroup.ExpectedMemberCount = 2;

	TArray<FPresentationRecord> Records;
	Records.Add(MakeZoneRecord(1, 31, ECardZone::Hand, ECardZone::ExhaustPile, 0, 0, &Declaration.Group));
	Records.Add(MakeZoneRecord(2, 32, ECardZone::Hand, ECardZone::DrawPile, 0, 0, &ForeignGroup));
	Records.Add(MakeZoneRecord(3, 32, ECardZone::DrawPile, ECardZone::Hand, 0, 0));
	Records.Add(MakeZoneRecord(4, 32, ECardZone::Hand, ECardZone::ExhaustPile, 0, 1, &Declaration.Group));
	Records.Add(MakeZoneRecord(5, 33, ECardZone::Hand, ECardZone::ExhaustPile, 0, 2, &Declaration.Group));
	const FPresentationResolutionEnvelope Envelope = MakeEnvelope(Baseline, Records, &Declaration);

	FPresentationStateSnapshot Reduced;
	TestTrue(TEXT("Chronological reducer dry-run itself succeeds after B leaves and returns"), Controller->ReduceEnvelopeForTesting(Baseline, Envelope, Reduced));
	FPresentationGroupSemanticCandidate Candidate;
	TestFalse(TEXT("Future B exact-card interference rejects group despite successful reducer dry-run"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, Envelope, 0, Candidate));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG2ManifestAndIncompleteTest,
	"SlayTheSpireDemo.SelectionPresentation.G2.ManifestAndIncomplete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG2ManifestAndIncompleteTest::RunTest(const FString& Parameters)
{
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>();
	const FPresentationStateSnapshot Baseline = MakeBaseline({ 41, 42, 44 });
	const FPresentationGroupDeclaration Declaration = MakeDeclaration(4, { 41, 42, 43 });
	TArray<FPresentationRecord> WrongMembers;
	WrongMembers.Add(MakeZoneRecord(1, 41, ECardZone::Hand, ECardZone::ExhaustPile, 0, 0, &Declaration.Group));
	WrongMembers.Add(MakeZoneRecord(2, 42, ECardZone::Hand, ECardZone::ExhaustPile, 0, 1, &Declaration.Group));
	WrongMembers.Add(MakeZoneRecord(3, 44, ECardZone::Hand, ECardZone::ExhaustPile, 0, 2, &Declaration.Group));

	FPresentationGroupSemanticCandidate Candidate;
	TestFalse(TEXT("Same member count with wrong RuntimeId is rejected"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, MakeEnvelope(Baseline, WrongMembers, &Declaration), 0, Candidate));

	TArray<FPresentationRecord> Incomplete = WrongMembers;
	Incomplete.RemoveAt(2);
	TestFalse(TEXT("Missing tagged member is rejected"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, MakeEnvelope(Baseline, Incomplete, &Declaration), 0, Candidate));
	TestFalse(TEXT("Tagged records without declaration are rejected"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, MakeEnvelope(Baseline, WrongMembers, nullptr), 0, Candidate));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG2SingletonAndShapeTest,
	"SlayTheSpireDemo.SelectionPresentation.G2.SingletonAndShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG2SingletonAndShapeTest::RunTest(const FString& Parameters)
{
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>();
	const FPresentationStateSnapshot Baseline = MakeBaseline({ 51, 52 });
	const FPresentationGroupDeclaration Singleton = MakeDeclaration(5, { 51 });
	TArray<FPresentationRecord> SingletonRecords;
	SingletonRecords.Add(MakeZoneRecord(1, 51, ECardZone::Hand, ECardZone::ExhaustPile, 0, 0, &Singleton.Group));
	FPresentationGroupSemanticCandidate Candidate;
	TestFalse(TEXT("ExpectedMemberCount one remains SingleRecord"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, MakeEnvelope(Baseline, SingletonRecords, &Singleton), 0, Candidate));

	const FPresentationGroupDeclaration Pair = MakeDeclaration(6, { 51, 52 });
	TArray<FPresentationRecord> WrongShape;
	FPresentationRecord Played;
	Played.BattleId = BattleId;
	Played.ResolutionId = ResolutionId;
	Played.PresentationSequence = 1;
	Played.Type = EBattlePresentationRecordType::CardPlayed;
	Played.Group = Pair.Group;
	Played.CardPlayed.Card = MakeCardSnapshot(51);
	Played.CardPlayed.HandIndexBefore = 0;
	Played.CardPlayed.EnergyBefore = 3;
	Played.CardPlayed.EnergyAfter = 2;
	WrongShape.Add(Played);
	WrongShape.Add(MakeZoneRecord(2, 52, ECardZone::Hand, ECardZone::ExhaustPile, 0, 0, &Pair.Group));
	TestFalse(TEXT("CardPlayed cannot masquerade as SelectionDestination member"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, MakeEnvelope(Baseline, WrongShape, &Pair), 0, Candidate));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG2SequenceAndUnknownTest,
	"SlayTheSpireDemo.SelectionPresentation.G2.SequenceAndUnknown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG2SequenceAndUnknownTest::RunTest(const FString& Parameters)
{
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>();
	const FPresentationStateSnapshot Baseline = MakeBaseline({ 61, 62, 63 });
	const FPresentationGroupDeclaration Declaration = MakeDeclaration(7, { 61, 62, 63 });
	TArray<FPresentationRecord> DuplicateSequence;
	DuplicateSequence.Add(MakeZoneRecord(1, 61, ECardZone::Hand, ECardZone::ExhaustPile, 0, 0, &Declaration.Group));
	DuplicateSequence.Add(MakeZoneRecord(1, 62, ECardZone::Hand, ECardZone::ExhaustPile, 0, 1, &Declaration.Group));
	DuplicateSequence.Add(MakeZoneRecord(3, 63, ECardZone::Hand, ECardZone::ExhaustPile, 0, 2, &Declaration.Group));
	FPresentationGroupSemanticCandidate Candidate;
	TestFalse(TEXT("Non-increasing PresentationSequence rejects group"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, MakeEnvelope(Baseline, DuplicateSequence, &Declaration), 0, Candidate));

	TArray<FPresentationRecord> UnknownInterleave;
	UnknownInterleave.Add(MakeZoneRecord(1, 61, ECardZone::Hand, ECardZone::ExhaustPile, 0, 0, &Declaration.Group));
	FPresentationRecord Unknown;
	Unknown.BattleId = BattleId;
	Unknown.ResolutionId = ResolutionId;
	Unknown.PresentationSequence = 2;
	Unknown.Type = EBattlePresentationRecordType::None;
	UnknownInterleave.Add(Unknown);
	UnknownInterleave.Add(MakeZoneRecord(3, 62, ECardZone::Hand, ECardZone::ExhaustPile, 0, 1, &Declaration.Group));
	UnknownInterleave.Add(MakeZoneRecord(4, 63, ECardZone::Hand, ECardZone::ExhaustPile, 0, 2, &Declaration.Group));
	TestFalse(TEXT("Unknown interleaved record is conservatively rejected"), Controller->TryBuildSemanticPresentationGroupCandidateForTesting(Baseline, MakeEnvelope(Baseline, UnknownInterleave, &Declaration), 0, Candidate));
	return true;
}

#endif
