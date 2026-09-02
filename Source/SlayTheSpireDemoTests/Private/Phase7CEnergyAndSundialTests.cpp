#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/DrawCardAction.h"
#include "Actions/GainEnergyAction.h"
#include "Battle/BattleManager.h"
#include "Battle/EnergyMutation.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Events/BattleEvent.h"
#include "Events/BattleEventDispatcher.h"
#include "Presentation/BattlePresentationRecorder.h"
#include "Relics/RelicContainer.h"
#include "Relics/RelicData.h"
#include "Relics/RelicInstance.h"
#include "Relics/SundialTrigger.h"
#include "Engine/World.h"

namespace Phase7C
{
	struct FWorldFixture
	{
		UWorld* World = nullptr;

		FWorldFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
		}

		~FWorldFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		ABattleManager* SpawnBattle() const
		{
			if (!IsValid(World))
			{
				return nullptr;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, Params);
		}
	};

	struct FSundialFixture : FWorldFixture
	{
		ABattleManager* Battle = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		URelicData* SundialDefinition = nullptr;
		USundialTrigger* SundialTrigger = nullptr;

		FSundialFixture()
		{
			if (!IsValid(World))
			{
				return;
			}

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, Params);
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, Params);
			Enemy = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(),
				FTransform(FVector(100.0, 0.0, 0.0)),
				Params
			);

			if (!IsValid(Battle) || !IsValid(Player) || !IsValid(Enemy))
			{
				return;
			}

			Player->MaxHP = 100;
			Enemy->MaxHP = 100;
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;

			SundialDefinition = NewObject<URelicData>(World);
			SundialDefinition->RelicId = TEXT("Sundial");
			SundialDefinition->DisplayName = FText::FromString(TEXT("Sundial"));
			SundialTrigger = NewObject<USundialTrigger>(SundialDefinition);
			SundialTrigger->ShufflesRequired = 3;
			SundialTrigger->EnergyGain = 2;
			SundialDefinition->Triggers.Add(SundialTrigger);
			Battle->DebugStartingRelics.Add(SundialDefinition);
			Battle->StartBattle();
		}

		bool IsReady() const
		{
			return IsValid(Battle)
				&& IsValid(Player)
				&& IsValid(Enemy)
				&& IsValid(SundialDefinition)
				&& IsValid(SundialTrigger)
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& IsValid(Battle->GetActionQueueForTesting())
				&& IsValid(Battle->GetPlayerRelicContainer())
				&& Battle->GetPlayerRelicContainer()->GetRelics().Num() == 1;
		}

		URelicInstance* GetSundial() const
		{
			if (!IsReady())
			{
				return nullptr;
			}
			return Battle->GetPlayerRelicContainer()->GetRelics()[0].Get();
		}

		bool BuildDispatchContext(UBattleEventDispatcher*& OutDispatcher, TArray<ACombatant*>& OutCombatants) const
		{
			return IsValid(Battle) && Battle->TryBuildEventDispatchContext(OutDispatcher, OutCombatants);
		}

		bool DispatchShuffle(UDeckRuntime* Deck, bool bRunQueue = true) const
		{
			UBattleEventDispatcher* Dispatcher = nullptr;
			TArray<ACombatant*> Combatants;
			if (!BuildDispatchContext(Dispatcher, Combatants))
			{
				return false;
			}

			UBattleActionQueue* Queue = Battle->GetActionQueueForTesting();
			if (!IsValid(Queue)
				|| !Dispatcher->Dispatch(FBattleEvent::MakeDeckShuffled(Deck), Queue, Combatants))
			{
				return false;
			}

			return !bRunQueue || Queue->StartProcessing();
		}
	};

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7CEnergyMutationContractsTest,
		"SlayTheSpireDemo.Phase7.EnergyGain.MutationContracts",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase7CEnergyMutationContractsTest::RunTest(const FString& Parameters)
	{
		FWorldFixture Fixture;
		ABattleManager* Battle = Fixture.SpawnBattle();
		if (!TestNotNull(TEXT("Battle"), Battle)) return false;

		Battle->MaxEnergy = 3;
		Battle->Energy = 3;
		const FEnergyCommitResult Gain = BattleEnergyMutation::TryGain(Battle, 2);
		TestTrue(TEXT("Gain +2 succeeds"), Gain.bSucceeded && Gain.bCommitted);
		TestEqual(TEXT("Gain Before"), Gain.EnergyBefore, 3);
		TestEqual(TEXT("Gain After may exceed MaxEnergy"), Gain.EnergyAfter, 5);
		TestEqual(TEXT("Gain Delta"), Gain.Delta, 2);
		TestEqual(TEXT("Authoritative Energy exceeds MaxEnergy"), Battle->Energy, 5);

		const FEnergyCommitResult Zero = BattleEnergyMutation::TryGain(Battle, 0);
		TestFalse(TEXT("Zero rejected"), Zero.bSucceeded);
		TestEqual(TEXT("Zero rejection does not mutate"), Battle->Energy, 5);

		const FEnergyCommitResult Negative = BattleEnergyMutation::TryGain(Battle, -2);
		TestFalse(TEXT("Negative rejected"), Negative.bSucceeded);
		TestEqual(TEXT("Negative rejection does not mutate"), Battle->Energy, 5);

		Battle->Energy = MAX_int32 - 1;
		const FEnergyCommitResult Overflow = BattleEnergyMutation::TryGain(Battle, 2);
		TestFalse(TEXT("Overflow rejected"), Overflow.bSucceeded);
		TestEqual(TEXT("Overflow rejection does not mutate"), Battle->Energy, MAX_int32 - 1);

		const FEnergyCommitResult InvalidBattle = BattleEnergyMutation::TryGain(nullptr, 2);
		TestFalse(TEXT("Invalid Battle fails soft"), InvalidBattle.bSucceeded);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7CGainEnergyActionPresentationTest,
		"SlayTheSpireDemo.Phase7.EnergyGain.ActionAndPresentation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase7CGainEnergyActionPresentationTest::RunTest(const FString& Parameters)
	{
		FWorldFixture Fixture;
		ABattleManager* Battle = Fixture.SpawnBattle();
		if (!TestNotNull(TEXT("Battle"), Battle)) return false;
		Battle->Energy = 4;
		Battle->MaxEnergy = 3;

		UBattlePresentationRecorder* Recorder = NewObject<UBattlePresentationRecorder>(Fixture.World);
		Recorder->ResetForBattle(1);
		FPresentationRecordWriter Writer;
		if (!TestTrue(TEXT("Begin presentation resolution"), Recorder->BeginResolution(EPresentationResolutionOrigin::System, Writer))) return false;

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		UGainEnergyAction* Action = NewObject<UGainEnergyAction>(Queue);
		Action->Initialize(Battle, 2);
		Action->SetPresentationRecordWriter(Writer);
		if (!TestTrue(TEXT("Queue accepts GainEnergyAction"), Queue->AddToBack(Action))) return false;
		if (!TestTrue(TEXT("Queue executes GainEnergyAction"), Queue->StartProcessing())) return false;
		TestTrue(TEXT("GainEnergyAction finishes"), Action->IsFinished());
		TestEqual(TEXT("GainEnergyAction commits Energy"), Battle->Energy, 6);

		FPresentationStateSnapshot Snapshot;
		Snapshot.BattleId = 1;
		Snapshot.StateRevision = 1;
		Snapshot.Energy = Battle->Energy;
		Snapshot.MaxEnergy = Battle->MaxEnergy;
		FPresentationResolutionEnvelope Envelope;
		if (!TestTrue(TEXT("Seal presentation resolution"), Recorder->SealResolution(Snapshot, Envelope))) return false;
		if (!TestEqual(TEXT("One EnergyChanged record"), Envelope.Records.Num(), 1)) return false;

		const FPresentationRecord& Record = Envelope.Records[0];
		TestTrue(TEXT("Record type EnergyChanged"), Record.Type == EBattlePresentationRecordType::EnergyChanged);
		TestEqual(TEXT("EnergyChanged Before"), Record.EnergyChanged.EnergyBefore, 4);
		TestEqual(TEXT("EnergyChanged After"), Record.EnergyChanged.EnergyAfter, 6);
		TestEqual(TEXT("EnergyChanged Delta"), Record.EnergyChanged.Delta, 2);

		UBattleActionQueue* InvalidQueue = NewObject<UBattleActionQueue>(Fixture.World);
		UGainEnergyAction* InvalidAction = NewObject<UGainEnergyAction>(InvalidQueue);
		InvalidAction->Initialize(nullptr, 2);
		TestTrue(TEXT("Invalid action accepted structurally"), InvalidQueue->AddToBack(InvalidAction));
		TestTrue(TEXT("Invalid action fails soft while queue completes"), InvalidQueue->StartProcessing());
		TestTrue(TEXT("Invalid action finishes"), InvalidAction->IsFinished());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7CSundialSequenceTest,
		"SlayTheSpireDemo.Phase7.Sundial.SequenceAndDeckIdentity",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase7CSundialSequenceTest::RunTest(const FString& Parameters)
	{
		FSundialFixture Fixture;
		if (!TestTrue(TEXT("Sundial fixture ready"), Fixture.IsReady())) return false;
		URelicInstance* Sundial = Fixture.GetSundial();
		if (!TestNotNull(TEXT("Sundial runtime"), Sundial)) return false;

		const int32 InitialEnergy = Fixture.Battle->Energy;
		TestEqual(TEXT("Setup starts counter at zero"), Sundial->GetCounter(), 0);

		UDeckRuntime* WrongDeck = NewObject<UDeckRuntime>(Fixture.Battle);
		TestTrue(TEXT("Wrong-deck event dispatch is handled"), Fixture.DispatchShuffle(WrongDeck, false));
		TestEqual(TEXT("Wrong deck does not advance Sundial"), Sundial->GetCounter(), 0);
		TestEqual(TEXT("Wrong deck does not grant Energy"), Fixture.Battle->Energy, InitialEnergy);

		UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
		if (!TestTrue(TEXT("First shuffle resolves"), Fixture.DispatchShuffle(Deck))) return false;
		TestEqual(TEXT("First shuffle 0 -> 1"), Sundial->GetCounter(), 1);
		TestEqual(TEXT("First shuffle no Energy"), Fixture.Battle->Energy, InitialEnergy);

		if (!TestTrue(TEXT("Second shuffle resolves"), Fixture.DispatchShuffle(Deck))) return false;
		TestEqual(TEXT("Second shuffle 1 -> 2"), Sundial->GetCounter(), 2);
		TestEqual(TEXT("Second shuffle no Energy"), Fixture.Battle->Energy, InitialEnergy);

		if (!TestTrue(TEXT("Third shuffle resolves"), Fixture.DispatchShuffle(Deck))) return false;
		TestEqual(TEXT("Third shuffle 2 -> 0"), Sundial->GetCounter(), 0);
		TestEqual(TEXT("Third shuffle grants +2 Energy"), Fixture.Battle->Energy, InitialEnergy + 2);

		if (!TestTrue(TEXT("Fourth shuffle resolves"), Fixture.DispatchShuffle(Deck))) return false;
		TestEqual(TEXT("Fourth shuffle 0 -> 1"), Sundial->GetCounter(), 1);
		TestEqual(TEXT("Fourth shuffle grants no extra Energy"), Fixture.Battle->Energy, InitialEnergy + 2);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7CSundialDrawTwoZeroCardShuffleTest,
		"SlayTheSpireDemo.Phase7.Sundial.DrawTwoCountsZeroCardShuffle",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase7CSundialDrawTwoZeroCardShuffleTest::RunTest(const FString& Parameters)
	{
		FSundialFixture Fixture;
		if (!TestTrue(TEXT("Sundial fixture ready"), Fixture.IsReady())) return false;
		URelicInstance* Sundial = Fixture.GetSundial();
		UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		if (!TestNotNull(TEXT("Sundial runtime"), Sundial)
			|| !TestNotNull(TEXT("Deck"), Deck)
			|| !TestNotNull(TEXT("Queue"), Queue))
		{
			return false;
		}

		UCardData* Definition = NewObject<UCardData>(Fixture.World);
		Definition->CardId = TEXT("SingleCycleCard");
		Definition->BaseCost = 0;
		Definition->TargetType = ECardTargetType::None;
		TArray<TObjectPtr<UCardData>> Definitions{Definition};
		Deck->InitializeFromDefinitions(Definitions, 1337);

		UCardInstance* OnlyCard = nullptr;
		if (!TestTrue(TEXT("Move only card Draw -> Hand"), Deck->TryDrawTopCard(OnlyCard))
			|| !TestNotNull(TEXT("Only card runtime"), OnlyCard)
			|| !TestTrue(TEXT("Move only card Hand -> Discard"), Deck->TryDiscardCard(OnlyCard)))
		{
			return false;
		}
		TestEqual(TEXT("Precondition Draw empty"), Deck->GetDrawCount(), 0);
		TestEqual(TEXT("Precondition Discard has one"), Deck->GetDiscardCount(), 1);

		UBattleEventDispatcher* Dispatcher = nullptr;
		TArray<ACombatant*> Combatants;
		if (!TestTrue(TEXT("Build event dispatch context"), Fixture.BuildDispatchContext(Dispatcher, Combatants))) return false;

		UDrawCardAction* DrawOne = NewObject<UDrawCardAction>(Queue);
		DrawOne->Initialize(Deck, Dispatcher, Combatants, Fixture.Player);
		UDrawCardAction* DrawTwo = NewObject<UDrawCardAction>(Queue);
		DrawTwo->Initialize(Deck, Dispatcher, Combatants, Fixture.Player);
		TArray<UBattleAction*> DrawBatch{DrawOne, DrawTwo};

		const int32 EnergyBefore = Fixture.Battle->Energy;
		if (!TestTrue(TEXT("Draw-two batch accepted"), Queue->AddBatchToBackPreserveOrder(DrawBatch))) return false;
		if (!TestTrue(TEXT("Draw-two batch resolves"), Queue->StartProcessing())) return false;

		// Draw #1: Draw empty + Discard one -> shuffle one card, then draw it.
		// Draw #2: Draw empty + Discard empty -> commit a zero-card shuffle once,
		// then RetryDraw stops because this draw attempt already shuffled.
		TestEqual(TEXT("Draw two causes two Sundial shuffle counts"), Sundial->GetCounter(), 2);
		TestEqual(TEXT("Two shuffles do not yet grant Energy"), Fixture.Battle->Energy, EnergyBefore);
		TestEqual(TEXT("Only card ends in Hand"), Deck->GetHandCount(), 1);
		TestEqual(TEXT("DrawPile ends empty"), Deck->GetDrawCount(), 0);
		TestEqual(TEXT("DiscardPile ends empty"), Deck->GetDiscardCount(), 0);
		TestFalse(TEXT("Zero-card retry flow does not fault"), Queue->IsResolutionFaulted());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7CSundialFrozenConfigurationTest,
		"SlayTheSpireDemo.Phase7.Sundial.TriggerReadOnlyAndFrozenConfig",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase7CSundialFrozenConfigurationTest::RunTest(const FString& Parameters)
	{
		FSundialFixture Fixture;
		if (!TestTrue(TEXT("Sundial fixture ready"), Fixture.IsReady())) return false;
		URelicInstance* Sundial = Fixture.GetSundial();
		if (!TestNotNull(TEXT("Sundial runtime"), Sundial)) return false;

		UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
		if (!Fixture.DispatchShuffle(Deck) || !Fixture.DispatchShuffle(Deck)) return false;
		TestEqual(TEXT("Pre-threshold counter is two"), Sundial->GetCounter(), 2);
		const int32 EnergyBeforeThird = Fixture.Battle->Energy;

		if (!TestTrue(TEXT("Third dispatch builds reaction without executing it"), Fixture.DispatchShuffle(Deck, false))) return false;
		TestEqual(TEXT("Trigger/BuildReactions are read-only for Counter"), Sundial->GetCounter(), 2);
		TestEqual(TEXT("Trigger/BuildReactions are read-only for Energy"), Fixture.Battle->Energy, EnergyBeforeThird);

		// Mutation after BuildReactions proves the queued Action owns the frozen
		// intended 3/+2 values rather than rediscovering Trigger configuration.
		Fixture.SundialTrigger->ShufflesRequired = 99;
		Fixture.SundialTrigger->EnergyGain = 99;

		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		if (!TestTrue(TEXT("Frozen reaction executes"), Queue->StartProcessing())) return false;
		TestEqual(TEXT("Frozen RequiredShuffles resets counter"), Sundial->GetCounter(), 0);
		TestEqual(TEXT("Frozen EnergyGain grants +2"), Fixture.Battle->Energy, EnergyBeforeThird + 2);
		return true;
	}
}

#endif
