#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/ReduceStatusAction.h"
#include "Actions/RemoveStatusAction.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Status/StatusInstance.h"
#include "Status/StatusMutationTypes.h"

namespace Phase6UIA2D1Test
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ABattleManager* Battle = nullptr;
		ACombatant* Target = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			Target = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Battle) || !IsValid(Target))
			{
				return;
			}

			Target->MaxHP = 100;
			Target->InitializeCombatant();
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
				&& IsValid(Battle)
				&& IsValid(Target)
				&& IsValid(Target->GetStatusContainer());
		}
	};

	UStatusData* CreateStatus(UObject* Outer, const TCHAR* StatusId)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		if (IsValid(Definition))
		{
			Definition->StatusId = FName(StatusId);
			Definition->DisplayName = FText::FromString(StatusId);
		}
		return Definition;
	}

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (Fixture.IsReady())
		{
			return true;
		}
		Test.AddError(TEXT("Failed to create the Phase 6UI-A2D1 status mutation fixture."));
		return false;
	}
}

using namespace Phase6UIA2D1Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D1StatusMutationLifecycleTest,
	"SlayTheSpireDemo.Phase6UIA2D1.Commit.StatusMutationLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D1StatusMutationLifecycleTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusContainer* Container = Fixture.Target->GetStatusContainer();
	UStatusData* WeakDefinition = CreateStatus(Fixture.World, TEXT("Weak"));
	UStatusData* AlternateWeakDefinition = CreateStatus(Fixture.World, TEXT("Weak"));
	if (!TestNotNull(TEXT("Weak definition"), WeakDefinition)
		|| !TestNotNull(TEXT("Alternate Weak definition"), AlternateWeakDefinition))
	{
		return false;
	}

	const FStatusMutationResult Create = Container->ApplyStatusCommit(WeakDefinition, 2, 10);
	TestTrue(TEXT("Create commits"), Create.Outcome == EStatusMutationOutcome::Committed);
	TestTrue(TEXT("Create marks membership creation"), Create.bCreated);
	TestFalse(TEXT("Create is not removal"), Create.bRemoved);
	TestEqual(TEXT("Create StatusId"), Create.StatusId, FName(TEXT("Weak")));
	TestEqual(TEXT("Create RuntimeSequence"), Create.RuntimeSequence, 10ull);
	TestEqual(TEXT("Create amount before"), Create.AmountBefore, 0);
	TestEqual(TEXT("Create amount after"), Create.AmountAfter, 2);
	TestTrue(TEXT("Create uses requested definition"), Create.EffectiveDefinition == WeakDefinition);
	UStatusInstance* Weak10 = Create.EffectiveInstance;
	if (!TestNotNull(TEXT("Created Weak#10 instance"), Weak10))
	{
		return false;
	}

	const FStatusMutationResult Merge = Container->ApplyStatusCommit(AlternateWeakDefinition, 3, 11);
	TestTrue(TEXT("Merge commits"), Merge.Outcome == EStatusMutationOutcome::Committed);
	TestFalse(TEXT("Merge does not create a second instance"), Merge.bCreated);
	TestFalse(TEXT("Merge does not remove"), Merge.bRemoved);
	TestEqual(TEXT("Merge amount before"), Merge.AmountBefore, 2);
	TestEqual(TEXT("Merge amount after"), Merge.AmountAfter, 5);
	TestEqual(TEXT("Merge retains existing RuntimeSequence"), Merge.RuntimeSequence, 10ull);
	TestTrue(TEXT("Merge retains exact existing runtime instance"), Merge.EffectiveInstance == Weak10);
	TestTrue(TEXT("Merge retains existing effective definition"), Merge.EffectiveDefinition == WeakDefinition);
	TestEqual(TEXT("Merge candidate does not create a second member"), Container->GetStatuses().Num(), 1);

	const FStatusMutationResult FillToMax = Container->ApplyStatusCommit(WeakDefinition, MAX_int32 - 5, 12);
	TestTrue(TEXT("Fill to MAX_int32 commits"), FillToMax.Outcome == EStatusMutationOutcome::Committed);
	TestEqual(TEXT("Fill to max amount before"), FillToMax.AmountBefore, 5);
	TestEqual(TEXT("Fill to max amount after"), FillToMax.AmountAfter, MAX_int32);
	TestEqual(TEXT("Fill to max retains RuntimeSequence"), FillToMax.RuntimeSequence, 10ull);

	const FStatusMutationResult MaxNoOp = Container->ApplyStatusCommit(WeakDefinition, 1, 13);
	TestTrue(TEXT("MAX_int32 increase is NoOp"), MaxNoOp.Outcome == EStatusMutationOutcome::NoOp);
	TestFalse(TEXT("MAX_int32 no-op is not created"), MaxNoOp.bCreated);
	TestFalse(TEXT("MAX_int32 no-op is not removed"), MaxNoOp.bRemoved);
	TestEqual(TEXT("MAX_int32 no-op amount before"), MaxNoOp.AmountBefore, MAX_int32);
	TestEqual(TEXT("MAX_int32 no-op amount after"), MaxNoOp.AmountAfter, MAX_int32);
	TestEqual(TEXT("MAX_int32 no-op reports existing RuntimeSequence"), MaxNoOp.RuntimeSequence, 10ull);
	TestTrue(TEXT("MAX_int32 no-op retains existing instance"), MaxNoOp.EffectiveInstance == Weak10);

	const FStatusMutationResult PartialReduce = Container->ReduceStatusCommit(Weak10, MAX_int32 - 5);
	TestTrue(TEXT("Partial reduce commits"), PartialReduce.Outcome == EStatusMutationOutcome::Committed);
	TestFalse(TEXT("Partial reduce keeps membership"), PartialReduce.bRemoved);
	TestEqual(TEXT("Partial reduce amount before"), PartialReduce.AmountBefore, MAX_int32);
	TestEqual(TEXT("Partial reduce amount after"), PartialReduce.AmountAfter, 5);
	TestTrue(TEXT("Partial reduce keeps exact instance in container"), Container->ContainsStatusInstance(Weak10));

	const FStatusMutationResult ReduceRemove = Container->ReduceStatusCommit(Weak10, 5);
	TestTrue(TEXT("Reduce-to-zero commits"), ReduceRemove.Outcome == EStatusMutationOutcome::Committed);
	TestTrue(TEXT("Reduce-to-zero marks removal"), ReduceRemove.bRemoved);
	TestFalse(TEXT("Reduce-to-zero is not creation"), ReduceRemove.bCreated);
	TestEqual(TEXT("Reduce-to-zero amount before"), ReduceRemove.AmountBefore, 5);
	TestEqual(TEXT("Reduce-to-zero amount after"), ReduceRemove.AmountAfter, 0);
	TestFalse(TEXT("Removed exact instance leaves container"), Container->ContainsStatusInstance(Weak10));
	TestEqual(TEXT("Container empty after exact reduce removal"), Container->GetStatuses().Num(), 0);

	const FStatusMutationResult StaleReduce = Container->ReduceStatusCommit(Weak10, 1);
	TestTrue(TEXT("Stale exact reduce is NoOp"), StaleReduce.Outcome == EStatusMutationOutcome::NoOp);
	TestFalse(TEXT("Stale reduce does not recreate membership"), Container->ContainsStatusInstance(Weak10));
	TestEqual(TEXT("Stale reduce leaves container empty"), Container->GetStatuses().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D1StaleReduceActionDoesNotRetargetTest,
	"SlayTheSpireDemo.Phase6UIA2D1.Action.StaleReduceDoesNotRetarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D1StaleReduceActionDoesNotRetargetTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusContainer* Container = Fixture.Target->GetStatusContainer();
	UStatusData* WeakDefinition = CreateStatus(Fixture.World, TEXT("Weak"));
	const FStatusMutationResult CreateOld = Container->ApplyStatusCommit(WeakDefinition, 2, 20);
	UStatusInstance* Weak20 = CreateOld.EffectiveInstance;
	if (!TestNotNull(TEXT("Old exact Weak instance"), Weak20))
	{
		return false;
	}

	UBattleActionQueue* StaleQueue = NewObject<UBattleActionQueue>(Fixture.Battle);
	UReduceStatusAction* StaleReduce = NewObject<UReduceStatusAction>(StaleQueue);
	StaleReduce->Initialize(
		Fixture.Battle,
		Fixture.Target,
		Fixture.Target,
		Weak20,
		1,
		EStatusChangeReason::Reduced
	);
	TestTrue(TEXT("Stale reduce queues before replacement exists"), StaleQueue->AddToBack(StaleReduce));

	const FStatusMutationResult RemoveOld = Container->RemoveStatusCommit(Weak20);
	TestTrue(TEXT("Old instance removed before queued reduce executes"), RemoveOld.Outcome == EStatusMutationOutcome::Committed && RemoveOld.bRemoved);
	const FStatusMutationResult CreateReplacement = Container->ApplyStatusCommit(WeakDefinition, 4, 30);
	UStatusInstance* Weak30 = CreateReplacement.EffectiveInstance;
	if (!TestNotNull(TEXT("Replacement Weak#30 instance"), Weak30))
	{
		return false;
	}
	TestTrue(TEXT("Replacement is a different runtime instance"), Weak30 != Weak20);
	TestTrue(TEXT("Replacement sequence is newer, not assumed contiguous"), Weak30->GetRuntimeSequence() > Weak20->GetRuntimeSequence());

	TestTrue(TEXT("Queued stale reduce processes"), StaleQueue->StartProcessing());
	TestFalse(TEXT("Stale reduce queue does not fault"), StaleQueue->IsResolutionFaulted());
	TestFalse(TEXT("Stale reduce queue drains"), StaleQueue->IsBusy());
	TestTrue(TEXT("Replacement remains authoritative member"), Container->ContainsStatusInstance(Weak30));
	TestTrue(TEXT("Old instance remains absent"), !Container->ContainsStatusInstance(Weak20));
	TestEqual(TEXT("Stale reduce cannot reduce replacement amount"), Weak30->GetAmount(), 4);
	TestTrue(TEXT("FindStatusById resolves replacement"), Container->FindStatusById(TEXT("Weak")) == Weak30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D1ExactRemoveActionDoesNotRetargetTest,
	"SlayTheSpireDemo.Phase6UIA2D1.Action.ExactRemoveDoesNotRetarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D1ExactRemoveActionDoesNotRetargetTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusContainer* Container = Fixture.Target->GetStatusContainer();
	UStatusData* WeakDefinition = CreateStatus(Fixture.World, TEXT("Weak"));
	const FStatusMutationResult CreateOld = Container->ApplyStatusCommit(WeakDefinition, 3, 40);
	UStatusInstance* Weak40 = CreateOld.EffectiveInstance;
	if (!TestNotNull(TEXT("Explicit-remove target Weak#40"), Weak40))
	{
		return false;
	}

	UBattleActionQueue* StaleQueue = NewObject<UBattleActionQueue>(Fixture.Battle);
	URemoveStatusAction* StaleRemove = NewObject<URemoveStatusAction>(StaleQueue);
	StaleRemove->Initialize(Fixture.Battle, Fixture.Target, Fixture.Target, Weak40);
	TestTrue(TEXT("Second exact remove is queued while old instance exists"), StaleQueue->AddToBack(StaleRemove));

	UBattleActionQueue* SuccessQueue = NewObject<UBattleActionQueue>(Fixture.Battle);
	URemoveStatusAction* SuccessfulRemove = NewObject<URemoveStatusAction>(SuccessQueue);
	SuccessfulRemove->Initialize(Fixture.Battle, Fixture.Target, Fixture.Target, Weak40);
	TestTrue(TEXT("Explicit remove queues"), SuccessQueue->AddToBack(SuccessfulRemove));
	TestTrue(TEXT("Explicit remove processes"), SuccessQueue->StartProcessing());
	TestFalse(TEXT("Explicit remove queue does not fault"), SuccessQueue->IsResolutionFaulted());
	TestFalse(TEXT("Explicit remove queue drains"), SuccessQueue->IsBusy());
	TestFalse(TEXT("Explicit remove removes exact old instance"), Container->ContainsStatusInstance(Weak40));
	TestEqual(TEXT("Container empty after explicit remove"), Container->GetStatuses().Num(), 0);

	const FStatusMutationResult CreateReplacement = Container->ApplyStatusCommit(WeakDefinition, 2, 50);
	UStatusInstance* Weak50 = CreateReplacement.EffectiveInstance;
	if (!TestNotNull(TEXT("Replacement Weak#50"), Weak50))
	{
		return false;
	}
	TestTrue(TEXT("Replacement has a newer RuntimeSequence"), Weak50->GetRuntimeSequence() > Weak40->GetRuntimeSequence());

	TestTrue(TEXT("Queued stale exact remove processes"), StaleQueue->StartProcessing());
	TestFalse(TEXT("Stale exact remove queue does not fault"), StaleQueue->IsResolutionFaulted());
	TestFalse(TEXT("Stale exact remove queue drains"), StaleQueue->IsBusy());
	TestTrue(TEXT("Stale remove cannot delete replacement"), Container->ContainsStatusInstance(Weak50));
	TestEqual(TEXT("Stale remove cannot mutate replacement amount"), Weak50->GetAmount(), 2);
	TestTrue(TEXT("Status lookup still resolves replacement"), Container->FindStatusById(TEXT("Weak")) == Weak50);
	return true;
}

#endif
