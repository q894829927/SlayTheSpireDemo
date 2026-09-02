#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase7BTriggerSourceTestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Events/BattleEvent.h"
#include "Events/BattleEventDispatcher.h"
#include "Relics/RelicContainer.h"
#include "Relics/RelicData.h"
#include "Relics/RelicInstance.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Status/StatusInstance.h"
#include "Engine/World.h"

namespace Phase7BTriggerSources
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ABattleManager* Battle = nullptr;
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

			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(),
				FTransform(FVector(100.0, 0.0, 0.0)),
				SpawnParameters
			);

			if (IsValid(Battle) && IsValid(Player) && IsValid(Enemy))
			{
				Player->MaxHP = 100;
				Enemy->MaxHP = 100;
				Battle->Player = Player;
				Battle->Enemy = Enemy;
				Battle->OpeningHandDrawCount = 0;
				Battle->PlayerTurnDrawCount = 0;
			}
		}

		~FFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		bool Start(const TArray<URelicData*>& StartingRelics)
		{
			if (!IsValid(Battle) || !IsValid(Player) || !IsValid(Enemy))
			{
				return false;
			}

			Battle->DebugStartingRelics.Reset();
			for (URelicData* Relic : StartingRelics)
			{
				Battle->DebugStartingRelics.Add(Relic);
			}
			Battle->StartBattle();

			return IsValid(Battle->GetPlayerRelicContainer())
				&& IsValid(Player->GetStatusContainer())
				&& IsValid(Enemy->GetStatusContainer());
		}
	};

	bool RequireStarted(
		FAutomationTestBase& Test,
		FFixture& Fixture,
		const TArray<URelicData*>& StartingRelics
	)
	{
		if (!Fixture.Start(StartingRelics))
		{
			Test.AddError(TEXT("Failed to start the transient Phase 7B battle fixture."));
			return false;
		}
		return true;
	}

	URelicData* CreateRelic(UObject* Outer, const TCHAR* RelicId)
	{
		URelicData* Definition = NewObject<URelicData>(Outer);
		Definition->RelicId = FName(RelicId);
		return Definition;
	}

	UStatusData* CreateStatus(UObject* Outer, const TCHAR* StatusId)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = FName(StatusId);
		return Definition;
	}

	UPhase7BTestSourceTrigger* AddSourceTrigger(
		UObject* DefinitionOuter,
		TArray<TObjectPtr<UBattleTrigger>>& TriggerArray,
		UPhase7BTestExecutionRecorder* Recorder,
		int32 Value,
		ETriggerRuntimeSourceKind SourceKind,
		FName SourceId,
		int32 Priority
	)
	{
		UPhase7BTestSourceTrigger* Trigger = NewObject<UPhase7BTestSourceTrigger>(DefinitionOuter);
		Trigger->Priority = Priority;
		Trigger->Initialize(Recorder, Value, SourceKind, SourceId);
		TriggerArray.Add(Trigger);
		return Trigger;
	}

	UStatusInstance* ApplyStatus(
		FFixture& Fixture,
		UStatusData* Definition
	)
	{
		if (!IsValid(Definition) || !IsValid(Fixture.Player) || !IsValid(Fixture.Battle))
		{
			return nullptr;
		}

		const FStatusMutationResult Result = Fixture.Player->GetStatusContainer()->ApplyStatusCommit(
			Definition,
			1,
			Fixture.Battle->AllocateRuntimeSequence()
		);
		return Result.EffectiveInstance;
	}

	UBattleEventDispatcher* CreateBoundDispatcher(FFixture& Fixture)
	{
		UBattleEventDispatcher* Dispatcher = NewObject<UBattleEventDispatcher>(Fixture.World);
		return IsValid(Dispatcher) && Dispatcher->BindBattleContext(Fixture.Battle)
			? Dispatcher
			: nullptr;
	}

	TArray<ACombatant*> BothCombatants(const FFixture& Fixture)
	{
		return TArray<ACombatant*>{Fixture.Player, Fixture.Enemy};
	}

	bool ExpectValues(
		FAutomationTestBase& Test,
		const UPhase7BTestExecutionRecorder* Recorder,
		std::initializer_list<int32> ExpectedValues
	)
	{
		if (!IsValid(Recorder))
		{
			Test.AddError(TEXT("Execution recorder is invalid."));
			return false;
		}

		const TArray<int32>& Actual = Recorder->GetValues();
		Test.TestEqual(TEXT("Execution count"), Actual.Num(), static_cast<int32>(ExpectedValues.size()));

		int32 Index = 0;
		for (int32 Expected : ExpectedValues)
		{
			if (!Actual.IsValidIndex(Index))
			{
				return false;
			}
			Test.TestEqual(FString::Printf(TEXT("Execution[%d]"), Index), Actual[Index], Expected);
			++Index;
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7BContextCompatibilityTest,
		"SlayTheSpireDemo.Phase7.TriggerSources.ContextCompatibility",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase7BContextCompatibilityTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		URelicData* RelicDefinition = CreateRelic(Fixture.World, TEXT("ContextRelic"));
		if (!RequireStarted(*this, Fixture, {RelicDefinition})) return false;

		const URelicContainer* RelicContainer = Fixture.Battle->GetPlayerRelicContainer();
		if (!TestNotNull(TEXT("Relic container"), RelicContainer)) return false;
		if (!TestEqual(TEXT("One relic runtime"), RelicContainer->GetRelics().Num(), 1)) return false;
		URelicInstance* Relic = RelicContainer->GetRelics()[0].Get();
		if (!TestNotNull(TEXT("Relic runtime"), Relic)) return false;

		UStatusData* StatusDefinition = CreateStatus(Fixture.World, TEXT("ContextStatus"));
		UStatusInstance* Status = ApplyStatus(Fixture, StatusDefinition);
		if (!TestNotNull(TEXT("Status runtime"), Status)) return false;

		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		if (!TestNotNull(TEXT("Action outer"), Queue)) return false;

		const FTriggerContext LegacyStatusContext(Status, Queue);
		TestTrue(TEXT("Legacy Status accessor preserved"), LegacyStatusContext.GetRuntimeSource() == Status);
		TestTrue(TEXT("Legacy generic object is Status"), LegacyStatusContext.GetRuntimeSourceObject() == Status);
		TestTrue(TEXT("Legacy Status has no Relic accessor"), LegacyStatusContext.GetRelicSource() == nullptr);
		TestEqual(TEXT("Legacy Status source id"), LegacyStatusContext.GetSourceId(), StatusDefinition->StatusId);
		TestEqual(TEXT("Legacy Status RuntimeSequence"), LegacyStatusContext.GetRuntimeSequence(), Status->GetRuntimeSequence());
		TestTrue(TEXT("Legacy Status owner preserved"), LegacyStatusContext.GetOwner() == Fixture.Player);
		TestTrue(TEXT("Legacy constructor has no implicit Battle lookup"), LegacyStatusContext.GetBattle() == nullptr);

		const FTriggerContext StatusContext(Status, Queue, Fixture.Battle);
		TestTrue(TEXT("Status source kind"), StatusContext.GetSourceKind() == ETriggerRuntimeSourceKind::Status);
		TestTrue(TEXT("Status Battle context"), StatusContext.GetBattle() == Fixture.Battle);

		const FTriggerContext RelicContext(
			FTriggerRuntimeSource::FromRelic(Relic),
			Queue,
			Fixture.Battle
		);
		TestTrue(TEXT("Relic generic object"), RelicContext.GetRuntimeSourceObject() == Relic);
		TestTrue(TEXT("Relic does not masquerade as Status"), RelicContext.GetRuntimeSource() == nullptr);
		TestTrue(TEXT("Relic accessor"), RelicContext.GetRelicSource() == Relic);
		TestTrue(TEXT("Relic source kind"), RelicContext.GetSourceKind() == ETriggerRuntimeSourceKind::Relic);
		TestEqual(TEXT("Relic source id"), RelicContext.GetSourceId(), RelicDefinition->RelicId);
		TestEqual(TEXT("Relic RuntimeSequence"), RelicContext.GetRuntimeSequence(), Relic->GetRuntimeSequence());
		TestTrue(TEXT("Battle-owned Relic has no Combatant owner"), RelicContext.GetOwner() == nullptr);
		TestTrue(TEXT("Relic Battle context"), RelicContext.GetBattle() == Fixture.Battle);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7BRelicReactionParticipationTest,
		"SlayTheSpireDemo.Phase7.TriggerSources.RelicReactionParticipation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase7BRelicReactionParticipationTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		UPhase7BTestExecutionRecorder* Recorder = NewObject<UPhase7BTestExecutionRecorder>(Fixture.World);
		URelicData* Relic = CreateRelic(Fixture.World, TEXT("ReactionRelic"));
		AddSourceTrigger(
			Relic,
			Relic->Triggers,
			Recorder,
			7,
			ETriggerRuntimeSourceKind::Relic,
			Relic->RelicId,
			0
		);
		if (!RequireStarted(*this, Fixture, {Relic})) return false;

		UBattleEventDispatcher* Dispatcher = CreateBoundDispatcher(Fixture);
		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		if (!TestNotNull(TEXT("Bound dispatcher"), Dispatcher) || !TestNotNull(TEXT("Queue"), Queue)) return false;

		TArray<FTriggerEligibilityRecord> Trace;
		TestTrue(TEXT("Relic dispatch succeeds"), Dispatcher->Dispatch(
			FBattleEvent::MakeTurnEnded(Fixture.Player),
			Queue,
			BothCombatants(Fixture),
			&Trace
		));
		TestTrue(TEXT("Relic reaction queue starts"), Queue->StartProcessing());
		ExpectValues(*this, Recorder, {7});

		if (!TestEqual(TEXT("One eligible Relic trigger"), Trace.Num(), 1)) return false;
		TestTrue(TEXT("Eligibility source kind is Relic"), Trace[0].SourceKind == ETriggerRuntimeSourceKind::Relic);
		TestEqual(TEXT("Eligibility SourceId"), Trace[0].SourceId, Relic->RelicId);
		TestTrue(TEXT("Legacy StatusId remains empty for Relic"), Trace[0].StatusId.IsNone());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7BCombinedOrderingAndTraceTest,
		"SlayTheSpireDemo.Phase7.TriggerSources.CombinedOrderingAndTrace",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase7BCombinedOrderingAndTraceTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture;
		UPhase7BTestExecutionRecorder* Recorder = NewObject<UPhase7BTestExecutionRecorder>(Fixture.World);

		URelicData* RelicA = CreateRelic(Fixture.World, TEXT("RelicA"));
		AddSourceTrigger(RelicA, RelicA->Triggers, Recorder, 10, ETriggerRuntimeSourceKind::Relic, RelicA->RelicId, 5);
		AddSourceTrigger(RelicA, RelicA->Triggers, Recorder, 11, ETriggerRuntimeSourceKind::Relic, RelicA->RelicId, 5);

		URelicData* RelicB = CreateRelic(Fixture.World, TEXT("RelicB"));
		AddSourceTrigger(RelicB, RelicB->Triggers, Recorder, 20, ETriggerRuntimeSourceKind::Relic, RelicB->RelicId, 0);

		if (!RequireStarted(*this, Fixture, {RelicA, RelicB})) return false;

		UStatusData* Status = CreateStatus(Fixture.World, TEXT("StatusC"));
		AddSourceTrigger(Status, Status->Triggers, Recorder, 30, ETriggerRuntimeSourceKind::Status, Status->StatusId, 5);
		UStatusInstance* StatusInstance = ApplyStatus(Fixture, Status);
		if (!TestNotNull(TEXT("Status runtime"), StatusInstance)) return false;

		const URelicContainer* RelicContainer = Fixture.Battle->GetPlayerRelicContainer();
		if (!TestNotNull(TEXT("Relic container"), RelicContainer)) return false;
		if (!TestEqual(TEXT("Two relic runtimes"), RelicContainer->GetRelics().Num(), 2)) return false;
		const URelicInstance* RelicAInstance = RelicContainer->GetRelics()[0].Get();
		const URelicInstance* RelicBInstance = RelicContainer->GetRelics()[1].Get();
		if (!TestNotNull(TEXT("Relic A runtime"), RelicAInstance) || !TestNotNull(TEXT("Relic B runtime"), RelicBInstance)) return false;

		TestTrue(TEXT("Starting Relic A precedes Relic B"), RelicAInstance->GetRuntimeSequence() < RelicBInstance->GetRuntimeSequence());
		TestTrue(TEXT("Later Status follows Starting Relics"), RelicBInstance->GetRuntimeSequence() < StatusInstance->GetRuntimeSequence());

		UBattleEventDispatcher* Dispatcher = CreateBoundDispatcher(Fixture);
		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		if (!TestNotNull(TEXT("Bound dispatcher"), Dispatcher) || !TestNotNull(TEXT("Queue"), Queue)) return false;

		TArray<FTriggerEligibilityRecord> Trace;
		TestTrue(TEXT("Combined dispatch succeeds"), Dispatcher->Dispatch(
			FBattleEvent::MakeTurnEnded(Fixture.Player),
			Queue,
			BothCombatants(Fixture),
			&Trace
		));
		TestTrue(TEXT("Combined reaction queue starts"), Queue->StartProcessing());

		// Priority wins first. With Priority=5 tied, battle-wide RuntimeSequence
		// orders RelicA before the later Status, and LocalTriggerIndex orders the
		// two RelicA triggers.
		ExpectValues(*this, Recorder, {20, 10, 11, 30});

		if (!TestEqual(TEXT("Four eligible triggers"), Trace.Num(), 4)) return false;

		TestTrue(TEXT("Trace[0] RelicB"),
			Trace[0].SourceKind == ETriggerRuntimeSourceKind::Relic
			&& Trace[0].SourceId == RelicB->RelicId
			&& Trace[0].Priority == 0);

		TestTrue(TEXT("Trace[1] RelicA local 0"),
			Trace[1].SourceKind == ETriggerRuntimeSourceKind::Relic
			&& Trace[1].SourceId == RelicA->RelicId
			&& Trace[1].LocalTriggerIndex == 0);
		TestTrue(TEXT("Trace[2] RelicA local 1"),
			Trace[2].SourceKind == ETriggerRuntimeSourceKind::Relic
			&& Trace[2].SourceId == RelicA->RelicId
			&& Trace[2].LocalTriggerIndex == 1);

		TestTrue(TEXT("Trace[3] StatusC"),
			Trace[3].SourceKind == ETriggerRuntimeSourceKind::Status
			&& Trace[3].SourceId == Status->StatusId
			&& Trace[3].StatusId == Status->StatusId);
		TestEqual(TEXT("Status trace RuntimeSequence"), Trace[3].RuntimeSequence, StatusInstance->GetRuntimeSequence());
		return true;
	}
}

#endif
