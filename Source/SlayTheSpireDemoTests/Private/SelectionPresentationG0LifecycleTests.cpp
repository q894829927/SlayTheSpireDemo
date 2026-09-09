#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG0LifecycleTest
{
	FPresentationStateSnapshot MakeSnapshot()
	{
		FPresentationStateSnapshot Snapshot;
		Snapshot.BattleId = 701;
		Snapshot.StateRevision = 15;
		Snapshot.BattleState = EBattleState::PlayerTurn;
		Snapshot.Outcome = EBattleHUDOutcome::None;
		Snapshot.Energy = 3;
		Snapshot.MaxEnergy = 3;
		Snapshot.Player.PresentationId = TEXT("Player");
		Snapshot.Player.bPlayer = true;
		Snapshot.Player.HP = 80;
		Snapshot.Player.MaxHP = 80;
		Snapshot.Enemy.PresentationId = TEXT("Enemy");
		Snapshot.Enemy.HP = 40;
		Snapshot.Enemy.MaxHP = 40;
		for (const int32 RuntimeId : { 71, 72 })
		{
			FBattleHUDCardView Card;
			Card.RuntimeId = RuntimeId;
			Card.CardId = FName(*FString::Printf(TEXT("Cancel_%d"), RuntimeId));
			Card.DisplayName = FText::FromString(TEXT("Cancel Probe"));
			Card.Cost = 1;
			Card.CardType = ECardType::Skill;
			Card.Rarity = ECardRarity::Common;
			Card.CardColor = ECardColor::Red;
			Card.TargetType = ECardTargetType::None;
			Snapshot.HandCards.Add(Card);
		}
		return Snapshot;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG0PendingLifecycleCancelTest,
	"SlayTheSpireDemo.SelectionPresentation.G0.PendingLifecycleCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSelectionPresentationG0PendingLifecycleCancelTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG0LifecycleTest;
	(void)Parameters;

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>();
	if (!TestNotNull(TEXT("ViewModel should be created."), ViewModel)) return false;
	ViewModel->ApplyPresentationSnapshot(MakeSnapshot(), true);

	TArray<int32> ChangedRuntimeIds;
	ViewModel->OnCardPresentationOwnershipChanged.AddLambda(
		[&ChangedRuntimeIds](const TArray<int32>& RuntimeIds)
		{
			ChangedRuntimeIds.Append(RuntimeIds);
		});

	const int64 Generation = ViewModel->BeginCardPresentationSelectionLifecycle(15);
	TestTrue(TEXT("Pending lifecycle begins."), Generation > 0);
	TestTrue(TEXT("First Pending owner is established."),
		ViewModel->SetPendingCardPresentationSelection(Generation, 71, true));
	TestTrue(TEXT("Second Pending owner is established."),
		ViewModel->SetPendingCardPresentationSelection(Generation, 72, true));
	ChangedRuntimeIds.Reset();

	TestTrue(TEXT("Exact active Pending lifecycle can cancel."),
		ViewModel->CancelCardPresentationSelectionLifecycle(Generation));
	TestEqual(TEXT("Cancelled first card returns to implicit Hand owner."),
		ViewModel->GetCardPresentationOwner(71), ECardPresentationOwner::Hand);
	TestEqual(TEXT("Cancelled second card returns to implicit Hand owner."),
		ViewModel->GetCardPresentationOwner(72), ECardPresentationOwner::Hand);
	TestTrue(TEXT("Cancel ownership event includes first RuntimeId."), ChangedRuntimeIds.Contains(71));
	TestTrue(TEXT("Cancel ownership event includes second RuntimeId."), ChangedRuntimeIds.Contains(72));
	TestFalse(TEXT("Stale cancelled generation cannot mutate ownership."),
		ViewModel->SetPendingCardPresentationSelection(Generation, 71, true));
	TestTrue(TEXT("Cancellation releases the active generation gate."),
		ViewModel->BeginCardPresentationSelectionLifecycle(15) > 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
