#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Actions/BattleAction.h"
#include "../Actions/BattleActionQueue.h"
#include "../Actions/DamageAction.h"
#include "../Actions/GainBlockAction.h"
#include "../Combat/Combatant.h"
#include "../Events/BattleEvent.h"
#include "../Events/BattleEventDispatcher.h"
#include "../Events/TurnEndStatusDecayTrigger.h"
#include "../Modifiers/Damage/DamageFlatAddModifier.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusData.h"
#include "../Status/StatusInstance.h"
#include "Engine/World.h"

namespace Phase6ARegression
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
			Test.AddError(TEXT("Failed to create the transient Phase 6A automation-test fixture."));
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

	UTurnEndStatusDecayTrigger* AddDecayTrigger(
		UStatusData* Definition,
		int32 Priority = 0,
		int32 AmountToRemove = 1
	)
	{
		UTurnEndStatusDecayTrigger* Trigger = NewObject<UTurnEndStatusDecayTrigger>(Definition);
		Trigger->Priority = Priority;
		Trigger->AmountToRemove = AmountToRemove;
		Definition->Triggers.Add(Trigger);
		return Trigger;
	}

	UDamageFlatAddModifier* AddDamageFlat(UStatusData* Definition, int32 Value = 1)
	{
		UDamageFlatAddModifier* Modifier = NewObject<UDamageFlatAddModifier>(Definition);
		Modifier->Scope = EModifierScope::Source;
		Modifier->Priority = 0;
		Modifier->ApplicableDamageKind = EDamageKind::Attack;
		Modifier->Value = Value;
		Modifier->AmountMode = EModifierAmountMode::ScaleWithAmount;
		Definition->DamageModifiers.Add(Modifier);
		return Modifier;
	}

	UStatusInstance* ApplyStatus(
		ACombatant* Combatant,
		UStatusData* Definition,
		int32 Amount,
		uint64 RuntimeSequence
	)
	{
		bool bCreated = false;
		return Combatant->GetStatusContainer()->ApplyStatus(Definition, Amount, RuntimeSequence, bCreated);
	}

	TArray<ACombatant*> BothCombatants(const FFixture& Fixture)
	{
		TArray<ACombatant*> Result;
		Result.Add(Fixture.Player);
		Result.Add(Fixture.Enemy);
		return Result;
	}

	UBattleEventDispatcher* CreateDispatcher(const FFixture& Fixture)
	{
		return NewObject<UBattleEventDispatcher>(Fixture.World);
	}

	UDamageAction* MakeDamage(UBattleActionQueue* Queue, ACombatant* Source, ACombatant* Target, int32 Amount)
	{
		UDamageAction* Action = NewObject<UDamageAction>(Queue);
		Action->Initialize(Source, Target, Amount, EDamageKind::Attack);
		return Action;
	}

	UGainBlockAction* MakeBlock(UBattleActionQueue* Queue, ACombatant* Target, int32 Amount)
	{
		UGainBlockAction* Action = NewObject<UGainBlockAction>(Queue);
		Action->Initialize(Target, Target, Amount);
		return Action;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AQueueBatchFrontPreservesOrderTest,
		"SlayTheSpireDemo.Phase6A.Queue.BatchFrontPreservesOrder",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AQueueBatchFrontPreservesOrderTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		Queue->AddToBack(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 4));

		TArray<UBattleAction*> Batch;
		Batch.Add(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 7));
		Batch.Add(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 3));
		Batch.Add(MakeBlock(Queue, Fixture.Enemy, 5));

		TestTrue(TEXT("Front batch accepted"), Queue->AddBatchToFrontPreserveOrder(Batch));
		Queue->StartProcessing();

		TestEqual(TEXT("Preserved front order leaves HP at 90"), Fixture.Enemy->HP, 90);
		TestEqual(TEXT("Preserved front order leaves one Block"), Fixture.Enemy->Block, 1);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AQueueBatchBackPreservesOrderTest,
		"SlayTheSpireDemo.Phase6A.Queue.BatchBackPreservesOrder",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AQueueBatchBackPreservesOrderTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		Queue->AddToBack(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 4));

		TArray<UBattleAction*> Batch;
		Batch.Add(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 7));
		Batch.Add(MakeBlock(Queue, Fixture.Enemy, 5));
		Batch.Add(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 3));

		TestTrue(TEXT("Back batch accepted"), Queue->AddBatchToBackPreserveOrder(Batch));
		Queue->StartProcessing();

		TestEqual(TEXT("Preserved back order leaves HP at 89"), Fixture.Enemy->HP, 89);
		TestEqual(TEXT("Preserved back order leaves two Block"), Fixture.Enemy->Block, 2);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AQueueBatchInsertionIsAtomicTest,
		"SlayTheSpireDemo.Phase6A.Queue.BatchInsertionIsAtomic",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AQueueBatchInsertionIsAtomicTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		Queue->AddToBack(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 2));
		const int32 BeforeCount = Queue->GetPendingCount();

		UDamageAction* Valid = MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 3);
		UDamageAction* WrongOuter = NewObject<UDamageAction>(Fixture.World);
		WrongOuter->Initialize(Fixture.Player, Fixture.Enemy, 4, EDamageKind::Attack);

		TArray<UBattleAction*> Batch{Valid, WrongOuter};
		TestFalse(TEXT("Mixed-validity batch is rejected"), Queue->AddBatchToFrontPreserveOrder(Batch));
		TestEqual(TEXT("Rejected batch leaves Pending unchanged"), Queue->GetPendingCount(), BeforeCount);

		Queue->StartProcessing();
		TestEqual(TEXT("Only the pre-existing Action executes"), Fixture.Enemy->HP, 98);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AQueueBatchRejectsDuplicateAndAlreadyQueuedActionsTest,
		"SlayTheSpireDemo.Phase6A.Queue.BatchRejectsDuplicateAndAlreadyQueuedActions",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AQueueBatchRejectsDuplicateAndAlreadyQueuedActionsTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		UDamageAction* Existing = MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 2);
		UDamageAction* Fresh = MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 3);
		Queue->AddToBack(Existing);

		TArray<UBattleAction*> AlreadyQueued{Existing, Fresh};
		TestFalse(TEXT("Batch cannot reuse an already-pending Action"), Queue->AddBatchToBackPreserveOrder(AlreadyQueued));
		TestEqual(TEXT("Pending count remains one"), Queue->GetPendingCount(), 1);

		TArray<UBattleAction*> Duplicate{Fresh, Fresh};
		TestFalse(TEXT("Batch cannot repeat the same Action pointer"), Queue->AddBatchToBackPreserveOrder(Duplicate));
		TestEqual(TEXT("Duplicate rejection is atomic"), Queue->GetPendingCount(), 1);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AQueueFaultDoesNotBroadcastQueueEmptyTest,
		"SlayTheSpireDemo.Phase6A.Queue.FaultDoesNotBroadcastQueueEmpty",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AQueueFaultDoesNotBroadcastQueueEmptyTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		Queue->SetMaxActionsPerResolutionForTesting(2);
		int32 EmptyCount = 0;
		int32 FaultCount = 0;
		Queue->OnQueueEmpty.AddLambda([&EmptyCount]() { ++EmptyCount; });
		Queue->OnResolutionFaulted.AddLambda(
			[&FaultCount](const FString&, int32, UBattleAction*) { ++FaultCount; }
		);

		TArray<UBattleAction*> Batch;
		Batch.Add(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 1));
		Batch.Add(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 1));
		Batch.Add(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 1));
		Queue->AddBatchToBackPreserveOrder(Batch);
		Queue->StartProcessing();

		TestTrue(TEXT("Queue enters resolution fault"), Queue->IsResolutionFaulted());
		TestEqual(TEXT("Fault broadcasts exactly once"), FaultCount, 1);
		TestEqual(TEXT("Fault never broadcasts normal QueueEmpty"), EmptyCount, 0);
		TestEqual(TEXT("Only two actions execute before the budget fault"), Fixture.Enemy->HP, 98);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AQueueFaultRejectsFurtherMutationTest,
		"SlayTheSpireDemo.Phase6A.Queue.FaultRejectsFurtherMutation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AQueueFaultRejectsFurtherMutationTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		Queue->SetMaxActionsPerResolutionForTesting(1);
		TArray<UBattleAction*> Initial;
		Initial.Add(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 1));
		Initial.Add(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 1));
		Queue->AddBatchToBackPreserveOrder(Initial);
		Queue->StartProcessing();
		TestTrue(TEXT("Precondition: Queue faulted"), Queue->IsResolutionFaulted());

		UDamageAction* Later = MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 10);
		TestFalse(TEXT("Faulted Queue rejects single add"), Queue->AddToBack(Later));
		TArray<UBattleAction*> LaterBatch{Later};
		TestFalse(TEXT("Faulted Queue rejects batch add"), Queue->AddBatchToBackPreserveOrder(LaterBatch));
		TestFalse(TEXT("Faulted Queue rejects StartProcessing"), Queue->StartProcessing());
		TestEqual(TEXT("Fault clears/isolates pending work"), Queue->GetPendingCount(), 0);
		TestEqual(TEXT("No later mutation executes"), Fixture.Enemy->HP, 99);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6ATriggerPriorityOrderingTest,
		"SlayTheSpireDemo.Phase6A.Trigger.PriorityOrdering",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6ATriggerPriorityOrderingTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* A = CreateStatus(Fixture.World, TEXT("A")); AddDecayTrigger(A, 10);
		UStatusData* B = CreateStatus(Fixture.World, TEXT("B")); AddDecayTrigger(B, 0);
		UStatusData* C = CreateStatus(Fixture.World, TEXT("C")); AddDecayTrigger(C, 5);
		ApplyStatus(Fixture.Player, A, 2, 3);
		ApplyStatus(Fixture.Player, B, 2, 1);
		ApplyStatus(Fixture.Player, C, 2, 2);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		TArray<FTriggerDispatchRecord> Trace;
		CreateDispatcher(Fixture)->Dispatch(FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture), &Trace);

		TestEqual(TEXT("Three eligible triggers"), Trace.Num(), 3);
		if (Trace.Num() == 3)
		{
			TestEqual(TEXT("Priority 0 first"), Trace[0].Priority, 0);
			TestEqual(TEXT("Priority 5 second"), Trace[1].Priority, 5);
			TestEqual(TEXT("Priority 10 third"), Trace[2].Priority, 10);
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6ATriggerRuntimeSequenceOrderingTest,
		"SlayTheSpireDemo.Phase6A.Trigger.RuntimeSequenceOrdering",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6ATriggerRuntimeSequenceOrderingTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* A = CreateStatus(Fixture.World, TEXT("A")); AddDecayTrigger(A);
		UStatusData* B = CreateStatus(Fixture.World, TEXT("B")); AddDecayTrigger(B);
		UStatusData* C = CreateStatus(Fixture.World, TEXT("C")); AddDecayTrigger(C);
		ApplyStatus(Fixture.Player, A, 2, 3);
		ApplyStatus(Fixture.Player, B, 2, 1);
		ApplyStatus(Fixture.Player, C, 2, 2);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		TArray<FTriggerDispatchRecord> Trace;
		CreateDispatcher(Fixture)->Dispatch(FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture), &Trace);

		TestEqual(TEXT("Three eligible triggers"), Trace.Num(), 3);
		if (Trace.Num() == 3)
		{
			TestEqual(TEXT("Sequence 1 first"), Trace[0].RuntimeSequence, 1ull);
			TestEqual(TEXT("Sequence 2 second"), Trace[1].RuntimeSequence, 2ull);
			TestEqual(TEXT("Sequence 3 third"), Trace[2].RuntimeSequence, 3ull);
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6ATriggerLocalTriggerIndexOrderingTest,
		"SlayTheSpireDemo.Phase6A.Trigger.LocalTriggerIndexOrdering",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6ATriggerLocalTriggerIndexOrderingTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* Status = CreateStatus(Fixture.World, TEXT("MultiTrigger"));
		AddDecayTrigger(Status, 0); AddDecayTrigger(Status, 0); AddDecayTrigger(Status, 0);
		ApplyStatus(Fixture.Player, Status, 5, 1);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		TArray<FTriggerDispatchRecord> Trace;
		CreateDispatcher(Fixture)->Dispatch(FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture), &Trace);

		TestEqual(TEXT("Three eligible local triggers"), Trace.Num(), 3);
		if (Trace.Num() == 3)
		{
			TestEqual(TEXT("Local index 0 first"), Trace[0].LocalTriggerIndex, 0);
			TestEqual(TEXT("Local index 1 second"), Trace[1].LocalTriggerIndex, 1);
			TestEqual(TEXT("Local index 2 third"), Trace[2].LocalTriggerIndex, 2);
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6ATriggerCollectionOrderDoesNotMatterTest,
		"SlayTheSpireDemo.Phase6A.Trigger.CollectionOrderDoesNotMatter",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6ATriggerCollectionOrderDoesNotMatterTest::RunTest(const FString& Parameters)
	{
		FFixture First;
		FFixture Second;
		if (!RequireReady(*this, First) || !RequireReady(*this, Second)) return false;

		auto BuildDefinitions = [](UObject* Outer, UStatusData*& A, UStatusData*& B, UStatusData*& C)
		{
			A = CreateStatus(Outer, TEXT("A")); AddDecayTrigger(A);
			B = CreateStatus(Outer, TEXT("B")); AddDecayTrigger(B);
			C = CreateStatus(Outer, TEXT("C")); AddDecayTrigger(C);
		};

		UStatusData *A1, *B1, *C1; BuildDefinitions(First.World, A1, B1, C1);
		ApplyStatus(First.Player, A1, 2, 3);
		ApplyStatus(First.Player, B1, 2, 1);
		ApplyStatus(First.Player, C1, 2, 2);

		UStatusData *A2, *B2, *C2; BuildDefinitions(Second.World, A2, B2, C2);
		ApplyStatus(Second.Player, C2, 2, 2);
		ApplyStatus(Second.Player, B2, 2, 1);
		ApplyStatus(Second.Player, A2, 2, 3);

		TArray<FTriggerDispatchRecord> FirstTrace;
		TArray<FTriggerDispatchRecord> SecondTrace;
		CreateDispatcher(First)->Dispatch(FBattleEvent::MakeTurnEnded(First.Player), NewObject<UBattleActionQueue>(First.World), BothCombatants(First), &FirstTrace);
		CreateDispatcher(Second)->Dispatch(FBattleEvent::MakeTurnEnded(Second.Player), NewObject<UBattleActionQueue>(Second.World), BothCombatants(Second), &SecondTrace);

		TestEqual(TEXT("Trace counts match"), FirstTrace.Num(), SecondTrace.Num());
		if (FirstTrace.Num() == 3 && SecondTrace.Num() == 3)
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TestEqual(FString::Printf(TEXT("Stable sequence at index %d"), Index), FirstTrace[Index].RuntimeSequence, SecondTrace[Index].RuntimeSequence);
				TestEqual(FString::Printf(TEXT("Stable StatusId at index %d"), Index), FirstTrace[Index].StatusId, SecondTrace[Index].StatusId);
			}
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6ATriggerReactionBeforeExistingPendingTest,
		"SlayTheSpireDemo.Phase6A.Trigger.ReactionBeforeExistingPending",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6ATriggerReactionBeforeExistingPendingTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* StrengthDecay = CreateStatus(Fixture.World, TEXT("StrengthDecay"));
		AddDamageFlat(StrengthDecay);
		AddDecayTrigger(StrengthDecay);
		UStatusInstance* Instance = ApplyStatus(Fixture.Player, StrengthDecay, 2, 1);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		Queue->AddToBack(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 0));
		TestTrue(
			TEXT("TurnEnded dispatch succeeds"),
			CreateDispatcher(Fixture)->Dispatch(FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture))
		);
		Queue->StartProcessing();

		TestEqual(TEXT("Reaction decays Strength before pending Damage resolves"), Fixture.Enemy->HP, 99);
		TestEqual(TEXT("Status Amount is reduced first"), Instance ? Instance->GetAmount() : 0, 1);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6ATriggerNestedReactionDepthFirstTest,
		"SlayTheSpireDemo.Phase6A.Trigger.NestedReactionDepthFirst",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6ATriggerNestedReactionDepthFirstTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* StrengthDecay = CreateStatus(Fixture.World, TEXT("StrengthDecay"));
		AddDamageFlat(StrengthDecay);
		AddDecayTrigger(StrengthDecay);
		ApplyStatus(Fixture.Player, StrengthDecay, 2, 1);

		UBattleEventDispatcher* Dispatcher = CreateDispatcher(Fixture);
		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		UGainBlockAction* A1 = MakeBlock(Queue, Fixture.Player, 1);
		UDamageAction* A2 = MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 0);
		TArray<UBattleAction*> SiblingBatch{A1, A2};
		Queue->AddBatchToBackPreserveOrder(SiblingBatch);

		const FBattleEvent NestedEvent = FBattleEvent::MakeTurnEnded(Fixture.Player);
		const TArray<ACombatant*> Combatants = BothCombatants(Fixture);
		A1->OnFinished.AddLambda(
			[Dispatcher, Queue, NestedEvent, Combatants](UBattleAction*)
			{
				Dispatcher->Dispatch(NestedEvent, Queue, Combatants);
			}
		);

		Queue->StartProcessing();
		TestEqual(TEXT("Nested reaction executes before sibling A2"), Fixture.Enemy->HP, 99);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AStatusTurnEndDecayTest,
		"SlayTheSpireDemo.Phase6A.Status.TurnEndDecay",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AStatusTurnEndDecayTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* Weak = CreateStatus(Fixture.World, TEXT("Weak")); AddDecayTrigger(Weak);
		UStatusInstance* Instance = ApplyStatus(Fixture.Player, Weak, 2, 1);
		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		CreateDispatcher(Fixture)->Dispatch(FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture));
		Queue->StartProcessing();

		TestEqual(TEXT("Weak Amount 2 decays to 1"), Instance ? Instance->GetAmount() : 0, 1);
		TestTrue(TEXT("Exact instance remains in container"), Fixture.Player->GetStatusContainer()->ContainsStatusInstance(Instance));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AStatusTurnEndDecayRemovesExactInstanceAtZeroTest,
		"SlayTheSpireDemo.Phase6A.Status.TurnEndDecayRemovesExactInstanceAtZero",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AStatusTurnEndDecayRemovesExactInstanceAtZeroTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* Weak = CreateStatus(Fixture.World, TEXT("Weak")); AddDecayTrigger(Weak);
		UStatusInstance* Instance = ApplyStatus(Fixture.Player, Weak, 1, 1);
		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		CreateDispatcher(Fixture)->Dispatch(FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture));
		Queue->StartProcessing();

		TestFalse(TEXT("Weak exact instance removed at zero"), Fixture.Player->GetStatusContainer()->ContainsStatusInstance(Instance));
		TestNull(TEXT("Weak no longer found by id"), Fixture.Player->GetStatusContainer()->FindStatusById(TEXT("Weak")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AStatusRemovedAndRecreatedInstanceIsNotReducedTest,
		"SlayTheSpireDemo.Phase6A.Status.RemovedAndRecreatedInstanceIsNotReduced",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AStatusRemovedAndRecreatedInstanceIsNotReducedTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* Weak = CreateStatus(Fixture.World, TEXT("Weak")); AddDecayTrigger(Weak);
		UStatusInstance* OldInstance = ApplyStatus(Fixture.Player, Weak, 1, 3);
		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		CreateDispatcher(Fixture)->Dispatch(FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture));

		TestTrue(TEXT("Old Weak removed before its reaction executes"), Fixture.Player->GetStatusContainer()->RemoveStatusById(TEXT("Weak")));
		UStatusInstance* NewInstance = ApplyStatus(Fixture.Player, Weak, 5, 8);
		TestTrue(TEXT("Recreated Weak is a different runtime instance"), OldInstance != nullptr && NewInstance != nullptr && OldInstance != NewInstance);
		Queue->StartProcessing();

		TestEqual(TEXT("New Weak Amount is not reduced by stale reaction"), NewInstance ? NewInstance->GetAmount() : 0, 5);
		TestEqual(TEXT("New Weak keeps its new RuntimeSequence"), NewInstance ? NewInstance->GetRuntimeSequence() : 0ull, 8ull);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AStatusOtherActorsTurnDoesNotDecayTest,
		"SlayTheSpireDemo.Phase6A.Status.OtherActorsTurnDoesNotDecay",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AStatusOtherActorsTurnDoesNotDecayTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* PlayerWeak = CreateStatus(Fixture.World, TEXT("PlayerWeak")); AddDecayTrigger(PlayerWeak);
		UStatusData* EnemyWeak = CreateStatus(Fixture.World, TEXT("EnemyWeak")); AddDecayTrigger(EnemyWeak);
		UStatusInstance* PlayerInstance = ApplyStatus(Fixture.Player, PlayerWeak, 2, 1);
		UStatusInstance* EnemyInstance = ApplyStatus(Fixture.Enemy, EnemyWeak, 2, 2);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		CreateDispatcher(Fixture)->Dispatch(FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture));
		Queue->StartProcessing();

		TestEqual(TEXT("Turn owner status decays"), PlayerInstance ? PlayerInstance->GetAmount() : 0, 1);
		TestEqual(TEXT("Other actor status does not decay"), EnemyInstance ? EnemyInstance->GetAmount() : 0, 2);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6AStatusSnapshotEligibilityVsLiveActionValidationTest,
		"SlayTheSpireDemo.Phase6A.Status.SnapshotEligibilityVsLiveActionValidation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6AStatusSnapshotEligibilityVsLiveActionValidationTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* Weak = CreateStatus(Fixture.World, TEXT("Weak")); AddDecayTrigger(Weak);
		UStatusInstance* Instance = ApplyStatus(Fixture.Player, Weak, 2, 1);
		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		TArray<FTriggerDispatchRecord> Trace;
		CreateDispatcher(Fixture)->Dispatch(FBattleEvent::MakeTurnEnded(Fixture.Player), Queue, BothCombatants(Fixture), &Trace);

		TestEqual(TEXT("Eligibility snapshot builds one reaction"), Trace.Num(), 1);
		TestEqual(TEXT("One reaction is pending"), Queue->GetPendingCount(), 1);
		TestTrue(TEXT("Source can be removed after snapshot"), Fixture.Player->GetStatusContainer()->RemoveStatusById(TEXT("Weak")));
		Queue->StartProcessing();

		TestFalse(TEXT("Live exact-instance validation sees source is no longer a member"), Fixture.Player->GetStatusContainer()->ContainsStatusInstance(Instance));
		TestFalse(TEXT("Stale reaction does not fault the queue"), Queue->IsResolutionFaulted());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6ASafetyResolutionBudgetFaultsInsteadOfLoopingForeverTest,
		"SlayTheSpireDemo.Phase6A.Safety.ResolutionBudgetFaultsInsteadOfLoopingForever",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6ASafetyResolutionBudgetFaultsInsteadOfLoopingForeverTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		Queue->SetMaxActionsPerResolutionForTesting(3);
		TArray<UBattleAction*> Batch;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			Batch.Add(MakeDamage(Queue, Fixture.Player, Fixture.Enemy, 1));
		}
		Queue->AddBatchToBackPreserveOrder(Batch);
		Queue->StartProcessing();

		TestTrue(TEXT("Budget overflow enters fault instead of continuing"), Queue->IsResolutionFaulted());
		TestEqual(TEXT("Exactly three actions execute"), Fixture.Enemy->HP, 97);
		TestTrue(TEXT("Fault reason identifies budget overflow"), Queue->GetResolutionFaultReason().Contains(TEXT("budget")));
		TestEqual(TEXT("Fault isolates remaining pending actions"), Queue->GetPendingCount(), 0);
		return true;
	}
}

#endif
