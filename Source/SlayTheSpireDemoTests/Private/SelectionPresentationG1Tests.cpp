#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Presentation/BattlePresentationRecorder.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG1RecorderMetadataTest,
	"SlayTheSpireDemo.SelectionPresentation.G1.RecorderMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG1RecorderMetadataTest::RunTest(const FString& Parameters)
{
	UBattlePresentationRecorder* Recorder = NewObject<UBattlePresentationRecorder>();
	if (!TestNotNull(TEXT("Recorder exists"), Recorder))
	{
		return false;
	}

	Recorder->ResetForBattle(7001);
	FPresentationRecordWriter Writer;
	if (!TestTrue(TEXT("Resolution begins"), Recorder->BeginResolution(EPresentationResolutionOrigin::PlayCard, Writer)))
	{
		return false;
	}

	TestTrue(TEXT("Writer is available"), Writer.IsAvailable());
	TestTrue(TEXT("Selection outcome receipt records on exact writer"), Writer.TryRecordSelectionOutcome(41));

	int64 FirstGroupId = 0;
	int64 SecondGroupId = 0;
	TestTrue(TEXT("First group id allocates"), Writer.TryAllocatePresentationGroupId(FirstGroupId));
	TestTrue(TEXT("Second group id allocates"), Writer.TryAllocatePresentationGroupId(SecondGroupId));
	TestTrue(TEXT("Group ids are positive"), FirstGroupId > 0 && SecondGroupId > 0);
	TestTrue(TEXT("Group ids are distinct inside one Resolution"), FirstGroupId != SecondGroupId);

	FPresentationGroupDeclaration Declaration;
	Declaration.Group.Kind = EPresentationGroupKind::SelectionDestination;
	Declaration.Group.GroupId = FirstGroupId;
	Declaration.Group.ExpectedMemberCount = 2;
	Declaration.CanonicalSelectedRuntimeIds = { 101, 202 };
	TestTrue(TEXT("Canonical declaration is accepted"), Writer.TryDeclarePresentationGroup(Declaration));

	FPresentationRecord Record;
	Record.Type = EBattlePresentationRecordType::CardZoneChanged;
	Record.Group = Declaration.Group;
	Record.CardZoneChanged.Card.RuntimeId = 101;
	Record.CardZoneChanged.Card.CardId = TEXT("G1Probe");
	Record.CardZoneChanged.FromZone = ECardZone::Hand;
	Record.CardZoneChanged.ToZone = ECardZone::ExhaustPile;
	Record.CardZoneChanged.FromIndex = 0;
	Record.CardZoneChanged.ToIndex = 0;
	TestTrue(TEXT("Tagged record appends"), Writer.Append(Record));

	FPresentationStateSnapshot Snapshot;
	Snapshot.BattleId = 7001;
	Snapshot.StateRevision = 42;
	FPresentationResolutionEnvelope Envelope;
	if (!TestTrue(TEXT("Resolution seals"), Recorder->SealResolution(Snapshot, Envelope)))
	{
		return false;
	}

	TestEqual(TEXT("One group declaration sealed"), Envelope.PresentationGroups.Num(), 1);
	TestEqual(TEXT("One selection receipt sealed"), Envelope.SelectionOutcomes.Num(), 1);
	if (Envelope.PresentationGroups.Num() == 1)
	{
		const FPresentationGroupDeclaration& SealedDeclaration = Envelope.PresentationGroups[0];
		TestEqual(TEXT("Sealed group id matches"), SealedDeclaration.Group.GroupId, FirstGroupId);
		TestEqual(TEXT("Canonical manifest count is preserved"), SealedDeclaration.CanonicalSelectedRuntimeIds.Num(), 2);
		if (SealedDeclaration.CanonicalSelectedRuntimeIds.Num() == 2)
		{
			TestEqual(TEXT("Canonical manifest first identity is preserved"), SealedDeclaration.CanonicalSelectedRuntimeIds[0], 101);
			TestEqual(TEXT("Canonical manifest second identity is preserved"), SealedDeclaration.CanonicalSelectedRuntimeIds[1], 202);
		}
	}
	if (Envelope.SelectionOutcomes.Num() == 1)
	{
		const FSelectionPresentationOutcomeReceipt& Receipt = Envelope.SelectionOutcomes[0];
		TestTrue(TEXT("Recorded receipt is valid"), Receipt.IsValid());
		TestEqual(TEXT("Receipt boundary is exact"), Receipt.SelectionBoundaryRevision, int64(41));
		TestEqual(TEXT("Receipt points to exact continuation Resolution"), Receipt.ResolutionId, Envelope.ResolutionId);
	}
	if (Envelope.Records.Num() == 1)
	{
		TestEqual(TEXT("Record retains declared group"), Envelope.Records[0].Group.GroupId, FirstGroupId);
	}

	int64 StaleGroupId = 0;
	TestFalse(TEXT("Sealed writer cannot allocate later group"), Writer.TryAllocatePresentationGroupId(StaleGroupId));

	FPresentationRecordWriter NewWriter;
	TestTrue(TEXT("New Resolution begins"), Recorder->BeginResolution(EPresentationResolutionOrigin::System, NewWriter));
	TestFalse(TEXT("Old writer remains stale in newer Resolution"), Writer.TryAllocatePresentationGroupId(StaleGroupId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG1ManifestValidationTest,
	"SlayTheSpireDemo.SelectionPresentation.G1.ManifestValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG1ManifestValidationTest::RunTest(const FString& Parameters)
{
	UBattlePresentationRecorder* Recorder = NewObject<UBattlePresentationRecorder>();
	if (!TestNotNull(TEXT("Recorder exists"), Recorder))
	{
		return false;
	}

	Recorder->ResetForBattle(7101);
	FPresentationRecordWriter Writer;
	if (!TestTrue(TEXT("Resolution begins"), Recorder->BeginResolution(EPresentationResolutionOrigin::PlayCard, Writer)))
	{
		return false;
	}

	int64 GroupId = 0;
	if (!TestTrue(TEXT("Group id allocates"), Writer.TryAllocatePresentationGroupId(GroupId)))
	{
		return false;
	}

	FPresentationGroupDeclaration CountMismatch;
	CountMismatch.Group.Kind = EPresentationGroupKind::SelectionDestination;
	CountMismatch.Group.GroupId = GroupId;
	CountMismatch.Group.ExpectedMemberCount = 2;
	CountMismatch.CanonicalSelectedRuntimeIds = { 301 };
	TestFalse(TEXT("Count-only declaration cannot omit selected identity"), Writer.TryDeclarePresentationGroup(CountMismatch));

	FPresentationGroupDeclaration DuplicateManifest;
	DuplicateManifest.Group.Kind = EPresentationGroupKind::SelectionDestination;
	DuplicateManifest.Group.GroupId = GroupId;
	DuplicateManifest.Group.ExpectedMemberCount = 2;
	DuplicateManifest.CanonicalSelectedRuntimeIds = { 301, 301 };
	TestFalse(TEXT("Duplicate RuntimeId manifest is rejected"), Writer.TryDeclarePresentationGroup(DuplicateManifest));

	FPresentationGroupDeclaration InvalidRuntimeId;
	InvalidRuntimeId.Group.Kind = EPresentationGroupKind::SelectionDestination;
	InvalidRuntimeId.Group.GroupId = GroupId;
	InvalidRuntimeId.Group.ExpectedMemberCount = 1;
	InvalidRuntimeId.CanonicalSelectedRuntimeIds = { INDEX_NONE };
	TestFalse(TEXT("Invalid RuntimeId manifest is rejected"), Writer.TryDeclarePresentationGroup(InvalidRuntimeId));

	FPresentationGroupDeclaration Valid;
	Valid.Group.Kind = EPresentationGroupKind::SelectionDestination;
	Valid.Group.GroupId = GroupId;
	Valid.Group.ExpectedMemberCount = 2;
	Valid.CanonicalSelectedRuntimeIds = { 301, 302 };
	TestTrue(TEXT("Exact canonical manifest is accepted"), Writer.TryDeclarePresentationGroup(Valid));
	TestFalse(TEXT("Group declaration cannot be duplicated"), Writer.TryDeclarePresentationGroup(Valid));

	FPresentationRecord MalformedTaggedRecord;
	MalformedTaggedRecord.Type = EBattlePresentationRecordType::EnergyChanged;
	MalformedTaggedRecord.Group.Kind = EPresentationGroupKind::SelectionDestination;
	MalformedTaggedRecord.Group.GroupId = GroupId + 99;
	MalformedTaggedRecord.Group.ExpectedMemberCount = 1;
	MalformedTaggedRecord.EnergyChanged.EnergyBefore = 3;
	MalformedTaggedRecord.EnergyChanged.EnergyAfter = 2;
	MalformedTaggedRecord.EnergyChanged.Delta = -1;
	TestTrue(TEXT("Malformed optional tag does not destroy serial record"), Writer.Append(MalformedTaggedRecord));

	FPresentationStateSnapshot Snapshot;
	Snapshot.BattleId = 7101;
	Snapshot.StateRevision = 2;
	FPresentationResolutionEnvelope Envelope;
	if (!TestTrue(TEXT("Resolution seals with valid serial history"), Recorder->SealResolution(Snapshot, Envelope)))
	{
		return false;
	}
	if (Envelope.Records.Num() == 1)
	{
		TestTrue(TEXT("Malformed optional group tag is stripped"), Envelope.Records[0].Group.Kind == EPresentationGroupKind::None);
	}
	TestEqual(TEXT("Valid manifest remains sealed"), Envelope.PresentationGroups.Num(), 1);
	return true;
}

#endif
