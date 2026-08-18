#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6ATestTypes.h"
#include "../Actions/BattleActionQueue.h"
#include "../Combat/Combatant.h"
#include "../Events/BattleEvent.h"
#include "../Events/BattleEventDispatcher.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusData.h"
#include "Engine/World.h"

namespace Phase6AExecutionRegression
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;

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

			if (IsValid(Player))
			{
				Player->MaxHP = 100;
				Player->InitializeCombatant();
			}
			if (IsValid(Enemy))
			{
				Enemy->MaxHP = 100;
				Enemy->InitializeCombatant();
			}
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
				&& IsValid(Player)
				&& IsValid(Enemy)
				&& IsValid(Player->GetStatusContainer())
				&& IsValid(Enemy->GetStatusContainer());
		}
	};

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (!Fixture.IsReady())
		{
			Test.AddError(TEXT("Failed to create the transient Phase 6A execution-order fixture."));
			return false;
		}
		return true;
	}

	UStatusData* CreateStatus(UObject* Outer, const TCHAR* StatusId)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = FName(StatusId);
		return Definition;
	}

	UPhase6ATestRecordTrigger* AddRecordTrigger(
		UStatusData* Definition,
		UPhase6ATestExecutionRecorder* Recorder,
		int32 Value,
		int32 Priority = 0
	)
	{
		UPhase6ATestRecordTrigger* Trigger = NewObject<UPhase6ATestRecordTrigger>(Definition);
		Trigger->Priority = Priority;
		Trigger->Initialize(Recorder, Value);
		Definition->Triggers.Add(Trigger);
		return Trigger;
	}

	UPhase6ATestNestedTrigger* AddNestedTrigger(
		UStatusData* Definition,
		UPhase6ATestExecutionRecorder* Recorder,
		UBattleEventDispatcher* Dispatcher,
		ACombatant* NestedTurnOwner,
		const TArray<ACombatant*>& Combatants,
		int32 EmitValue,
		int32 SiblingValue,
		int32 Priority = 0
	)
	{
		UPhase6ATestNestedTrigger* Trigger = NewObject<UPhase6ATestNestedTrigger>(Definition);
		Trigger->Priority = Priority;
		Trigger->Initialize(Recorder, Dispatcher, NestedTurnOwner, Combatants, EmitValue, SiblingValue);
		Definition->Triggers.Add(Trigger);
		return Trigger;
	}

	void ApplyStatus(ACombatant* Combatant, UStatusData* Definition, uint64 RuntimeSequence)
	{
		bool bCreated = false;
		Combatant->GetStatusContainer()->ApplyStatus(Definition, 1, RuntimeSequence, bCreated);
	}

	TArray<ACombatant*> BothCombatants(const FFixture& Fixture)
	{
		return TArray<ACombatant*>{Fixture.Player, Fixture.Enemy};
	}

	bool ExpectValues(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		const UPhase6ATestExecutionRecorder* Recorder,
		std::initializer_list<int32> ExpectedValues
	)
	{
		if (!IsValid(Recorder))
		{
			Test.AddError(FString::Printf(TEXT("%s: Recorder is invalid."), Label));
			return false;
		}

		const TArray<int32>& Actual = Recorder->GetValues();
		Test.TestEqual(FString::Printf(TEXT("%s count"), Label), Actual.Num(), static_cast<int32>(ExpectedValues.size()));

		int32 Index = 0;
		for (int32 Expected : ExpectedValues)
		{
			if (!Actual.IsValidIndex(Index))
			{
				return false;
			}
			Test.TestEqual(FString::Printf(TEXT("%s[%d]"), Label, Index), Actual[Index], Expected);
			++Index;
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AActualPriorityExecutionOrderingTest,
		"SlayTheSpireDemo.Phase6A.Trigger.ActualPriorityExecutionOrdering",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AActualPriorityExecutionOrderingTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UPhase6ATestExecutionRecorder* Recorder = NewObject<UPhase6ATestExecutionRecorder>(Fixture.World);
		UStatusData* A = CreateStatus(Fixture.World, TEXT("Priority10")); AddRecordTrigger(A, Recorder, 10, 10);
		UStatusData* B = CreateStatus(Fixture.World, TEXT("Priority0")); AddRecordTrigger(B, Recorder, 0, 0);
		UStatusData* C = CreateStatus(Fixture.World, TEXT("Priority5")); AddRecordTrigger(C, Recorder, 5, 5);
		ApplyStatus(Fixture.Player, A, 3);
		ApplyStatus(Fixture.Player, B, 1);
		ApplyStatus(Fixture.Player, C, 2);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		UBattleEventDispatcher* Dispatcher = NewObject<UBattleEventDispatcher>(Fixture.World);
		TArray<FTriggerEligibilityRecord> EligibilityTrace;
		TestTrue(TEXT("Dispatch succeeds"), Dispatcher->Dispatch(
			FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture), &EligibilityTrace));
		TestTrue(TEXT("Queue starts"), Queue->StartProcessing());

		ExpectValues(*this, TEXT("Actual priority execution"), Recorder, {0, 5, 10});
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AActualRuntimeSequenceExecutionOrderingTest,
		"SlayTheSpireDemo.Phase6A.Trigger.ActualRuntimeSequenceExecutionOrdering",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AActualRuntimeSequenceExecutionOrderingTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UPhase6ATestExecutionRecorder* Recorder = NewObject<UPhase6ATestExecutionRecorder>(Fixture.World);
		UStatusData* Seq3 = CreateStatus(Fixture.World, TEXT("Seq3")); AddRecordTrigger(Seq3, Recorder, 3);
		UStatusData* Seq1 = CreateStatus(Fixture.World, TEXT("Seq1")); AddRecordTrigger(Seq1, Recorder, 1);
		UStatusData* Seq2 = CreateStatus(Fixture.World, TEXT("Seq2")); AddRecordTrigger(Seq2, Recorder, 2);
		ApplyStatus(Fixture.Player, Seq3, 3);
		ApplyStatus(Fixture.Player, Seq1, 1);
		ApplyStatus(Fixture.Player, Seq2, 2);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		UBattleEventDispatcher* Dispatcher = NewObject<UBattleEventDispatcher>(Fixture.World);
		TestTrue(TEXT("Dispatch succeeds"), Dispatcher->Dispatch(
			FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture)));
		TestTrue(TEXT("Queue starts"), Queue->StartProcessing());

		ExpectValues(*this, TEXT("Actual RuntimeSequence execution"), Recorder, {1, 2, 3});
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AActualLocalTriggerIndexExecutionOrderingTest,
		"SlayTheSpireDemo.Phase6A.Trigger.ActualLocalTriggerIndexExecutionOrdering",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AActualLocalTriggerIndexExecutionOrderingTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UPhase6ATestExecutionRecorder* Recorder = NewObject<UPhase6ATestExecutionRecorder>(Fixture.World);
		UStatusData* Status = CreateStatus(Fixture.World, TEXT("ThreeLocalTriggers"));
		AddRecordTrigger(Status, Recorder, 1);
		AddRecordTrigger(Status, Recorder, 2);
		AddRecordTrigger(Status, Recorder, 3);
		ApplyStatus(Fixture.Player, Status, 1);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		UBattleEventDispatcher* Dispatcher = NewObject<UBattleEventDispatcher>(Fixture.World);
		TestTrue(TEXT("Dispatch succeeds"), Dispatcher->Dispatch(
			FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture)));
		TestTrue(TEXT("Queue starts"), Queue->StartProcessing());

		ExpectValues(*this, TEXT("Actual LocalTriggerIndex execution"), Recorder, {1, 2, 3});
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AActualCollectionOrderDoesNotMatterTest,
		"SlayTheSpireDemo.Phase6A.Trigger.ActualCollectionOrderDoesNotMatter",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AActualCollectionOrderDoesNotMatterTest::RunTest(const FString& Parameters)
	{
		FFixture First;
		FFixture Second;
		if (!RequireReady(*this, First) || !RequireReady(*this, Second)) return false;

		UPhase6ATestExecutionRecorder* FirstRecorder = NewObject<UPhase6ATestExecutionRecorder>(First.World);
		UStatusData* FirstA = CreateStatus(First.World, TEXT("A")); AddRecordTrigger(FirstA, FirstRecorder, 3);
		UStatusData* FirstB = CreateStatus(First.World, TEXT("B")); AddRecordTrigger(FirstB, FirstRecorder, 1);
		UStatusData* FirstC = CreateStatus(First.World, TEXT("C")); AddRecordTrigger(FirstC, FirstRecorder, 2);
		ApplyStatus(First.Player, FirstA, 3);
		ApplyStatus(First.Player, FirstB, 1);
		ApplyStatus(First.Player, FirstC, 2);

		UPhase6ATestExecutionRecorder* SecondRecorder = NewObject<UPhase6ATestExecutionRecorder>(Second.World);
		UStatusData* SecondC = CreateStatus(Second.World, TEXT("C")); AddRecordTrigger(SecondC, SecondRecorder, 2);
		UStatusData* SecondB = CreateStatus(Second.World, TEXT("B")); AddRecordTrigger(SecondB, SecondRecorder, 1);
		UStatusData* SecondA = CreateStatus(Second.World, TEXT("A")); AddRecordTrigger(SecondA, SecondRecorder, 3);
		ApplyStatus(Second.Player, SecondC, 2);
		ApplyStatus(Second.Player, SecondB, 1);
		ApplyStatus(Second.Player, SecondA, 3);

		UBattleEventDispatcher* FirstDispatcher = NewObject<UBattleEventDispatcher>(First.World);
		UBattleActionQueue* FirstQueue = NewObject<UBattleActionQueue>(First.World);
		FirstDispatcher->Dispatch(FBattleEvent::MakeTurnEnded(First.Player), FirstQueue, BothCombatants(First));
		FirstQueue->StartProcessing();

		UBattleEventDispatcher* SecondDispatcher = NewObject<UBattleEventDispatcher>(Second.World);
		UBattleActionQueue* SecondQueue = NewObject<UBattleActionQueue>(Second.World);
		SecondDispatcher->Dispatch(FBattleEvent::MakeTurnEnded(Second.Player), SecondQueue, BothCombatants(Second));
		SecondQueue->StartProcessing();

		ExpectValues(*this, TEXT("First actual collection-independent execution"), FirstRecorder, {1, 2, 3});
		ExpectValues(*this, TEXT("Second actual collection-independent execution"), SecondRecorder, {1, 2, 3});
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6ANestedReactionDepthFirstExecuteTimeDispatchTest,
		"SlayTheSpireDemo.Phase6A.Trigger.NestedReactionDepthFirstExecuteTimeDispatch",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6ANestedReactionDepthFirstExecuteTimeDispatchTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UPhase6ATestExecutionRecorder* Recorder = NewObject<UPhase6ATestExecutionRecorder>(Fixture.World);
		UBattleEventDispatcher* Dispatcher = NewObject<UBattleEventDispatcher>(Fixture.World);
		const TArray<ACombatant*> Combatants = BothCombatants(Fixture);

		UStatusData* PlayerNested = CreateStatus(Fixture.World, TEXT("PlayerNested"));
		AddNestedTrigger(PlayerNested, Recorder, Dispatcher, Fixture.Enemy, Combatants, 1, 2);
		ApplyStatus(Fixture.Player, PlayerNested, 1);

		UStatusData* EnemyNestedReaction = CreateStatus(Fixture.World, TEXT("EnemyNestedReaction"));
		AddRecordTrigger(EnemyNestedReaction, Recorder, 3);
		ApplyStatus(Fixture.Enemy, EnemyNestedReaction, 2);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		TestTrue(TEXT("Outer TurnEnded dispatch succeeds"), Dispatcher->Dispatch(
			FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, Combatants));
		TestTrue(TEXT("Queue starts"), Queue->StartProcessing());

		// A1 commits Record(1), dispatches Event B before Finish(), Event B inserts
		// B1 Record(3) at the front, then sibling A2 Record(2) continues.
		ExpectValues(*this, TEXT("Execute-time nested depth-first execution"), Recorder, {1, 3, 2});
		return true;
	}
}

#endif
