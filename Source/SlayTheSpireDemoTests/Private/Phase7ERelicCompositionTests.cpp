#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/AdvanceRelicCounterAction.h"
#include "Actions/BattleAction.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/GainBlockAction.h"
#include "Actions/GainEnergyAction.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Events/BattleEvent.h"
#include "Events/BattleEventDispatcher.h"
#include "Presentation/BattlePresentationRecorder.h"
#include "Relics/DeckShuffledCountTrigger.h"
#include "Relics/Effects/GainBlockRelicEffect.h"
#include "Relics/Effects/GainEnergyRelicEffect.h"
#include "Relics/Effects/RelicEffect.h"
#include "Relics/RelicContainer.h"
#include "Relics/RelicData.h"
#include "Relics/RelicInstance.h"
#include "Engine/World.h"

namespace Phase7E
{
	struct FCompositionFixture
	{
		UWorld* World = nullptr;
		ABattleManager* Battle = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		URelicData* Definition = nullptr;
		UDeckShuffledCountTrigger* Trigger = nullptr;
		UGainBlockRelicEffect* BlockEffect = nullptr;
		UGainEnergyRelicEffect* EnergyEffect = nullptr;

		FCompositionFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
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
				Params);

			if (!IsValid(Battle) || !IsValid(Player) || !IsValid(Enemy))
			{
				return;
			}

			Player->PresentationId = TEXT("Phase7EPlayer");
			Enemy->PresentationId = TEXT("Phase7EEnemy");
			Player->MaxHP = 100;
			Enemy->MaxHP = 100;
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;

			Definition = NewObject<URelicData>(World);
			Definition->RelicId = TEXT("Phase7ECompositeRelic");
			Definition->DisplayName = FText::FromString(TEXT("Phase7E Composite Relic"));
			Definition->bShowCounter = true;
			Definition->CounterDisplayMax = 3;

			Trigger = NewObject<UDeckShuffledCountTrigger>(Definition);
			Trigger->RequiredCount = 3;
			BlockEffect = NewObject<UGainBlockRelicEffect>(Trigger);
			BlockEffect->Amount = 6;
			EnergyEffect = NewObject<UGainEnergyRelicEffect>(Trigger);
			EnergyEffect->Amount = 1;
			Trigger->Effects.Add(BlockEffect);
			Trigger->Effects.Add(EnergyEffect);
			Definition->Triggers.Add(Trigger);
			Battle->DebugStartingRelics.Add(Definition);
			Battle->StartBattle();
		}

		~FCompositionFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		bool IsReady() const
		{
			const URelicContainer* Container = IsValid(Battle) ? Battle->GetPlayerRelicContainer() : nullptr;
			return IsValid(World)
				&& IsValid(Battle)
				&& IsValid(Player)
				&& IsValid(Enemy)
				&& IsValid(Definition)
				&& IsValid(Trigger)
				&& IsValid(BlockEffect)
				&& IsValid(EnergyEffect)
				&& IsValid(Container)
				&& Container->GetRelics().Num() == 1
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& IsValid(Battle->GetActionQueueForTesting());
		}

		URelicInstance* GetRuntimeRelic() const
		{
			if (!IsReady())
			{
				return nullptr;
			}
			return Battle->GetPlayerRelicContainer()->GetRelics()[0].Get();
		}

		bool DispatchShuffle(
			UDeckRuntime* Deck,
			const FPresentationRecordWriter* Writer = nullptr,
			bool bRunQueue = true) const
		{
			UBattleEventDispatcher* Dispatcher = nullptr;
			TArray<ACombatant*> Combatants;
			if (!IsValid(Battle)
				|| !Battle->TryBuildEventDispatchContext(Dispatcher, Combatants)
				|| !IsValid(Dispatcher))
			{
				return false;
			}

			UBattleActionQueue* Queue = Battle->GetActionQueueForTesting();
			if (!IsValid(Queue)
				|| !Dispatcher->Dispatch(
					FBattleEvent::MakeDeckShuffled(Deck),
					Queue,
					Combatants,
					nullptr,
					Writer))
			{
				return false;
			}

			return !bRunQueue || Queue->StartProcessing();
		}
	};

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7ERelicEffectBuildContractsTest,
		"SlayTheSpireDemo.Phase7E.Effects.BuildContracts",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	bool FPhase7ERelicEffectBuildContractsTest::RunTest(const FString& Parameters)
	{
		FCompositionFixture Fixture;
		if (!TestTrue(TEXT("Composition fixture ready"), Fixture.IsReady())) return false;

		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		URelicInstance* Relic = Fixture.GetRuntimeRelic();
		if (!TestNotNull(TEXT("Queue"), Queue) || !TestNotNull(TEXT("Relic"), Relic)) return false;

		FName OwnerPresentationId = NAME_None;
		if (!TestTrue(
			TEXT("Owner PresentationId resolves"),
			Fixture.Battle->TryResolveCombatantPresentationId(Fixture.Player, OwnerPresentationId)))
		{
			return false;
		}

		FRelicEffectContext Context;
		Context.Relic = Relic;
		Context.Battle = Fixture.Battle;
		Context.Owner = Fixture.Player;
		Context.OwnerPresentationId = OwnerPresentationId;
		Context.ActionOuter = Queue;

		TArray<UBattleAction*> EnergyActions;
		TestTrue(TEXT("GainEnergyRelicEffect builds"), Fixture.EnergyEffect->BuildActions(Context, EnergyActions));
		if (!TestEqual(TEXT("GainEnergy action count"), EnergyActions.Num(), 1)) return false;
		TestTrue(TEXT("GainEnergy action type"), IsValid(Cast<UGainEnergyAction>(EnergyActions[0])));
		TestTrue(TEXT("GainEnergy action Outer is target Queue"), EnergyActions[0]->GetOuter() == Queue);

		TArray<UBattleAction*> BlockActions;
		TestTrue(TEXT("GainBlockRelicEffect builds"), Fixture.BlockEffect->BuildActions(Context, BlockActions));
		if (!TestEqual(TEXT("GainBlock action count"), BlockActions.Num(), 1)) return false;
		TestTrue(TEXT("GainBlock action type"), IsValid(Cast<UGainBlockAction>(BlockActions[0])));
		TestTrue(TEXT("GainBlock action Outer is target Queue"), BlockActions[0]->GetOuter() == Queue);

		FRelicEffectContext MissingIdentityContext = Context;
		MissingIdentityContext.OwnerPresentationId = NAME_None;
		TArray<UBattleAction*> MissingIdentityActions;
		TestFalse(
			TEXT("GainBlock fails closed without participant identity"),
			Fixture.BlockEffect->BuildActions(MissingIdentityContext, MissingIdentityActions));
		TestEqual(TEXT("Missing identity builds no actions"), MissingIdentityActions.Num(), 0);

		UGainEnergyRelicEffect* InvalidEnergyEffect = NewObject<UGainEnergyRelicEffect>(Fixture.Trigger);
		InvalidEnergyEffect->Amount = 0;
		TArray<UBattleAction*> InvalidEnergyActions;
		TestFalse(TEXT("Invalid Energy amount fails closed"), InvalidEnergyEffect->BuildActions(Context, InvalidEnergyActions));

		UAdvanceRelicCounterAction* ValidCounter = NewObject<UAdvanceRelicCounterAction>(Queue);
		TestTrue(TEXT("CounterAction accepts valid prepared batch"),
			ValidCounter->Initialize(Relic, 3, EnergyActions));

		UBattleActionQueue* OtherQueue = NewObject<UBattleActionQueue>(Fixture.World);
		UGainEnergyAction* WrongOuterReward = NewObject<UGainEnergyAction>(OtherQueue);
		WrongOuterReward->Initialize(Fixture.Battle, 1);
		TArray<UBattleAction*> WrongOuterRewards{WrongOuterReward};
		UAdvanceRelicCounterAction* InvalidCounter = NewObject<UAdvanceRelicCounterAction>(Queue);
		TestFalse(TEXT("CounterAction rejects reward with wrong Outer"),
			InvalidCounter->Initialize(Relic, 3, WrongOuterRewards));

		TArray<UBattleAction*> DuplicateRewards{EnergyActions[0], EnergyActions[0]};
		UAdvanceRelicCounterAction* DuplicateCounter = NewObject<UAdvanceRelicCounterAction>(Queue);
		TestFalse(TEXT("CounterAction rejects duplicate reward pointer"),
			DuplicateCounter->Initialize(Relic, 3, DuplicateRewards));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7ECompositeRelicSequenceAndPresentationTest,
		"SlayTheSpireDemo.Phase7E.Composite.SequenceOrderAndPresentation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	bool FPhase7ECompositeRelicSequenceAndPresentationTest::RunTest(const FString& Parameters)
	{
		FCompositionFixture Fixture;
		if (!TestTrue(TEXT("Composition fixture ready"), Fixture.IsReady())) return false;

		URelicInstance* Relic = Fixture.GetRuntimeRelic();
		UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
		if (!TestNotNull(TEXT("Runtime Relic"), Relic) || !TestNotNull(TEXT("Deck"), Deck)) return false;

		const int32 EnergyBefore = Fixture.Battle->Energy;
		const int32 BlockBefore = Fixture.Player->Block;
		TestEqual(TEXT("Setup shuffle does not advance Counter"), Relic->GetCounter(), 0);

		UDeckRuntime* WrongDeck = NewObject<UDeckRuntime>(Fixture.Battle);
		TestTrue(TEXT("Wrong-deck dispatch handled"), Fixture.DispatchShuffle(WrongDeck, nullptr, false));
		TestEqual(TEXT("Wrong deck leaves Counter unchanged"), Relic->GetCounter(), 0);

		UBattlePresentationRecorder* Recorder = NewObject<UBattlePresentationRecorder>(Fixture.World);
		Recorder->ResetForBattle(1);
		FPresentationRecordWriter Writer;
		if (!TestTrue(
			TEXT("Begin composite presentation resolution"),
			Recorder->BeginResolution(EPresentationResolutionOrigin::System, Writer)))
		{
			return false;
		}

		if (!TestTrue(TEXT("First shuffle resolves"), Fixture.DispatchShuffle(Deck, &Writer))) return false;
		TestEqual(TEXT("First shuffle 0 -> 1"), Relic->GetCounter(), 1);
		TestEqual(TEXT("First shuffle no Block"), Fixture.Player->Block, BlockBefore);
		TestEqual(TEXT("First shuffle no Energy"), Fixture.Battle->Energy, EnergyBefore);

		if (!TestTrue(TEXT("Second shuffle resolves"), Fixture.DispatchShuffle(Deck, &Writer))) return false;
		TestEqual(TEXT("Second shuffle 1 -> 2"), Relic->GetCounter(), 2);
		TestEqual(TEXT("Second shuffle no Block"), Fixture.Player->Block, BlockBefore);
		TestEqual(TEXT("Second shuffle no Energy"), Fixture.Battle->Energy, EnergyBefore);

		if (!TestTrue(TEXT("Third shuffle resolves"), Fixture.DispatchShuffle(Deck, &Writer))) return false;
		TestEqual(TEXT("Third shuffle 2 -> 0"), Relic->GetCounter(), 0);
		TestEqual(TEXT("Threshold grants Block first"), Fixture.Player->Block, BlockBefore + 6);
		TestEqual(TEXT("Threshold grants Energy"), Fixture.Battle->Energy, EnergyBefore + 1);

		FPresentationStateSnapshot Snapshot;
		Snapshot.BattleId = 1;
		Snapshot.StateRevision = 1;
		Snapshot.Energy = Fixture.Battle->Energy;
		Snapshot.MaxEnergy = Fixture.Battle->MaxEnergy;
		FPresentationResolutionEnvelope Envelope;
		if (!TestTrue(TEXT("Composite presentation resolution remains valid"), Recorder->SealResolution(Snapshot, Envelope))) return false;
		if (!TestEqual(TEXT("Exactly two reward records"), Envelope.Records.Num(), 2)) return false;

		TestTrue(TEXT("Reward record 0 is BlockChanged"), Envelope.Records[0].Type == EBattlePresentationRecordType::BlockChanged);
		TestEqual(TEXT("Block SourcePresentationId"), Envelope.Records[0].BlockChanged.SourcePresentationId, FName(TEXT("Phase7EPlayer")));
		TestEqual(TEXT("Block TargetPresentationId"), Envelope.Records[0].BlockChanged.TargetPresentationId, FName(TEXT("Phase7EPlayer")));
		TestEqual(TEXT("Block Delta"), Envelope.Records[0].BlockChanged.BlockDelta, 6);
		TestTrue(TEXT("Reward record 1 is EnergyChanged"), Envelope.Records[1].Type == EBattlePresentationRecordType::EnergyChanged);
		TestEqual(TEXT("Energy Delta"), Envelope.Records[1].EnergyChanged.Delta, 1);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7EFailClosedAndLiveMembershipTest,
		"SlayTheSpireDemo.Phase7E.Composite.FailClosedAndLiveMembership",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	bool FPhase7EFailClosedAndLiveMembershipTest::RunTest(const FString& Parameters)
	{
		FCompositionFixture Fixture;
		if (!TestTrue(TEXT("Composition fixture ready"), Fixture.IsReady())) return false;

		URelicInstance* Relic = Fixture.GetRuntimeRelic();
		UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		if (!TestNotNull(TEXT("Relic"), Relic)
			|| !TestNotNull(TEXT("Deck"), Deck)
			|| !TestNotNull(TEXT("Queue"), Queue))
		{
			return false;
		}

		const int32 EnergyBefore = Fixture.Battle->Energy;
		const int32 BlockBefore = Fixture.Player->Block;

		Fixture.EnergyEffect->Amount = 0;
		TestTrue(TEXT("Build failure dispatch is fail-soft"), Fixture.DispatchShuffle(Deck, nullptr, false));
		TestEqual(TEXT("Build failure inserts no reaction"), Queue->GetPendingCount(), 0);
		TestEqual(TEXT("Build failure does not advance Counter"), Relic->GetCounter(), 0);
		TestEqual(TEXT("Build failure grants no Block"), Fixture.Player->Block, BlockBefore);
		TestEqual(TEXT("Build failure grants no Energy"), Fixture.Battle->Energy, EnergyBefore);

		Fixture.EnergyEffect->Amount = 1;
		TestTrue(TEXT("Valid reaction is prepared"), Fixture.DispatchShuffle(Deck, nullptr, false));
		if (!TestEqual(TEXT("One CounterAction pending"), Queue->GetPendingCount(), 1)) return false;

		Fixture.Battle->GetPlayerRelicContainer()->Reset();
		TestTrue(TEXT("Queue processes stale reaction fail-soft"), Queue->StartProcessing());
		TestTrue(TEXT("Stale membership does not fault Queue"), !Queue->IsResolutionFaulted());
		TestEqual(TEXT("Stale membership does not mutate Counter"), Relic->GetCounter(), 0);
		TestEqual(TEXT("Stale membership grants no Block"), Fixture.Player->Block, BlockBefore);
		TestEqual(TEXT("Stale membership grants no Energy"), Fixture.Battle->Energy, EnergyBefore);
		return true;
	}
}

#endif
