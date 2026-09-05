#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/FinishCardPlayAction.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Deck/DeckRuntime.h"
#include "Events/BattleEvent.h"
#include "Events/BattleEventDispatcher.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1AExhaustWiringFailureBeforeCommitTest,
	"SlayTheSpireDemo.CardExpansion.Wave1A.ExhaustFact.WiringFailureBeforeCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1AExhaustWiringFailureBeforeCommitTest::RunTest(const FString& Parameters)
{
	UCardData* Definition = NewObject<UCardData>(GetTransientPackage());
	Definition->CardId = TEXT("Wave1AWiringFailure");
	Definition->DisplayName = FText::FromString(TEXT("Wave1AWiringFailure"));
	Definition->Description = FText::FromString(TEXT("Test."));
	Definition->BaseCost = 0;
	Definition->UpgradedCost = 0;
	Definition->TargetType = ECardTargetType::None;
	Definition->DefaultDestination = ECardDestination::Exhaust;

	UDeckRuntime* Deck = NewObject<UDeckRuntime>(GetTransientPackage());
	TArray<TObjectPtr<UCardData>> Definitions;
	Definitions.Add(Definition);
	Deck->InitializeFromDefinitions(Definitions, 1337);

	UCardInstance* Card = nullptr;
	TestTrue(TEXT("Draw commits"), Deck->TryDrawTopCardCommit(Card).bCommitted);
	if (!TestNotNull(TEXT("Runtime card exists"), Card))
	{
		return false;
	}
	TestTrue(TEXT("Hand to PlayArea commits"), Deck->TryMoveHandCardToPlayAreaCommit(Card).bCommitted);

	int32 ExhaustEventCount = 0;
	UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
	UBattleEventDispatcher::OnEventDispatchedForTesting.AddLambda(
		[&](const FBattleEvent& Event)
		{
			if (Event.TryGet<FCardExhaustedEvent>() != nullptr)
			{
				++ExhaustEventCount;
			}
		}
	);

	UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(GetTransientPackage());
	UFinishCardPlayAction* FinishAction = NewObject<UFinishCardPlayAction>(Queue);
	FinishAction->Initialize(Deck, Card);
	TestTrue(TEXT("Finish action enqueues"), Queue->AddToBack(FinishAction));
	Queue->StartProcessing();
	UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();

	TestTrue(TEXT("Missing event wiring faults resolution"), Queue->IsResolutionFaulted());
	TestTrue(TEXT("Card remains in PlayArea when wiring is invalid"), Deck->IsCardInPlayArea(Card));
	TestEqual(TEXT("No Exhaust commit occurs"), Deck->GetExhaustCount(), 0);
	TestEqual(TEXT("No CardExhausted event is emitted"), ExhaustEventCount, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
