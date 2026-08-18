#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Actions/BattleActionQueue.h"
#include "../Actions/DrawCardAction.h"
#include "../Actions/ShuffleDeckAction.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEvent.h"
#include "../Events/BattleEventDispatcher.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusData.h"
#include "Phase6ATestTypes.h"
#include "Engine/World.h"
#include "UObject/Package.h"

namespace Phase6CRegression
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Combatant = nullptr;
		UBattleActionQueue* Queue = nullptr;
		UBattleEventDispatcher* Dispatcher = nullptr;
		UDeckRuntime* Deck = nullptr;
		UPhase6ATestExecutionRecorder* Recorder = nullptr;
		TArray<ACombatant*> Combatants;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Combatant = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			if (IsValid(Combatant))
			{
				Combatant->MaxHP = 100;
				Combatant->InitializeCombatant();
				Combatants.Add(Combatant);
			}

			Queue = NewObject<UBattleActionQueue>(World);
			Dispatcher = NewObject<UBattleEventDispatcher>(World);
			Deck = NewObject<UDeckRuntime>(World);
			Recorder = NewObject<UPhase6ATestExecutionRecorder>(World);
		}

		~FFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		bool IsReady() const
		{
			return IsValid(World)
				&& IsValid(Combatant)
				&& IsValid(Combatant->GetStatusContainer())
				&& IsValid(Queue)
				&& IsValid(Dispatcher)
				&& IsValid(Deck)
				&& IsValid(Recorder)
				&& Combatants.Num() == 1;
		}
	};

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (!Fixture.IsReady())
		{
			Test.AddError(TEXT("Failed to create the transient Phase 6C fixture."));
			return false;
		}
		return true;
	}

	UCardData* CreateCardDefinition(UObject* Outer, const TCHAR* CardId)
	{
		UCardData* Definition = NewObject<UCardData>(Outer);
		Definition->CardId = FName(CardId);
		Definition->BaseCost = 0;
		Definition->TargetType = ECardTargetType::None;
		return Definition;
	}

	void InitializeDeck(UDeckRuntime* Deck, UObject* DefinitionOuter, int32 CardCount)
	{
		TArray<TObjectPtr<UCardData>> Definitions;
		for (int32 Index = 0; Index < CardCount; ++Index)
		{
			Definitions.Add(CreateCardDefinition(
				DefinitionOuter,
				*FString::Printf(TEXT("Phase6C_Card_%d"), Index + 1)
			));
		}
		Deck->InitializeFromDefinitions(Definitions, 1337);
	}

	bool MoveOneCardToDiscard(UDeckRuntime* Deck)
	{
		UCardInstance* Card = nullptr;
		return Deck->TryDrawTopCard(Card)
			&& IsValid(Card)
			&& Deck->TryDiscardCard(Card);
	}

	bool AddDeckShuffledRecordStatus(FFixture& Fixture)
	{
		UStatusData* Definition = NewObject<UStatusData>(Fixture.World);
		Definition->StatusId = FName(TEXT("Phase6CDeckObserver"));

		UPhase6ATestRecordTrigger* Trigger = NewObject<UPhase6ATestRecordTrigger>(Definition);
		Trigger->Priority = 0;
		Trigger->InitializeForDeckShuffled(Fixture.Recorder, Fixture.Deck);
		Definition->Triggers.Add(Trigger);

		bool bCreated = false;
		return IsValid(Fixture.Combatant->GetStatusContainer()->ApplyStatus(Definition, 1, 1, bCreated)) && bCreated;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6CDeckShuffledTypedPayloadIsolationTest,
		"SlayTheSpireDemo.Phase6C.Event.TypedPayloadIsolation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6CDeckShuffledTypedPayloadIsolationTest::RunTest(const FString& Parameters)
	{
		UDeckRuntime* Deck = NewObject<UDeckRuntime>(GetTransientPackage());
		FBattleEvent ShuffledEvent = FBattleEvent::MakeDeckShuffled(Deck);

		const FDeckShuffledEvent* ShuffledPayload = ShuffledEvent.TryGet<FDeckShuffledEvent>();
		TestNotNull(TEXT("DeckShuffled payload is accessible from a DeckShuffled event"), ShuffledPayload);
		TestTrue(TEXT("DeckShuffled payload preserves the exact Deck"), ShuffledPayload && ShuffledPayload->Deck == Deck);
		TestNull(TEXT("TurnEnded payload is not accessible from a DeckShuffled event"), ShuffledEvent.TryGet<FTurnEndedEvent>());

		FBattleEvent TurnEndedEvent = FBattleEvent::MakeTurnEnded(nullptr);
		TestNotNull(TEXT("TurnEnded payload is accessible from a TurnEnded event"), TurnEndedEvent.TryGet<FTurnEndedEvent>());
		TestNull(TEXT("DeckShuffled payload is not accessible from a TurnEnded event"), TurnEndedEvent.TryGet<FDeckShuffledEvent>());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6CSuccessfulShuffleEmitsAfterCommitTest,
		"SlayTheSpireDemo.Phase6C.Shuffle.SuccessEmitsAfterCommit",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6CSuccessfulShuffleEmitsAfterCommitTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		InitializeDeck(Fixture.Deck, Fixture.World, 1);
		TestTrue(TEXT("Test card moved to Discard before shuffle"), MoveOneCardToDiscard(Fixture.Deck));
		TestTrue(TEXT("DeckShuffled observer status created"), AddDeckShuffledRecordStatus(Fixture));

		UShuffleDeckAction* Shuffle = NewObject<UShuffleDeckAction>(Fixture.Queue);
		Shuffle->Initialize(Fixture.Deck, Fixture.Dispatcher, Fixture.Combatants);
		TestTrue(TEXT("Shuffle action enqueued"), Fixture.Queue->AddToBack(Shuffle));
		TestTrue(TEXT("Shuffle resolution started"), Fixture.Queue->StartProcessing());

		const TArray<int32>& Values = Fixture.Recorder->GetValues();
		TestEqual(TEXT("Successful shuffle emitted exactly one reaction"), Values.Num(), 1);
		if (Values.Num() == 1)
		{
			TestEqual(TEXT("Reaction observes DrawPile after shuffle commit"), Values[0], 1);
		}
		TestEqual(TEXT("Successful shuffle moved one card into DrawPile"), Fixture.Deck->GetDrawCount(), 1);
		TestEqual(TEXT("Successful shuffle emptied DiscardPile"), Fixture.Deck->GetDiscardCount(), 0);
		TestFalse(TEXT("Successful shuffle event path does not fault"), Fixture.Queue->IsResolutionFaulted());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6CEmptyDiscardDoesNotEmitTest,
		"SlayTheSpireDemo.Phase6C.Shuffle.EmptyDiscardDoesNotEmit",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6CEmptyDiscardDoesNotEmitTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		InitializeDeck(Fixture.Deck, Fixture.World, 1);
		TestTrue(TEXT("DeckShuffled observer status created"), AddDeckShuffledRecordStatus(Fixture));

		UShuffleDeckAction* Shuffle = NewObject<UShuffleDeckAction>(Fixture.Queue);
		Shuffle->Initialize(Fixture.Deck, Fixture.Dispatcher, Fixture.Combatants);
		TestTrue(TEXT("No-op shuffle action enqueued"), Fixture.Queue->AddToBack(Shuffle));
		TestTrue(TEXT("No-op shuffle resolution started"), Fixture.Queue->StartProcessing());

		TestEqual(TEXT("Empty Discard shuffle emits no reaction"), Fixture.Recorder->GetValues().Num(), 0);
		TestEqual(TEXT("DrawPile remains unchanged"), Fixture.Deck->GetDrawCount(), 1);
		TestEqual(TEXT("DiscardPile remains empty"), Fixture.Deck->GetDiscardCount(), 0);
		TestFalse(TEXT("Expected shuffle no-op does not fault"), Fixture.Queue->IsResolutionFaulted());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6CNonEmptyDrawPileDoesNotEmitTest,
		"SlayTheSpireDemo.Phase6C.Shuffle.NonEmptyDrawPileDoesNotEmit",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6CNonEmptyDrawPileDoesNotEmitTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		InitializeDeck(Fixture.Deck, Fixture.World, 2);
		TestTrue(TEXT("One card moved to Discard while another remains in DrawPile"), MoveOneCardToDiscard(Fixture.Deck));
		TestEqual(TEXT("Precondition DrawPile count"), Fixture.Deck->GetDrawCount(), 1);
		TestEqual(TEXT("Precondition DiscardPile count"), Fixture.Deck->GetDiscardCount(), 1);
		TestTrue(TEXT("DeckShuffled observer status created"), AddDeckShuffledRecordStatus(Fixture));

		UShuffleDeckAction* Shuffle = NewObject<UShuffleDeckAction>(Fixture.Queue);
		Shuffle->Initialize(Fixture.Deck, Fixture.Dispatcher, Fixture.Combatants);
		TestTrue(TEXT("Rejected-by-deck shuffle action enqueued"), Fixture.Queue->AddToBack(Shuffle));
		TestTrue(TEXT("Rejected-by-deck shuffle resolution started"), Fixture.Queue->StartProcessing());

		TestEqual(TEXT("Shuffle with non-empty DrawPile emits no reaction"), Fixture.Recorder->GetValues().Num(), 0);
		TestEqual(TEXT("DrawPile stays unchanged"), Fixture.Deck->GetDrawCount(), 1);
		TestEqual(TEXT("DiscardPile stays unchanged"), Fixture.Deck->GetDiscardCount(), 1);
		TestFalse(TEXT("Expected shuffle rejection does not fault"), Fixture.Queue->IsResolutionFaulted());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6CReactionBeforeRetryDrawTest,
		"SlayTheSpireDemo.Phase6C.Draw.ShuffleReactionBeforeRetryDraw",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6CReactionBeforeRetryDrawTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		InitializeDeck(Fixture.Deck, Fixture.World, 1);
		TestTrue(TEXT("Test card moved to Discard before draw retry flow"), MoveOneCardToDiscard(Fixture.Deck));
		TestTrue(TEXT("DeckShuffled observer status created"), AddDeckShuffledRecordStatus(Fixture));

		int32 QueueEmptyCount = 0;
		Fixture.Queue->OnQueueEmpty.AddLambda([&QueueEmptyCount]() { ++QueueEmptyCount; });

		UDrawCardAction* Draw = NewObject<UDrawCardAction>(Fixture.Queue);
		Draw->Initialize(Fixture.Deck, Fixture.Dispatcher, Fixture.Combatants);

		UPhase6ATestRecordAction* Tail = NewObject<UPhase6ATestRecordAction>(Fixture.Queue);
		Tail->InitializeDeckDrawCount(Fixture.Recorder, Fixture.Deck);

		TArray<UBattleAction*> Batch{Draw, Tail};
		TestTrue(TEXT("Draw plus tail batch enqueued"), Fixture.Queue->AddBatchToBackPreserveOrder(Batch));
		TestTrue(TEXT("Draw retry resolution started"), Fixture.Queue->StartProcessing());

		const TArray<int32>& Values = Fixture.Recorder->GetValues();
		TestEqual(TEXT("Reaction and post-retry tail both executed"), Values.Num(), 2);
		if (Values.Num() == 2)
		{
			TestEqual(TEXT("DeckShuffled reaction runs after shuffle and sees one card in DrawPile"), Values[0], 1);
			TestEqual(TEXT("Tail runs after RetryDraw and sees DrawPile consumed"), Values[1], 0);
		}
		TestEqual(TEXT("RetryDraw moved shuffled card into Hand"), Fixture.Deck->GetHandCount(), 1);
		TestEqual(TEXT("RetryDraw consumed DrawPile"), Fixture.Deck->GetDrawCount(), 0);
		TestEqual(TEXT("Shuffle reactions and RetryDraw remain one resolution with one final QueueEmpty"), QueueEmptyCount, 1);
		TestFalse(TEXT("Shuffle reaction/retry flow does not fault"), Fixture.Queue->IsResolutionFaulted());
		return true;
	}
}

#endif
