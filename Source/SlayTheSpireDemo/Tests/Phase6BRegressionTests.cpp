#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Actions/BattleActionQueue.h"
#include "../Battle/BattleManager.h"
#include "../Combat/Combatant.h"
#include "../Events/TurnEndStatusDecayTrigger.h"
#include "../Modifiers/Damage/DamageRatioModifier.h"
#include "../Modifiers/ModifierTypes.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusData.h"
#include "../Status/StatusInstance.h"
#include "Engine/World.h"

namespace Phase6BRegression
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

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

			if (IsValid(Player))
			{
				Player->MaxHP = 100;
			}
			if (IsValid(Enemy))
			{
				Enemy->MaxHP = 100;
			}
			if (IsValid(Battle) && IsValid(Player) && IsValid(Enemy))
			{
				Battle->Player = Player;
				Battle->Enemy = Enemy;
				Battle->MaxEnergy = 3;
				Battle->EnemyTestAttackDamage = 5;
				Battle->StartBattle();
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
				&& IsValid(Battle)
				&& IsValid(Battle->GetActionQueueForTesting())
				&& IsValid(Player->GetStatusContainer())
				&& IsValid(Enemy->GetStatusContainer())
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}
	};

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (!Fixture.IsReady())
		{
			Test.AddError(TEXT("Failed to create the transient Phase 6B battle fixture."));
			return false;
		}
		return true;
	}

	void ExpectImmediateResolutionFaultLogs(FAutomationTestBase& Test)
	{
		Test.AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution fault requested:"), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("[Battle] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
	}

	UStatusData* CreateDecayStatus(UObject* Outer, const TCHAR* StatusId)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = FName(StatusId);
		UTurnEndStatusDecayTrigger* Trigger = NewObject<UTurnEndStatusDecayTrigger>(Definition);
		Trigger->Priority = 0;
		Trigger->AmountToRemove = 1;
		Definition->Triggers.Add(Trigger);
		return Definition;
	}

	UStatusInstance* ApplyStatus(ACombatant* Target, UStatusData* Definition, int32 Amount, uint64 RuntimeSequence)
	{
		bool bCreated = false;
		return Target->GetStatusContainer()->ApplyStatus(Definition, Amount, RuntimeSequence, bCreated);
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6BPlayerEndingStateCommitsOnlyAfterEnqueueSuccessTest,
		"SlayTheSpireDemo.Phase6B.Turn.PlayerEndingStateCommitsOnlyAfterEnqueueSuccess",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6BPlayerEndingStateCommitsOnlyAfterEnqueueSuccessTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		AddExpectedErrorPlain(TEXT("[Battle] EndPlayerTurn failed to enqueue the atomic TurnEndedAction batch."), EAutomationExpectedErrorFlags::Contains, 1);
		ExpectImmediateResolutionFaultLogs(*this);

		Fixture.Battle->SetForceInvalidPlayerEndBatchForTesting(true);
		Fixture.Battle->EndPlayerTurn();

		TestEqual(TEXT("Rejected batch faults the battle"), Fixture.Battle->BattleState, EBattleState::ResolutionFaulted);
		TestEqual(
			TEXT("PlayerTurnEnding was never committed before insertion failure"),
			Fixture.Battle->GetStateBeforeLastResolutionFaultForTesting(),
			EBattleState::PlayerTurn
		);
		TestEqual(TEXT("Rejected batch leaves no pending actions"), Fixture.Battle->GetActionQueueForTesting()->GetPendingCount(), 0);
		TestEqual(TEXT("Faulted battle has zero energy"), Fixture.Battle->Energy, 0);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6BEnemyBatchInsertionIsAtomicTest,
		"SlayTheSpireDemo.Phase6B.Turn.EnemyBatchInsertionIsAtomic",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6BEnemyBatchInsertionIsAtomicTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		AddExpectedErrorPlain(TEXT("[Battle] StartEnemyTurn failed to enqueue the atomic enemy action batch."), EAutomationExpectedErrorFlags::Contains, 1);
		ExpectImmediateResolutionFaultLogs(*this);

		Fixture.Battle->SetForceInvalidEnemyTurnBatchForTesting(true);
		Fixture.Battle->EndPlayerTurn();

		TestEqual(TEXT("Invalid enemy batch faults the battle"), Fixture.Battle->BattleState, EBattleState::ResolutionFaulted);
		TestEqual(
			TEXT("EnemyTurn was never committed before insertion failure"),
			Fixture.Battle->GetStateBeforeLastResolutionFaultForTesting(),
			EBattleState::PlayerTurnEnding
		);
		TestEqual(TEXT("Atomic rejection prevents enemy DamageAction from executing"), Fixture.Player->HP, 100);
		TestEqual(TEXT("Atomic rejection leaves no partial pending work"), Fixture.Battle->GetActionQueueForTesting()->GetPendingCount(), 0);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6BLethalEnemyActionSkipsTurnEndedEventTest,
		"SlayTheSpireDemo.Phase6B.Turn.LethalEnemyActionSkipsTurnEndedEvent",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6BLethalEnemyActionSkipsTurnEndedEventTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* EnemyDecay = CreateDecayStatus(Fixture.World, TEXT("EnemyDecay"));
		UStatusInstance* EnemyDecayInstance = ApplyStatus(Fixture.Enemy, EnemyDecay, 2, 1);
		Fixture.Battle->EnemyTestAttackDamage = 100;

		Fixture.Battle->EndPlayerTurn();

		TestEqual(TEXT("Lethal enemy action resolves battle as Defeat"), Fixture.Battle->BattleState, EBattleState::Defeat);
		TestTrue(TEXT("Player is dead"), Fixture.Player->IsDead());
		TestEqual(
			TEXT("Enemy TurnEnded event was skipped, so enemy decay did not run"),
			EnemyDecayInstance ? EnemyDecayInstance->GetAmount() : 0,
			2
		);
		TestFalse(TEXT("Lethal path does not fault resolution"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6BOneFinalQueueEmptyPerTurnBoundaryTest,
		"SlayTheSpireDemo.Phase6B.Turn.OneFinalQueueEmptyPerTurnBoundary",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6BOneFinalQueueEmptyPerTurnBoundaryTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* PlayerDecay = CreateDecayStatus(Fixture.World, TEXT("PlayerDecay"));
		UStatusData* EnemyDecay = CreateDecayStatus(Fixture.World, TEXT("EnemyDecay"));
		UStatusInstance* PlayerInstance = ApplyStatus(Fixture.Player, PlayerDecay, 2, 1);
		UStatusInstance* EnemyInstance = ApplyStatus(Fixture.Enemy, EnemyDecay, 2, 2);

		int32 QueueEmptyCount = 0;
		Fixture.Battle->GetActionQueueForTesting()->OnQueueEmpty.AddLambda([&QueueEmptyCount]() { ++QueueEmptyCount; });

		Fixture.Battle->EndPlayerTurn();

		TestEqual(TEXT("Player ending and enemy ending each produce one final QueueEmpty"), QueueEmptyCount, 2);
		TestEqual(TEXT("Player reaction completed before its turn boundary emptied"), PlayerInstance ? PlayerInstance->GetAmount() : 0, 1);
		TestEqual(TEXT("Enemy reaction completed before its turn boundary emptied"), EnemyInstance ? EnemyInstance->GetAmount() : 0, 1);
		TestEqual(TEXT("Full cycle returns to PlayerTurn"), Fixture.Battle->BattleState, EBattleState::PlayerTurn);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6BResolutionFaultTransitionsBattleStateTest,
		"SlayTheSpireDemo.Phase6B.Turn.ResolutionFaultTransitionsBattleState",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6BResolutionFaultTransitionsBattleStateTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		ExpectImmediateResolutionFaultLogs(*this);
		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		TestTrue(TEXT("Resolution fault request accepted"), Queue->RequestResolutionFault(TEXT("Phase 6B test fault.")));

		TestTrue(TEXT("Queue entered ResolutionFault"), Queue->IsResolutionFaulted());
		TestEqual(TEXT("Battle mirrors queue fault state"), Fixture.Battle->BattleState, EBattleState::ResolutionFaulted);
		TestEqual(TEXT("Fault records the previous battle state"), Fixture.Battle->GetStateBeforeLastResolutionFaultForTesting(), EBattleState::PlayerTurn);
		TestEqual(TEXT("Fault disables energy"), Fixture.Battle->Energy, 0);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6BTurnEndReactionCompletesBeforeNextTurnTest,
		"SlayTheSpireDemo.Phase6B.Turn.TurnEndReactionCompletesBeforeNextTurn",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6BTurnEndReactionCompletesBeforeNextTurnTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* Vulnerable = CreateDecayStatus(Fixture.World, TEXT("Vulnerable"));
		UDamageRatioModifier* VulnerableRatio = NewObject<UDamageRatioModifier>(Vulnerable);
		VulnerableRatio->Scope = EModifierScope::Target;
		VulnerableRatio->Priority = 0;
		VulnerableRatio->ApplicableDamageKind = EDamageKind::Attack;
		VulnerableRatio->Phase = EDamageModifierPhase::TargetMultiplier;
		VulnerableRatio->Numerator = 3;
		VulnerableRatio->Denominator = 2;
		VulnerableRatio->AmountMode = EModifierAmountMode::PresenceOnly;
		Vulnerable->DamageModifiers.Add(VulnerableRatio);

		ApplyStatus(Fixture.Player, Vulnerable, 1, 1);
		Fixture.Battle->EnemyTestAttackDamage = 5;

		Fixture.Battle->EndPlayerTurn();

		TestNull(TEXT("Player Vulnerable expired at player TurnEnded"), Fixture.Player->GetStatusContainer()->FindStatusById(TEXT("Vulnerable")));
		TestEqual(
			TEXT("Enemy attack sees Vulnerable already removed, so Base 5 stays 5 instead of becoming 7"),
			Fixture.Player->HP,
			95
		);
		TestEqual(TEXT("Enemy ending completes and next PlayerTurn starts"), Fixture.Battle->BattleState, EBattleState::PlayerTurn);
		TestFalse(TEXT("Normal turn cycle does not fault resolution"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
		return true;
	}
}

#endif
