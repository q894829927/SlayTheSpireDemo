#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/AdvanceRelicCounterAction.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/GainEnergyAction.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Relics/RelicContainer.h"
#include "Relics/RelicData.h"
#include "Relics/RelicInstance.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase7ERelicCounterThresholdEnqueueFailureTest,
	"SlayTheSpireDemo.Phase7E.Counter.ThresholdEnqueueFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPhase7ERelicCounterThresholdEnqueueFailureTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("World"), World)) return false;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, Params);
	ACombatant* Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, Params);
	ACombatant* Enemy = World->SpawnActor<ACombatant>(
		ACombatant::StaticClass(),
		FTransform(FVector(100.0, 0.0, 0.0)),
		Params);
	if (!TestNotNull(TEXT("Battle"), Battle)
		|| !TestNotNull(TEXT("Player"), Player)
		|| !TestNotNull(TEXT("Enemy"), Enemy))
	{
		World->DestroyWorld(false);
		return false;
	}

	Battle->Player = Player;
	Battle->Enemy = Enemy;
	Battle->OpeningHandDrawCount = 0;
	Battle->PlayerTurnDrawCount = 0;

	URelicData* Definition = NewObject<URelicData>(World);
	Definition->RelicId = TEXT("CounterFaultRelic");
	Battle->DebugStartingRelics.Add(Definition);
	Battle->StartBattle();

	UBattleActionQueue* Queue = Battle->GetActionQueueForTesting();
	URelicContainer* Container = Battle->GetPlayerRelicContainer();
	URelicInstance* Relic = IsValid(Container) && Container->GetRelics().Num() == 1
		? Container->GetRelics()[0].Get()
		: nullptr;
	if (!TestNotNull(TEXT("Queue"), Queue) || !TestNotNull(TEXT("Runtime Relic"), Relic))
	{
		World->DestroyWorld(false);
		return false;
	}

	// First advance establishes Counter=1 without consuming its prepared reward.
	UGainEnergyAction* UnusedReward = NewObject<UGainEnergyAction>(Queue);
	UnusedReward->Initialize(Battle, 1);
	TArray<UBattleAction*> FirstRewards{UnusedReward};
	UAdvanceRelicCounterAction* FirstAdvance = NewObject<UAdvanceRelicCounterAction>(Queue);
	if (!TestTrue(TEXT("First CounterAction initializes"), FirstAdvance->Initialize(Relic, 3, FirstRewards))
		|| !TestTrue(TEXT("First CounterAction queues"), Queue->AddToBack(FirstAdvance))
		|| !TestTrue(TEXT("First CounterAction executes"), Queue->StartProcessing()))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Precondition Counter is one"), Relic->GetCounter(), 1);

	// The reward is deliberately inserted behind the current CounterAction. When
	// the threshold action tries to insert that same reward at the front, Queue
	// validation rejects it as already pending. Counter must remain 1.
	UGainEnergyAction* PendingReward = NewObject<UGainEnergyAction>(Queue);
	PendingReward->Initialize(Battle, 1);
	TArray<UBattleAction*> ThresholdRewards{PendingReward};
	UAdvanceRelicCounterAction* ThresholdAdvance = NewObject<UAdvanceRelicCounterAction>(Queue);
	if (!TestTrue(TEXT("Threshold CounterAction initializes"), ThresholdAdvance->Initialize(Relic, 2, ThresholdRewards)))
	{
		World->DestroyWorld(false);
		return false;
	}

	TArray<UBattleAction*> InitialBatch{ThresholdAdvance, PendingReward};
	if (!TestTrue(TEXT("Initial threshold batch queues"), Queue->AddBatchToBackPreserveOrder(InitialBatch))
		|| !TestTrue(TEXT("Threshold batch starts"), Queue->StartProcessing()))
	{
		World->DestroyWorld(false);
		return false;
	}

	TestTrue(TEXT("Dependent insertion failure faults Queue"), Queue->IsResolutionFaulted());
	TestEqual(TEXT("Counter is not reset on insertion failure"), Relic->GetCounter(), 1);

	World->DestroyWorld(false);
	return true;
}

#endif
