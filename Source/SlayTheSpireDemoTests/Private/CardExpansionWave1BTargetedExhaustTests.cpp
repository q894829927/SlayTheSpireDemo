#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/ExhaustCardAction.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Events/BattleEvent.h"
#include "Events/BattleEventDispatcher.h"
#include "Presentation/PresentationTypes.h"
#include "Engine/World.h"

namespace CardExpansionWave1BTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		TArray<FPresentationResolutionEnvelope> Deliveries;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(),
				FTransform(FVector(100.0, 0.0, 0.0)),
				SpawnParameters
			);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle))
			{
				return;
			}

			Player->MaxHP = 100;
			Enemy->MaxHP = 100;
			Player->PresentationId = TEXT("PlayerHero");
			Enemy->PresentationId = TEXT("EnemyPrimary");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));

			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->bEnableCommittedPresentationRecording = true;
			Battle->OnPresentationResolutionReady.AddLambda(
				[this](const FPresentationResolutionEnvelope& Envelope)
				{
					Deliveries.Add(Envelope);
				}
			);
		}

		~FFixture()
		{
			UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		UCardData* CreateCard(const TCHAR* CardId)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(CardId);
			Card->DisplayName = FText::FromString(CardId);
			Card->Description = FText::FromString(TEXT("Wave 1B targeted exhaust fixture."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		bool Start(const TArray<UCardData*>& Definitions)
		{
			if (!IsValid(Battle) || Definitions.Num() == 0)
			{
				return false;
			}

			Battle->DebugStartingDeck.Reset();
			for (UCardData* Definition : Definitions)
			{
				Battle->DebugStartingDeck.Add(Definition);
			}
			Battle->OpeningHandDrawCount = Definitions.Num();
			Battle->StartBattle();
			Flush();
			return IsValid(Battle->GetActionQueueForTesting())
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn
				&& Battle->GetDeckRuntimeForTesting()->GetHandCount() == Definitions.Num();
		}

		void Flush() const
		{
			if (IsValid(Battle))
			{
				Battle->FlushScheduledReadStateReadyForTesting();
			}
		}

		void ResetDeliveries()
		{
			Deliveries.Reset();
		}

		const FPresentationResolutionEnvelope* LastDelivery() const
		{
			return Deliveries.Num() > 0 ? &Deliveries.Last() : nullptr;
		}
	};

	const FPresentationRecord* FindTargetedExhaustRecord(
		const FPresentationResolutionEnvelope& Envelope,
		int32 RuntimeId
	)
	{
		return Envelope.Records.FindByPredicate(
			[RuntimeId](const FPresentationRecord& Record)
			{
				return Record.Type == EBattlePresentationRecordType::CardZoneChanged
					&& Record.CardZoneChanged.Card.RuntimeId == RuntimeId
					&& Record.CardZoneChanged.FromZone == ECardZone::Hand
					&& Record.CardZoneChanged.ToZone == ECardZone::ExhaustPile;
			}
		);
	}

	bool RunTargetedExhaustAction(
		FFixture& Fixture,
		UCardInstance* Target,
		UExhaustCardAction*& OutAction
	)
	{
		OutAction = nullptr;
		if (!IsValid(Target)
			|| !Fixture.Battle->BeginSystemPresentationResolutionForTesting())
		{
			return false;
		}

		UBattleEventDispatcher* Dispatcher = nullptr;
		TArray<ACombatant*> Combatants;
		if (!Fixture.Battle->TryBuildEventDispatchContext(Dispatcher, Combatants)
			|| !IsValid(Dispatcher))
		{
			return false;
		}

		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
		if (!IsValid(Queue) || !IsValid(Deck))
		{
			return false;
		}

		UExhaustCardAction* Action = NewObject<UExhaustCardAction>(Queue);
		Action->Initialize(Deck, Target, Fixture.Player, Dispatcher, Combatants);
		Action->SetPresentationRecordWriter(Fixture.Battle->GetActivePresentationRecordWriterForTesting());
		OutAction = Action;
		return Queue->AddToBack(Action) && Queue->StartProcessing();
	}
}

using namespace CardExpansionWave1BTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1BTargetedExhaustCommitTest,
	"SlayTheSpireDemo.CardExpansion.Wave1B.TargetedExhaust.CommitAndEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1BTargetedExhaustCommitTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* FirstDefinition = Fixture.CreateCard(TEXT("Wave1BTarget"));
	UCardData* SecondDefinition = Fixture.CreateCard(TEXT("Wave1BOther"));
	if (!TestTrue(TEXT("Fixture starts with two Hand cards"), Fixture.Start({ FirstDefinition, SecondDefinition })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	const TArray<TObjectPtr<UCardInstance>>& Hand = Deck->GetHandCards();
	if (!TestEqual(TEXT("Two exact Hand instances exist"), Hand.Num(), 2))
	{
		return false;
	}

	UCardInstance* Target = Hand[0].Get();
	UCardInstance* Other = Hand[1].Get();
	if (!TestNotNull(TEXT("Target exists"), Target) || !TestNotNull(TEXT("Other card exists"), Other))
	{
		return false;
	}

	Fixture.ResetDeliveries();
	int32 ExhaustEventCount = 0;
	FCardExhaustedEvent ObservedPayload;
	bool bObservedCommittedState = false;
	UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
	UBattleEventDispatcher::OnEventDispatchedForTesting.AddLambda(
		[&](const FBattleEvent& Event)
		{
			const FCardExhaustedEvent* Payload = Event.TryGet<FCardExhaustedEvent>();
			if (Payload == nullptr)
			{
				return;
			}
			++ExhaustEventCount;
			ObservedPayload = *Payload;
			bObservedCommittedState = Deck->GetExhaustCount() == 1
				&& !Deck->IsCardInHand(Target)
				&& Deck->IsCardInHand(Other);
		}
	);

	UExhaustCardAction* Action = nullptr;
	if (!TestTrue(TEXT("Targeted Exhaust Action runs"), RunTargetedExhaustAction(Fixture, Target, Action)))
	{
		return false;
	}
	Fixture.Flush();

	if (!TestNotNull(TEXT("Action exists"), Action))
	{
		return false;
	}

	const FCardZoneMutationResult& CommitResult = Action->GetCommitResult();
	TestTrue(TEXT("Action retains committed typed result"), CommitResult.bCommitted);
	TestEqual(TEXT("Commit RuntimeId matches exact target"), CommitResult.CardRuntimeId, Target->GetRuntimeId());
	TestEqual(TEXT("Commit CardId matches exact target"), CommitResult.CardId, Target->GetCardId());
	TestEqual(TEXT("Commit FromZone is Hand"), CommitResult.FromZone, ECardZone::Hand);
	TestEqual(TEXT("Commit ToZone is ExhaustPile"), CommitResult.ToZone, ECardZone::ExhaustPile);
	TestEqual(TEXT("Exactly one CardExhausted event"), ExhaustEventCount, 1);
	TestTrue(TEXT("Dispatch observes already-committed state"), bObservedCommittedState);
	TestTrue(TEXT("Event keeps exact target reference"), ObservedPayload.Card == Target);
	TestEqual(TEXT("Event RuntimeId matches CommitResult"), ObservedPayload.CardRuntimeId, CommitResult.CardRuntimeId);
	TestEqual(TEXT("Event CardId matches CommitResult"), ObservedPayload.CardId, CommitResult.CardId);
	TestEqual(TEXT("Event FromZone matches CommitResult"), ObservedPayload.FromZone, CommitResult.FromZone);
	TestEqual(TEXT("Event ToZone matches CommitResult"), ObservedPayload.ToZone, CommitResult.ToZone);
	TestEqual(TEXT("Only target leaves Hand"), Deck->GetHandCount(), 1);
	TestEqual(TEXT("Exactly one card enters ExhaustPile"), Deck->GetExhaustCount(), 1);
	TestFalse(TEXT("Target is no longer in Hand"), Deck->IsCardInHand(Target));
	TestTrue(TEXT("Other exact Hand card is untouched"), Deck->IsCardInHand(Other));
	TestFalse(TEXT("Successful targeted Exhaust does not fault resolution"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());

	const FPresentationResolutionEnvelope* Envelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Targeted Exhaust presentation envelope exists"), Envelope))
	{
		return false;
	}
	const FPresentationRecord* ZoneRecord = FindTargetedExhaustRecord(*Envelope, Target->GetRuntimeId());
	if (!TestNotNull(TEXT("Targeted Exhaust CardZoneChanged record exists"), ZoneRecord))
	{
		return false;
	}
	TestEqual(TEXT("Presentation CardId matches CommitResult"), ZoneRecord->CardZoneChanged.Card.CardId, CommitResult.CardId);
	TestEqual(TEXT("Presentation FromIndex matches CommitResult"), ZoneRecord->CardZoneChanged.FromIndex, CommitResult.FromIndex);
	TestEqual(TEXT("Presentation ToIndex matches CommitResult"), ZoneRecord->CardZoneChanged.ToIndex, CommitResult.ToIndex);

	const int32 EventCountBeforeStaleRetry = ExhaustEventCount;
	Fixture.ResetDeliveries();
	UExhaustCardAction* StaleAction = nullptr;
	if (!TestTrue(TEXT("Stale target Action still executes fail-soft"), RunTargetedExhaustAction(Fixture, Target, StaleAction)))
	{
		return false;
	}
	Fixture.Flush();
	if (!TestNotNull(TEXT("Stale Action exists"), StaleAction))
	{
		return false;
	}
	TestFalse(TEXT("Stale target exposes non-committed typed result"), StaleAction->GetCommitResult().bCommitted);
	TestEqual(TEXT("Stale retry emits no duplicate CardExhausted"), ExhaustEventCount, EventCountBeforeStaleRetry);
	TestEqual(TEXT("Stale retry does not duplicate ExhaustPile entry"), Deck->GetExhaustCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1BWiringFailureBeforeCommitTest,
	"SlayTheSpireDemo.CardExpansion.Wave1B.TargetedExhaust.WiringFailureBeforeCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1BWiringFailureBeforeCommitTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* Definition = Fixture.CreateCard(TEXT("Wave1BWiringFailure"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ Definition })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	UCardInstance* Target = Deck->GetFirstHandCard();
	if (!TestNotNull(TEXT("Target exists"), Target) || !TestNotNull(TEXT("Queue exists"), Queue))
	{
		return false;
	}

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

	UExhaustCardAction* Action = NewObject<UExhaustCardAction>(Queue);
	TArray<ACombatant*> NoCombatants;
	Action->Initialize(Deck, Target, Fixture.Player, nullptr, NoCombatants);
	if (!TestTrue(TEXT("Wiring-failure Action enqueued"), Queue->AddToBack(Action))
		|| !TestTrue(TEXT("Wiring-failure Action processed"), Queue->StartProcessing()))
	{
		return false;
	}

	TestFalse(TEXT("Missing wiring prevents targeted Exhaust commit"), Action->GetCommitResult().bCommitted);
	TestTrue(TEXT("Missing wiring requests ResolutionFault"), Queue->IsResolutionFaulted());
	TestTrue(TEXT("Target remains in Hand after pre-commit fault"), Deck->IsCardInHand(Target));
	TestEqual(TEXT("ExhaustPile remains unchanged"), Deck->GetExhaustCount(), 0);
	TestEqual(TEXT("No CardExhausted event emitted"), ExhaustEventCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1BDeckRuntimeMutationOnlyTest,
	"SlayTheSpireDemo.CardExpansion.Wave1B.TargetedExhaust.DeckRuntimeMutationOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1BDeckRuntimeMutationOnlyTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* FirstDefinition = Fixture.CreateCard(TEXT("Wave1BDeckTarget"));
	UCardData* SecondDefinition = Fixture.CreateCard(TEXT("Wave1BDeckOther"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ FirstDefinition, SecondDefinition })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	const TArray<TObjectPtr<UCardInstance>>& Hand = Deck->GetHandCards();
	if (!TestEqual(TEXT("Two Hand cards exist"), Hand.Num(), 2))
	{
		return false;
	}
	UCardInstance* Target = Hand[0].Get();
	UCardInstance* Other = Hand[1].Get();

	int32 EventCount = 0;
	UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
	UBattleEventDispatcher::OnEventDispatchedForTesting.AddLambda(
		[&](const FBattleEvent& Event)
		{
			if (Event.TryGet<FCardExhaustedEvent>() != nullptr)
			{
				++EventCount;
			}
		}
	);

	const FCardZoneMutationResult CommitResult = Deck->TryExhaustHandCardCommit(Target);
	TestTrue(TEXT("DeckRuntime commits exact Hand target"), CommitResult.bCommitted);
	TestEqual(TEXT("DeckRuntime result FromZone"), CommitResult.FromZone, ECardZone::Hand);
	TestEqual(TEXT("DeckRuntime result ToZone"), CommitResult.ToZone, ECardZone::ExhaustPile);
	TestEqual(TEXT("DeckRuntime result RuntimeId"), CommitResult.CardRuntimeId, Target->GetRuntimeId());
	TestTrue(TEXT("Other Hand card remains untouched"), Deck->IsCardInHand(Other));
	TestEqual(TEXT("DeckRuntime mutation owner does not dispatch Gameplay event"), EventCount, 0);
	return true;
}

#endif
