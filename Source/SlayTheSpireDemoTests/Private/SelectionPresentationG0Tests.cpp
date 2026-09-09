#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG0Test
{
	FPresentationStateSnapshot MakeSnapshot(
		int64 BattleId,
		int64 StateRevision,
		std::initializer_list<int32> HandRuntimeIds)
	{
		FPresentationStateSnapshot Snapshot;
		Snapshot.BattleId = BattleId;
		Snapshot.StateRevision = StateRevision;
		Snapshot.BattleState = EBattleState::PlayerTurn;
		Snapshot.Outcome = EBattleHUDOutcome::None;
		Snapshot.Energy = 3;
		Snapshot.MaxEnergy = 3;
		Snapshot.bCanEndTurn = true;
		Snapshot.Player.PresentationId = TEXT("Player");
		Snapshot.Player.bPlayer = true;
		Snapshot.Player.DisplayName = FText::FromString(TEXT("Player"));
		Snapshot.Player.HP = 80;
		Snapshot.Player.MaxHP = 80;
		Snapshot.Enemy.PresentationId = TEXT("Enemy");
		Snapshot.Enemy.DisplayName = FText::FromString(TEXT("Enemy"));
		Snapshot.Enemy.HP = 40;
		Snapshot.Enemy.MaxHP = 40;

		for (const int32 RuntimeId : HandRuntimeIds)
		{
			FBattleHUDCardView Card;
			Card.RuntimeId = RuntimeId;
			Card.CardId = FName(*FString::Printf(TEXT("G0_%d"), RuntimeId));
			Card.DisplayName = FText::FromString(FString::Printf(TEXT("Card%d"), RuntimeId));
			Card.Cost = 1;
			Card.CardType = ECardType::Skill;
			Card.Rarity = ECardRarity::Common;
			Card.CardColor = ECardColor::Red;
			Card.TargetType = ECardTargetType::None;
			Snapshot.HandCards.Add(Card);
		}
		return Snapshot;
	}

	bool HasFlag(EBattleHUDDirtyFlags Value, EBattleHUDDirtyFlags Flag)
	{
		return EnumHasAnyFlags(Value, Flag);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG0DirtyPropagationTest,
	"SlayTheSpireDemo.SelectionPresentation.G0.DirtyPropagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSelectionPresentationG0DirtyPropagationTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG0Test;
	(void)Parameters;

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>();
	TestNotNull(TEXT("ViewModel should be created."), ViewModel);
	if (!IsValid(ViewModel)) return false;

	TArray<EBattleHUDDirtyFlags> Publications;
	ViewModel->OnNativeChanged.AddLambda(
		[&Publications](EBattleHUDDirtyFlags DirtyFlags)
		{
			Publications.Add(DirtyFlags);
		});

	FPresentationStateSnapshot Baseline = MakeSnapshot(101, 1, { 11, 12 });
	ViewModel->ApplyPresentationSnapshot(Baseline, true);
	TestTrue(TEXT("First battle snapshot is a full publication."),
		Publications.Num() == 1 && Publications.Last() == EBattleHUDDirtyFlags::All);

	FPresentationStateSnapshot EnergyOnly = Baseline;
	EnergyOnly.StateRevision = 2;
	EnergyOnly.Energy = 2;
	ViewModel->ApplyPresentationSnapshot(EnergyOnly, true);
	const EBattleHUDDirtyFlags EnergyFlags = Publications.Last();
	TestTrue(TEXT("Energy change marks Energy."), HasFlag(EnergyFlags, EBattleHUDDirtyFlags::Energy));
	TestFalse(TEXT("Energy change must not mark Hand."), HasFlag(EnergyFlags, EBattleHUDDirtyFlags::Hand));
	TestFalse(TEXT("Energy change must not mark Statuses."), HasFlag(EnergyFlags, EBattleHUDDirtyFlags::Statuses));
	TestFalse(TEXT("Energy change must not mark PileCounts."), HasFlag(EnergyFlags, EBattleHUDDirtyFlags::PileCounts));

	FPresentationStateSnapshot StatusOnly = EnergyOnly;
	StatusOnly.StateRevision = 3;
	FBattleHUDStatusView Status;
	Status.StatusId = TEXT("G0Status");
	Status.RuntimeSequence = 1;
	Status.DisplayName = FText::FromString(TEXT("Status"));
	Status.Description = FText::FromString(TEXT("Status description"));
	Status.Amount = 1;
	StatusOnly.Player.Statuses.Add(Status);
	ViewModel->ApplyPresentationSnapshot(StatusOnly, true);
	const EBattleHUDDirtyFlags StatusFlags = Publications.Last();
	TestTrue(TEXT("Status change marks Statuses."), HasFlag(StatusFlags, EBattleHUDDirtyFlags::Statuses));
	TestFalse(TEXT("Status change must not mark Hand."), HasFlag(StatusFlags, EBattleHUDDirtyFlags::Hand));

	FPresentationStateSnapshot HandChanged = StatusOnly;
	HandChanged.StateRevision = 4;
	HandChanged.HandCards.RemoveAt(0);
	ViewModel->ApplyPresentationSnapshot(HandChanged, true);
	TestTrue(TEXT("Actual Hand change marks Hand."),
		HasFlag(Publications.Last(), EBattleHUDDirtyFlags::Hand));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG0OwnershipEventAndConfirmTest,
	"SlayTheSpireDemo.SelectionPresentation.G0.OwnershipEventAndConfirm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSelectionPresentationG0OwnershipEventAndConfirmTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG0Test;
	(void)Parameters;

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>();
	if (!TestNotNull(TEXT("ViewModel should be created."), ViewModel)) return false;
	ViewModel->ApplyPresentationSnapshot(MakeSnapshot(202, 10, { 21, 22 }), true);

	int32 NativePublicationCount = 0;
	int32 OwnershipPublicationCount = 0;
	TArray<int32> LastOwnershipRuntimeIds;
	ViewModel->OnNativeChanged.AddLambda(
		[&NativePublicationCount](EBattleHUDDirtyFlags)
		{
			++NativePublicationCount;
		});
	ViewModel->OnCardPresentationOwnershipChanged.AddLambda(
		[&OwnershipPublicationCount, &LastOwnershipRuntimeIds](const TArray<int32>& RuntimeIds)
		{
			++OwnershipPublicationCount;
			LastOwnershipRuntimeIds = RuntimeIds;
		});

	const int64 Generation = ViewModel->BeginCardPresentationSelectionLifecycle(10);
	TestTrue(TEXT("Selection lifecycle gets a generation."), Generation > 0);
	TestFalse(TEXT("A second Pending lifecycle cannot overlap."),
		ViewModel->BeginCardPresentationSelectionLifecycle(10) > 0);
	TestFalse(TEXT("A RuntimeId absent from frozen Hand cannot enter SelectionArea."),
		ViewModel->SetPendingCardPresentationSelection(Generation, 999, true));

	TestTrue(TEXT("Selecting first card enters SelectionArea."),
		ViewModel->SetPendingCardPresentationSelection(Generation, 21, true));
	TestEqual(TEXT("Ownership mutation has no historical Native publication."), NativePublicationCount, 0);
	TestEqual(TEXT("Ownership mutation publishes its own event."), OwnershipPublicationCount, 1);
	TestTrue(TEXT("Ownership event names the exact RuntimeId."),
		LastOwnershipRuntimeIds.Num() == 1 && LastOwnershipRuntimeIds[0] == 21);

	FCardPresentationOwnershipEntry Entry;
	TestTrue(TEXT("Selected card has explicit ownership entry."),
		ViewModel->TryGetCardPresentationOwnershipEntry(21, Entry));
	TestEqual(TEXT("Selected card owner is SelectionArea."), Entry.Owner, ECardPresentationOwner::SelectionArea);
	TestEqual(TEXT("Selected card phase is Pending."), Entry.Phase, ESelectionPresentationVisualPhase::Pending);

	TestTrue(TEXT("Deselect restores implicit Hand owner."),
		ViewModel->SetPendingCardPresentationSelection(Generation, 21, false));
	TestEqual(TEXT("Deselect removes explicit owner."),
		ViewModel->GetCardPresentationOwner(21), ECardPresentationOwner::Hand);

	TestTrue(TEXT("First card can be selected again."),
		ViewModel->SetPendingCardPresentationSelection(Generation, 21, true));
	TestTrue(TEXT("Second card can be selected."),
		ViewModel->SetPendingCardPresentationSelection(Generation, 22, true));
	TestTrue(TEXT("Confirm freezes the exact selected set."),
		ViewModel->ConfirmCardPresentationSelection(Generation, { 21, 22 }));
	TestFalse(TEXT("Confirmed generation is closed to stale pending mutation."),
		ViewModel->SetPendingCardPresentationSelection(Generation, 21, false));
	TestTrue(TEXT("A fresh lifecycle can begin after Confirm closes the interactive gate."),
		ViewModel->BeginCardPresentationSelectionLifecycle(10) > 0);

	TestTrue(TEXT("Confirmed entry remains explicit."),
		ViewModel->TryGetCardPresentationOwnershipEntry(21, Entry));
	TestEqual(TEXT("Confirmed owner remains SelectionArea."), Entry.Owner, ECardPresentationOwner::SelectionArea);
	TestEqual(TEXT("Confirmed phase is frozen."), Entry.Phase, ESelectionPresentationVisualPhase::Confirmed);
	TestEqual(TEXT("Confirmed completion starts unresolved."),
		Entry.CompletionWatermark.Mode,
		ESelectionPresentationCompletionMode::Unresolved);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG0CompletionWatermarkTest,
	"SlayTheSpireDemo.SelectionPresentation.G0.CompletionWatermark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSelectionPresentationG0CompletionWatermarkTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG0Test;
	(void)Parameters;

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>();
	if (!TestNotNull(TEXT("ViewModel should be created."), ViewModel)) return false;
	ViewModel->ApplyPresentationSnapshot(MakeSnapshot(303, 20, { 31 }), true);

	const int64 DirectGeneration = ViewModel->BeginCardPresentationSelectionLifecycle(20);
	TestTrue(TEXT("Direct lifecycle begins."), DirectGeneration > 0);
	TestTrue(TEXT("Direct card enters SelectionArea."),
		ViewModel->SetPendingCardPresentationSelection(DirectGeneration, 31, true));
	TestTrue(TEXT("Direct card confirms."),
		ViewModel->ConfirmCardPresentationSelection(DirectGeneration, { 31 }));
	TestFalse(TEXT("Boundary revision itself cannot be the post-Confirm watermark."),
		ViewModel->ArmDirectCardPresentationCompletion(DirectGeneration, 20));
	TestTrue(TEXT("Strictly newer state revision can arm Direct completion."),
		ViewModel->ArmDirectCardPresentationCompletion(DirectGeneration, 21));
	TestFalse(TEXT("Direct watermark cannot be overwritten by another revision."),
		ViewModel->ArmDirectCardPresentationCompletion(DirectGeneration, 22));
	TestFalse(TEXT("Direct watermark cannot be overwritten by Recorded mode."),
		ViewModel->ArmRecordedCardPresentationCompletion(DirectGeneration, 700));
	TestEqual(TEXT("Unreached Direct watermark keeps SelectionArea owner."),
		ViewModel->GetCardPresentationOwner(31), ECardPresentationOwner::SelectionArea);

	ViewModel->ApplyPresentationSnapshot(MakeSnapshot(303, 21, { 31 }), true);
	TestEqual(TEXT("Exact Direct revision restores SelectionArea owner to Hand."),
		ViewModel->GetCardPresentationOwner(31), ECardPresentationOwner::Hand);

	const int64 RecordedGeneration = ViewModel->BeginCardPresentationSelectionLifecycle(21);
	TestTrue(TEXT("Recorded lifecycle begins."), RecordedGeneration > 0);
	TestTrue(TEXT("Recorded card enters SelectionArea."),
		ViewModel->SetPendingCardPresentationSelection(RecordedGeneration, 31, true));
	TestTrue(TEXT("Recorded card confirms."),
		ViewModel->ConfirmCardPresentationSelection(RecordedGeneration, { 31 }));
	TestTrue(TEXT("Recorded completion arms exact ResolutionId."),
		ViewModel->ArmRecordedCardPresentationCompletion(RecordedGeneration, 701));
	TestTrue(TEXT("Re-arming exact same Recorded watermark is idempotent."),
		ViewModel->ArmRecordedCardPresentationCompletion(RecordedGeneration, 701));
	TestFalse(TEXT("Recorded watermark cannot be replaced by another ResolutionId."),
		ViewModel->ArmRecordedCardPresentationCompletion(RecordedGeneration, 702));
	ViewModel->MarkPresentationResolutionCompleted(999, 701);
	TestEqual(TEXT("Wrong BattleId completion does not restore owner."),
		ViewModel->GetCardPresentationOwner(31), ECardPresentationOwner::SelectionArea);
	ViewModel->MarkPresentationResolutionCompleted(303, 701);
	TestEqual(TEXT("Exact recorded Resolution completion restores owner."),
		ViewModel->GetCardPresentationOwner(31), ECardPresentationOwner::Hand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG0StaleAndTransitionTest,
	"SlayTheSpireDemo.SelectionPresentation.G0.StaleAndTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSelectionPresentationG0StaleAndTransitionTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG0Test;
	(void)Parameters;

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>();
	if (!TestNotNull(TEXT("ViewModel should be created."), ViewModel)) return false;
	ViewModel->ApplyPresentationSnapshot(MakeSnapshot(404, 30, { 41, 42 }), true);

	const int64 StaleGeneration = ViewModel->BeginCardPresentationSelectionLifecycle(30);
	TestTrue(TEXT("Pending lifecycle begins."), StaleGeneration > 0);
	TestTrue(TEXT("Pending owner is created."),
		ViewModel->SetPendingCardPresentationSelection(StaleGeneration, 41, true));
	ViewModel->ApplyPresentationSnapshot(MakeSnapshot(404, 31, { 41, 42 }), true);
	TestEqual(TEXT("Revision replacement retires stale Pending owner."),
		ViewModel->GetCardPresentationOwner(41), ECardPresentationOwner::Hand);
	TestFalse(TEXT("Old generation cannot mutate newer revision."),
		ViewModel->SetPendingCardPresentationSelection(StaleGeneration, 41, true));
	const int64 FreshGeneration = ViewModel->BeginCardPresentationSelectionLifecycle(31);
	TestTrue(TEXT("Stale Pending gate does not block a fresh lifecycle."), FreshGeneration > 0);
	TestTrue(TEXT("Fresh card enters SelectionArea."),
		ViewModel->SetPendingCardPresentationSelection(FreshGeneration, 42, true));
	TestTrue(TEXT("Fresh card confirms."),
		ViewModel->ConfirmCardPresentationSelection(FreshGeneration, { 42 }));
	TestTrue(TEXT("Confirmed owner can transfer SelectionArea -> Transition."),
		ViewModel->TryTransferCardPresentationOwnership(
			FreshGeneration,
			42,
			ECardPresentationOwner::SelectionArea,
			ECardPresentationOwner::Transition));
	TestFalse(TEXT("Transition cannot regress directly back to SelectionArea."),
		ViewModel->TryTransferCardPresentationOwnership(
			FreshGeneration,
			42,
			ECardPresentationOwner::Transition,
			ECardPresentationOwner::SelectionArea));
	TestTrue(TEXT("Transition advances only to ConsumedPendingReducer."),
		ViewModel->TryTransferCardPresentationOwnership(
			FreshGeneration,
			42,
			ECardPresentationOwner::Transition,
			ECardPresentationOwner::ConsumedPendingReducer));
	TestFalse(TEXT("ConsumedPendingReducer cannot regress into Transition."),
		ViewModel->TryTransferCardPresentationOwnership(
			FreshGeneration,
			42,
			ECardPresentationOwner::ConsumedPendingReducer,
			ECardPresentationOwner::Transition));
	TestTrue(TEXT("Consumed owner can still arm Direct completion."),
		ViewModel->ArmDirectCardPresentationCompletion(FreshGeneration, 32));
	ViewModel->ApplyPresentationSnapshot(MakeSnapshot(404, 32, { 41, 42 }), true);
	TestEqual(TEXT("Formal lifecycle completion fail-safe restores even Consumed owner."),
		ViewModel->GetCardPresentationOwner(42), ECardPresentationOwner::Hand);

	const int64 AbsenceGeneration = ViewModel->BeginCardPresentationSelectionLifecycle(32);
	TestTrue(TEXT("Absence lifecycle begins."), AbsenceGeneration > 0);
	TestTrue(TEXT("Card enters SelectionArea before authoritative absence."),
		ViewModel->SetPendingCardPresentationSelection(AbsenceGeneration, 42, true));
	TestTrue(TEXT("Card confirms before authoritative absence."),
		ViewModel->ConfirmCardPresentationSelection(AbsenceGeneration, { 42 }));
	TestTrue(TEXT("Card transfers to Transition before authoritative absence."),
		ViewModel->TryTransferCardPresentationOwnership(
			AbsenceGeneration,
			42,
			ECardPresentationOwner::SelectionArea,
			ECardPresentationOwner::Transition));
	ViewModel->ApplyPresentationSnapshot(MakeSnapshot(404, 33, { 41 }), true);
	TestEqual(TEXT("Authoritative absence clears even unresolved Transition owner."),
		ViewModel->GetCardPresentationOwner(42), ECardPresentationOwner::Hand);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
