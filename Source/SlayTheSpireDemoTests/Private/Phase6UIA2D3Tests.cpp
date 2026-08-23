#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Actions/ApplyStatusAction.h"
#include "Actions/BattleAction.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/ReduceStatusAction.h"
#include "Actions/RemoveStatusAction.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/BattlePresentationRecorder.h"
#include "Presentation/PresentationTypes.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Status/StatusInstance.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2D3Test
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
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

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
			Battle->StartBattle();
			Flush();
			Deliveries.Reset();
		}

		~FFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		bool IsReady() const
		{
			return IsValid(World)
				&& IsValid(Player)
				&& IsValid(Enemy)
				&& IsValid(Battle)
				&& IsValid(Battle->GetActionQueueForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}

		void Flush() const
		{
			if (IsValid(Battle)) Battle->FlushScheduledReadStateReadyForTesting();
		}

		void ResetDeliveries()
		{
			Deliveries.Reset();
		}

		const FPresentationResolutionEnvelope* LastDelivery() const
		{
			return Deliveries.Num() > 0 ? &Deliveries.Last() : nullptr;
		}

		bool FreezeCurrentState()
		{
			if (!IsValid(Battle) || !Battle->BeginSystemPresentationResolutionForTesting())
			{
				return false;
			}
			if (!Battle->SealActivePresentationResolutionForTesting())
			{
				return false;
			}
			Flush();
			return true;
		}

		bool RunSystemBatch(const TArray<UBattleAction*>& Actions)
		{
			if (!IsValid(Battle) || Actions.Num() == 0 || !Battle->BeginSystemPresentationResolutionForTesting())
			{
				return false;
			}

			const FPresentationRecordWriter Writer = Battle->GetActivePresentationRecordWriterForTesting();
			if (!Writer.IsAvailable())
			{
				return false;
			}

			for (UBattleAction* Action : Actions)
			{
				if (!IsValid(Action)) return false;
				Action->SetPresentationRecordWriter(Writer);
			}

			UBattleActionQueue* Queue = Battle->GetActionQueueForTesting();
			if (!IsValid(Queue)
				|| !Queue->AddBatchToBackPreserveOrder(Actions)
				|| !Queue->StartProcessing())
			{
				return false;
			}

			if (UBattlePresentationRecorder* Recorder = Battle->GetPresentationRecorderForTesting())
			{
				if (Recorder->HasActiveResolution() && !Battle->SealActivePresentationResolutionForTesting())
				{
					return false;
				}
			}
			Flush();
			return true;
		}
	};

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (Fixture.IsReady()) return true;
		Test.AddError(TEXT("Failed to create the Phase 6UI-A2D3 fixture."));
		return false;
	}

	UStatusData* CreateStatusDefinition(
		UObject* Outer,
		const TCHAR* StatusId,
		const TCHAR* DisplayName,
		const TCHAR* Description
	)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		if (!IsValid(Definition)) return nullptr;
		Definition->StatusId = FName(StatusId);
		Definition->DisplayName = FString(DisplayName).IsEmpty()
			? FText::GetEmpty()
			: FText::FromString(DisplayName);
		Definition->Description = FText::FromString(Description);
		return Definition;
	}

	UStatusInstance* FindMutableStatus(UStatusContainer* Container, FName StatusId)
	{
		if (!IsValid(Container)) return nullptr;
		for (const TObjectPtr<UStatusInstance>& StatusPtr : Container->GetStatuses())
		{
			UStatusInstance* Status = StatusPtr.Get();
			if (IsValid(Status) && Status->GetStatusId() == StatusId)
			{
				return Status;
			}
		}
		return nullptr;
	}

	const FBattleHUDStatusView* FindStatusView(
		const FBattleHUDCombatantView& Combatant,
		FName StatusId
	)
	{
		return Combatant.Statuses.FindByPredicate(
			[StatusId](const FBattleHUDStatusView& Status)
			{
				return Status.StatusId == StatusId;
			}
		);
	}

	int32 CountStatusRecords(const FPresentationResolutionEnvelope& Envelope)
	{
		int32 Count = 0;
		for (const FPresentationRecord& Record : Envelope.Records)
		{
			if (Record.Type == EBattlePresentationRecordType::StatusChanged) ++Count;
		}
		return Count;
	}

	FPresentationRecord MakeSyntheticStatusRecord(
		const FPresentationResolutionEnvelope& Envelope,
		int64 PresentationSequence,
		FName TargetPresentationId,
		FName StatusId,
		int64 RuntimeSequence,
		int32 AmountBefore,
		int32 AmountAfter,
		EStatusChangeReason Reason,
		bool bCreated,
		bool bRemoved
	)
	{
		FPresentationRecord Record;
		Record.BattleId = Envelope.BattleId;
		Record.ResolutionId = Envelope.ResolutionId;
		Record.PresentationSequence = PresentationSequence;
		Record.Type = EBattlePresentationRecordType::StatusChanged;
		Record.StatusChanged.SourcePresentationId = TargetPresentationId;
		Record.StatusChanged.TargetPresentationId = TargetPresentationId;
		Record.StatusChanged.StatusId = StatusId;
		Record.StatusChanged.RuntimeSequence = RuntimeSequence;
		Record.StatusChanged.AmountBefore = AmountBefore;
		Record.StatusChanged.AmountAfter = AmountAfter;
		Record.StatusChanged.Reason = Reason;
		Record.StatusChanged.bCreated = bCreated;
		Record.StatusChanged.bRemoved = bRemoved;
		Record.StatusChanged.DisplayName = FText::FromName(StatusId);
		Record.StatusChanged.DescriptionBefore = AmountBefore > 0
			? FText::FromString(FString::Printf(TEXT("%s amount %d."), *StatusId.ToString(), AmountBefore))
			: FText::GetEmpty();
		Record.StatusChanged.DescriptionAfter = AmountAfter > 0
			? FText::FromString(FString::Printf(TEXT("%s amount %d."), *StatusId.ToString(), AmountAfter))
			: FText::GetEmpty();
		return Record;
	}
}

using namespace Phase6UIA2D3Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D3SnapshotRuntimeSequenceSortingTest,
	"SlayTheSpireDemo.Phase6UIA2D3.Snapshot.RuntimeSequenceSorting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D3SnapshotRuntimeSequenceSortingTest::RunTest(const FString& Parameters)
{
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* Status30 = CreateStatusDefinition(Fixture.World, TEXT("Status30"), TEXT("Status 30"), TEXT("Amount {Amount}."));
		UStatusData* Status10 = CreateStatusDefinition(Fixture.World, TEXT("Status10"), TEXT("Status 10"), TEXT("Amount {Amount}."));
		UStatusData* Status20 = CreateStatusDefinition(Fixture.World, TEXT("Status20"), TEXT("Status 20"), TEXT("Amount {Amount}."));
		if (!TestNotNull(TEXT("Status30 definition"), Status30)
			|| !TestNotNull(TEXT("Status10 definition"), Status10)
			|| !TestNotNull(TEXT("Status20 definition"), Status20))
		{
			return false;
		}

		UStatusContainer* Container = Fixture.Player->GetStatusContainer();
		TestTrue(TEXT("Sequence 30 status commits"), Container->ApplyStatusCommit(Status30, 1, 30).IsCommitted());
		TestTrue(TEXT("Sequence 10 status commits"), Container->ApplyStatusCommit(Status10, 1, 10).IsCommitted());
		TestTrue(TEXT("Sequence 20 status commits"), Container->ApplyStatusCommit(Status20, 1, 20).IsCommitted());
		Fixture.ResetDeliveries();
		TestTrue(TEXT("Current status state freezes"), Fixture.FreezeCurrentState());

		const FPresentationResolutionEnvelope* Envelope = Fixture.LastDelivery();
		if (!TestNotNull(TEXT("Sorted snapshot envelope"), Envelope)) return false;
		const TArray<FBattleHUDStatusView>& Statuses = Envelope->FinalSnapshot.Player.Statuses;
		TestEqual(TEXT("Three statuses are frozen"), Statuses.Num(), 3);
		if (Statuses.Num() != 3) return false;
		TestEqual(TEXT("First status is RuntimeSequence 10"), Statuses[0].RuntimeSequence, static_cast<int64>(10));
		TestEqual(TEXT("Second status is RuntimeSequence 20"), Statuses[1].RuntimeSequence, static_cast<int64>(20));
		TestEqual(TEXT("Third status is RuntimeSequence 30"), Statuses[2].RuntimeSequence, static_cast<int64>(30));
		TestEqual(TEXT("Sequence sort keeps Status10 identity"), Statuses[0].StatusId, FName(TEXT("Status10")));
		TestEqual(TEXT("Sequence sort keeps Status20 identity"), Statuses[1].StatusId, FName(TEXT("Status20")));
		TestEqual(TEXT("Sequence sort keeps Status30 identity"), Statuses[2].StatusId, FName(TEXT("Status30")));

		FPresentationStateSnapshot Latest;
		TestTrue(TEXT("Latest frozen baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Latest));
		TestEqual(TEXT("Latest baseline preserves sorted status count"), Latest.Player.Statuses.Num(), 3);
		if (Latest.Player.Statuses.Num() == 3)
		{
			TestTrue(
				TEXT("Latest baseline ordering is strictly ascending"),
				Latest.Player.Statuses[0].RuntimeSequence < Latest.Player.Statuses[1].RuntimeSequence
					&& Latest.Player.Statuses[1].RuntimeSequence < Latest.Player.Statuses[2].RuntimeSequence
			);
		}
	}

	{
		FFixture DuplicateFixture;
		if (!RequireReady(*this, DuplicateFixture)) return false;
		UStatusData* First = CreateStatusDefinition(DuplicateFixture.World, TEXT("FirstStatus"), TEXT("First"), TEXT("First {Amount}."));
		UStatusData* Second = CreateStatusDefinition(DuplicateFixture.World, TEXT("SecondStatus"), TEXT("Second"), TEXT("Second {Amount}."));
		if (!TestNotNull(TEXT("Duplicate-id first definition"), First)
			|| !TestNotNull(TEXT("Duplicate-id second definition"), Second)) return false;
		UStatusContainer* Container = DuplicateFixture.Player->GetStatusContainer();
		TestTrue(TEXT("Duplicate-id first runtime status commits"), Container->ApplyStatusCommit(First, 1, 100).IsCommitted());
		TestTrue(TEXT("Duplicate-id second runtime status commits"), Container->ApplyStatusCommit(Second, 1, 200).IsCommitted());
		Second->StatusId = First->StatusId;
		DuplicateFixture.ResetDeliveries();
		TestFalse(TEXT("Freeze rejects duplicate live StatusId identities"), DuplicateFixture.FreezeCurrentState());
		TestFalse(TEXT("Duplicate StatusId freeze failure degrades Presentation only"), DuplicateFixture.Battle->IsPresentationAvailable());
		TestTrue(TEXT("Duplicate StatusId freeze failure does not Gameplay-fault battle"), DuplicateFixture.Battle->BattleState != EBattleState::ResolutionFaulted);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D3StatusLifecycleReducerTest,
	"SlayTheSpireDemo.Phase6UIA2D3.Playback.StatusLifecycleReducer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D3StatusLifecycleReducerTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;

	UStatusData* Anchor = CreateStatusDefinition(Fixture.World, TEXT("Anchor"), TEXT("Anchor"), TEXT("Anchor amount {Amount}."));
	UStatusData* Weak = CreateStatusDefinition(Fixture.World, TEXT("Weak"), TEXT(""), TEXT("Weak amount {Amount}."));
	UStatusData* Vulnerable = CreateStatusDefinition(Fixture.World, TEXT("Vulnerable"), TEXT("Vulnerable"), TEXT("Vulnerable amount {Amount}."));
	if (!TestNotNull(TEXT("Anchor definition"), Anchor)
		|| !TestNotNull(TEXT("Weak definition"), Weak)
		|| !TestNotNull(TEXT("Vulnerable definition"), Vulnerable))
	{
		return false;
	}

	UStatusContainer* Container = Fixture.Player->GetStatusContainer();
	const FStatusMutationResult AnchorResult = Container->ApplyStatusCommit(
		Anchor,
		1,
		Fixture.Battle->AllocateRuntimeSequence()
	);
	TestTrue(TEXT("Anchor baseline commits"), AnchorResult.IsCommitted());
	Fixture.ResetDeliveries();
	TestTrue(TEXT("Anchor baseline freezes"), Fixture.FreezeCurrentState());

	FPresentationStateSnapshot ExpectedBaseline;
	if (!TestTrue(TEXT("Anchor frozen baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(ExpectedBaseline))) return false;

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	TestTrue(TEXT("ViewModel initializes before Controller ownership"), ViewModel->Initialize(Fixture.Battle, false));
	// Simulate a HUD rebuild/caller that hands the Controller a valid but stale
	// ViewModel. Controller bootstrap must repair display state before raising the
	// Resolution watermark, rather than depending on Presenter call order.
	ViewModel->BattleId = -111;
	ViewModel->StateRevision = -222;
	ViewModel->Player.Statuses.Reset();
	UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	Widget->bAcceptAsyncPlayback = true;
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	TestTrue(TEXT("Controller initializes from Anchor baseline"), Controller->Initialize(Fixture.Battle, ViewModel, Widget));
	TestTrue(TEXT("Controller explicitly owns presentation display after bootstrap"), ViewModel->IsPresentationDisplayOwned());
	TestEqual(TEXT("Controller repairs stale ViewModel BattleId before watermarking"), ViewModel->BattleId, ExpectedBaseline.BattleId);
	TestEqual(TEXT("Controller repairs stale ViewModel StateRevision before watermarking"), ViewModel->StateRevision, ExpectedBaseline.StateRevision);
	TestEqual(TEXT("Anchor baseline visible before new history"), ViewModel->Player.Statuses.Num(), 1);

	Fixture.ResetDeliveries();
	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	UApplyStatusAction* CreateWeak = NewObject<UApplyStatusAction>(Queue);
	CreateWeak->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, Weak, 2);
	UApplyStatusAction* CreateVulnerable = NewObject<UApplyStatusAction>(Queue);
	CreateVulnerable->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, Vulnerable, 1);
	TArray<UBattleAction*> CreateActions;
	CreateActions.Add(CreateWeak);
	CreateActions.Add(CreateVulnerable);
	TestTrue(TEXT("Two status creates resolve in one system Resolution"), Fixture.RunSystemBatch(CreateActions));

	const FPresentationResolutionEnvelope* CreateEnvelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Create envelope"), CreateEnvelope)) return false;
	TestEqual(TEXT("Create envelope has two StatusChanged records"), CountStatusRecords(*CreateEnvelope), 2);
	TestTrue(TEXT("Controller waits on first StatusChanged"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Before first callback only Anchor is displayed"), ViewModel->Player.Statuses.Num(), 1);

	Controller->NotifyPresentationFinished(Controller->GetActivePlaybackTokenForTesting());
	TestTrue(TEXT("Controller advances to second StatusChanged"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("First reducer inserts Weak only"), ViewModel->Player.Statuses.Num(), 2);
	const FBattleHUDStatusView* WeakAfterCreate = FindStatusView(ViewModel->Player, TEXT("Weak"));
	if (!TestNotNull(TEXT("Weak exists after first reducer"), WeakAfterCreate)) return false;
	TestEqual(TEXT("Weak create amount"), WeakAfterCreate->Amount, 2);
	TestEqual(TEXT("Empty authored display name falls back to StatusId"), WeakAfterCreate->DisplayName.ToString(), FString(TEXT("Weak")));
	TestEqual(TEXT("Weak create description"), WeakAfterCreate->Description.ToString(), FString(TEXT("Weak amount 2.")));
	TestTrue(
		TEXT("Reducer keeps status RuntimeSequence ordering"),
		ViewModel->Player.Statuses[0].RuntimeSequence < ViewModel->Player.Statuses[1].RuntimeSequence
	);
	const int64 WeakRuntimeSequence = WeakAfterCreate->RuntimeSequence;
	TestTrue(TEXT("Weak RuntimeSequence is positive"), WeakRuntimeSequence > 0);
	TestNull(TEXT("Vulnerable is not reduced before its Record completes"), FindStatusView(ViewModel->Player, TEXT("Vulnerable")));

	Controller->NotifyPresentationFinished(Controller->GetActivePlaybackTokenForTesting());
	TestFalse(TEXT("Create envelope completes after second callback"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Final create snapshot contains Anchor, Weak and Vulnerable"), ViewModel->Player.Statuses.Num(), 3);

	UStatusInstance* WeakInstance = FindMutableStatus(Container, TEXT("Weak"));
	if (!TestNotNull(TEXT("Live Weak instance exists for exact update/remove batch"), WeakInstance)) return false;
	TestEqual(TEXT("Live Weak identity matches displayed RuntimeSequence"), static_cast<int64>(WeakInstance->GetRuntimeSequence()), WeakRuntimeSequence);

	Fixture.ResetDeliveries();
	UApplyStatusAction* IncreaseWeak = NewObject<UApplyStatusAction>(Queue);
	IncreaseWeak->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, Weak, 2);
	UReduceStatusAction* ReduceWeak = NewObject<UReduceStatusAction>(Queue);
	ReduceWeak->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, WeakInstance, 1, EStatusChangeReason::Reduced);
	URemoveStatusAction* RemoveWeak = NewObject<URemoveStatusAction>(Queue);
	RemoveWeak->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, WeakInstance);
	TArray<UBattleAction*> MutationActions;
	MutationActions.Add(IncreaseWeak);
	MutationActions.Add(ReduceWeak);
	MutationActions.Add(RemoveWeak);
	TestTrue(TEXT("Increase, reduce and remove resolve in one system Resolution"), Fixture.RunSystemBatch(MutationActions));

	const FPresentationResolutionEnvelope* MutationEnvelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Mutation envelope"), MutationEnvelope)) return false;
	TestEqual(TEXT("Mutation envelope has three StatusChanged records"), CountStatusRecords(*MutationEnvelope), 3);
	TestTrue(TEXT("Controller waits on Increased"), Controller->IsWaitingForCompletionForTesting());
	const FBattleHUDStatusView* WeakBeforeIncrease = FindStatusView(ViewModel->Player, TEXT("Weak"));
	if (!TestNotNull(TEXT("Weak is still visible before Increased callback"), WeakBeforeIncrease)) return false;
	TestEqual(TEXT("Displayed Weak remains baseline amount before callback"), WeakBeforeIncrease->Amount, 2);

	Controller->NotifyPresentationFinished(Controller->GetActivePlaybackTokenForTesting());
	const FBattleHUDStatusView* WeakAfterIncrease = FindStatusView(ViewModel->Player, TEXT("Weak"));
	if (!TestNotNull(TEXT("Weak remains after increase"), WeakAfterIncrease)) return false;
	TestEqual(TEXT("Increase reducer applies AmountAfter"), WeakAfterIncrease->Amount, 4);
	TestEqual(TEXT("Increase reducer applies DescriptionAfter"), WeakAfterIncrease->Description.ToString(), FString(TEXT("Weak amount 4.")));
	TestEqual(TEXT("Increase keeps exact RuntimeSequence"), WeakAfterIncrease->RuntimeSequence, WeakRuntimeSequence);
	TestTrue(TEXT("Controller advances to Reduced"), Controller->IsWaitingForCompletionForTesting());

	Controller->NotifyPresentationFinished(Controller->GetActivePlaybackTokenForTesting());
	const FBattleHUDStatusView* WeakAfterReduce = FindStatusView(ViewModel->Player, TEXT("Weak"));
	if (!TestNotNull(TEXT("Weak remains after partial reduction"), WeakAfterReduce)) return false;
	TestEqual(TEXT("Reduce reducer applies AmountAfter"), WeakAfterReduce->Amount, 3);
	TestEqual(TEXT("Reduce reducer applies DescriptionAfter"), WeakAfterReduce->Description.ToString(), FString(TEXT("Weak amount 3.")));
	TestEqual(TEXT("Reduce keeps exact RuntimeSequence"), WeakAfterReduce->RuntimeSequence, WeakRuntimeSequence);
	TestTrue(TEXT("Controller advances to Removed"), Controller->IsWaitingForCompletionForTesting());

	Controller->NotifyPresentationFinished(Controller->GetActivePlaybackTokenForTesting());
	TestFalse(TEXT("Mutation envelope completes after remove"), Controller->IsWaitingForCompletionForTesting());
	TestNull(TEXT("Remove reducer deletes exact Weak identity"), FindStatusView(ViewModel->Player, TEXT("Weak")));
	TestNotNull(TEXT("Anchor remains after Weak removal"), FindStatusView(ViewModel->Player, TEXT("Anchor")));
	TestNotNull(TEXT("Vulnerable remains after Weak removal"), FindStatusView(ViewModel->Player, TEXT("Vulnerable")));
	TestFalse(TEXT("Status reducer does not fault Gameplay"), Queue->IsResolutionFaulted());

	Controller->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D3StaleRuntimeSequenceCollapseTest,
	"SlayTheSpireDemo.Phase6UIA2D3.Safety.StaleRuntimeSequenceCollapses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D3StaleRuntimeSequenceCollapseTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;

	UStatusData* Weak = CreateStatusDefinition(Fixture.World, TEXT("Weak"), TEXT("Weak"), TEXT("Weak amount {Amount}."));
	if (!TestNotNull(TEXT("Weak definition"), Weak)) return false;
	const FStatusMutationResult Replacement = Fixture.Player->GetStatusContainer()->ApplyStatusCommit(Weak, 2, 50);
	TestTrue(TEXT("Replacement Weak#50 commits"), Replacement.IsCommitted());
	Fixture.ResetDeliveries();
	TestTrue(TEXT("Replacement baseline freezes"), Fixture.FreezeCurrentState());

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	TestTrue(TEXT("Stale-test ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true));
	UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	Widget->bAcceptAsyncPlayback = true;
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	TestTrue(TEXT("Stale-test Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, Widget));

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Replacement frozen baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
	const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
	FPresentationResolutionEnvelope Envelope;
	Envelope.BattleId = Baseline.BattleId;
	Envelope.ResolutionId = ResolutionId;
	Envelope.Origin = EPresentationResolutionOrigin::System;
	Envelope.FinalStateRevision = Baseline.StateRevision;
	Envelope.FinalSnapshot = Baseline;
	Envelope.Records.Add(MakeSyntheticStatusRecord(
		Envelope,
		1,
		Baseline.Player.PresentationId,
		TEXT("Weak"),
		10,
		1,
		0,
		EStatusChangeReason::Removed,
		false,
		true
	));

	Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
	TestEqual(TEXT("Stale record is rejected before Blueprint playback"), Widget->PlayCallCount, 0);
	TestFalse(TEXT("Stale reducer mismatch collapses before waiting"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Synthetic stale envelope is marked completed"), Controller->GetLastCompletedResolutionIdForTesting(), ResolutionId);
	const FBattleHUDStatusView* DisplayedWeak = FindStatusView(ViewModel->Player, TEXT("Weak"));
	if (!TestNotNull(TEXT("Replacement Weak survives stale remove"), DisplayedWeak)) return false;
	TestEqual(TEXT("Replacement RuntimeSequence remains #50"), DisplayedWeak->RuntimeSequence, static_cast<int64>(50));
	TestEqual(TEXT("Replacement amount remains authoritative"), DisplayedWeak->Amount, 2);
	UStatusInstance* LiveWeak = FindMutableStatus(Fixture.Player->GetStatusContainer(), TEXT("Weak"));
	if (!TestNotNull(TEXT("Live replacement Weak remains"), LiveWeak)) return false;
	TestEqual(TEXT("Gameplay replacement amount is untouched"), LiveWeak->GetAmount(), 2);
	TestFalse(TEXT("Stale Presentation history does not fault Gameplay"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());

	Controller->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D3AmountMismatchCollapseTest,
	"SlayTheSpireDemo.Phase6UIA2D3.Safety.AmountMismatchCollapses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D3AmountMismatchCollapseTest::RunTest(const FString& Parameters)
{
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;

		UStatusData* Weak = CreateStatusDefinition(Fixture.World, TEXT("Weak"), TEXT("Weak"), TEXT("Weak amount {Amount}."));
		if (!TestNotNull(TEXT("Weak definition"), Weak)) return false;
		const FStatusMutationResult Initial = Fixture.Player->GetStatusContainer()->ApplyStatusCommit(Weak, 2, 60);
		TestTrue(TEXT("Weak#60 baseline commits"), Initial.IsCommitted());
		Fixture.ResetDeliveries();
		TestTrue(TEXT("Weak#60 baseline freezes"), Fixture.FreezeCurrentState());

		UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
		TestTrue(TEXT("Amount-mismatch ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true));
		UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
		Widget->bAcceptAsyncPlayback = true;
		UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
		TestTrue(TEXT("Amount-mismatch Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, Widget));

		FPresentationStateSnapshot Baseline;
		if (!TestTrue(TEXT("Amount-mismatch frozen baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
		const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Baseline.BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Baseline.StateRevision;
		Envelope.FinalSnapshot = Baseline;
		if (Envelope.FinalSnapshot.Player.Statuses.Num() != 1)
		{
			AddError(TEXT("Amount-mismatch fixture expected one frozen status."));
			return false;
		}
		Envelope.FinalSnapshot.Player.Statuses[0].Amount = 3;
		Envelope.FinalSnapshot.Player.Statuses[0].Description = FText::FromString(TEXT("Weak amount 3."));
		Envelope.Records.Add(MakeSyntheticStatusRecord(
			Envelope,
			1,
			Baseline.Player.PresentationId,
			TEXT("Weak"),
			60,
			999,
			1000,
			EStatusChangeReason::Increased,
			false,
			false
		));

		Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
		TestEqual(TEXT("Amount mismatch is rejected before Blueprint playback"), Widget->PlayCallCount, 0);
		TestFalse(TEXT("Amount mismatch collapses without waiting"), Controller->IsWaitingForCompletionForTesting());
		const FBattleHUDStatusView* DisplayedWeak = FindStatusView(ViewModel->Player, TEXT("Weak"));
		if (!TestNotNull(TEXT("FinalSnapshot keeps Weak visible after collapse"), DisplayedWeak)) return false;
		TestEqual(TEXT("Collapse applies authoritative FinalSnapshot amount"), DisplayedWeak->Amount, 3);
		TestEqual(TEXT("Collapse applies authoritative FinalSnapshot description"), DisplayedWeak->Description.ToString(), FString(TEXT("Weak amount 3.")));
		UStatusInstance* LiveWeak = FindMutableStatus(Fixture.Player->GetStatusContainer(), TEXT("Weak"));
		if (!TestNotNull(TEXT("Live Weak remains after Presentation collapse"), LiveWeak)) return false;
		TestEqual(TEXT("Presentation collapse never mutates Gameplay amount"), LiveWeak->GetAmount(), 2);
		TestFalse(TEXT("Amount mismatch does not manufacture Gameplay fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
		Controller->Shutdown();
	}

	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		Fixture.ResetDeliveries();
		TestTrue(TEXT("Fake-source baseline freezes"), Fixture.FreezeCurrentState());
		UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
		TestTrue(TEXT("Fake-source ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true));
		UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
		Widget->bAcceptAsyncPlayback = true;
		UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
		TestTrue(TEXT("Fake-source Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, Widget));
		FPresentationStateSnapshot Baseline;
		if (!TestTrue(TEXT("Fake-source baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Baseline.BattleId;
		Envelope.ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Baseline.StateRevision;
		Envelope.FinalSnapshot = Baseline;
		FPresentationRecord Record = MakeSyntheticStatusRecord(
			Envelope, 1, Baseline.Player.PresentationId, TEXT("Synthetic"), 101, 0, 1,
			EStatusChangeReason::Applied, true, false);
		Record.StatusChanged.SourcePresentationId = TEXT("ImpossibleSource");
		Envelope.Records.Add(Record);
		Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
		TestEqual(TEXT("Impossible SourcePresentationId never reaches Blueprint"), Widget->PlayCallCount, 0);
		TestFalse(TEXT("Impossible SourcePresentationId collapses immediately"), Controller->IsWaitingForCompletionForTesting());
		Controller->Shutdown();
	}

	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		Fixture.ResetDeliveries();
		TestTrue(TEXT("Malformed-create baseline freezes"), Fixture.FreezeCurrentState());
		UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
		TestTrue(TEXT("Malformed-create ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true));
		UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
		Widget->bAcceptAsyncPlayback = true;
		UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
		TestTrue(TEXT("Malformed-create Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, Widget));
		FPresentationStateSnapshot Baseline;
		if (!TestTrue(TEXT("Malformed-create baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Baseline.BattleId;
		Envelope.ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Baseline.StateRevision;
		Envelope.FinalSnapshot = Baseline;
		FBattleHUDStatusView FinalStatus;
		FinalStatus.StatusId = TEXT("MalformedCreate");
		FinalStatus.RuntimeSequence = 111;
		FinalStatus.Amount = 1;
		FinalStatus.DisplayName = FText::FromString(TEXT("Malformed Create"));
		FinalStatus.Description = FText::FromString(TEXT("MalformedCreate amount 1."));
		Envelope.FinalSnapshot.Player.Statuses.Add(FinalStatus);
		FPresentationRecord Record = MakeSyntheticStatusRecord(
			Envelope, 1, Baseline.Player.PresentationId, TEXT("MalformedCreate"), 111, 0, 1,
			EStatusChangeReason::Applied, true, false);
		Record.StatusChanged.DescriptionBefore = FText::FromString(TEXT("Illegal create-before text"));
		Envelope.Records.Add(Record);
		Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
		TestEqual(TEXT("Create with non-empty DescriptionBefore never reaches Blueprint"), Widget->PlayCallCount, 0);
		TestFalse(TEXT("Malformed create description collapses immediately"), Controller->IsWaitingForCompletionForTesting());
		TestNotNull(TEXT("Collapse still applies authoritative created FinalSnapshot"), FindStatusView(ViewModel->Player, TEXT("MalformedCreate")));
		Controller->Shutdown();
	}

	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		UStatusData* Weak = CreateStatusDefinition(Fixture.World, TEXT("MalformedRemove"), TEXT("Malformed Remove"), TEXT("MalformedRemove amount {Amount}."));
		if (!TestNotNull(TEXT("Malformed-remove definition"), Weak)) return false;
		TestTrue(TEXT("Malformed-remove baseline status commits"), Fixture.Player->GetStatusContainer()->ApplyStatusCommit(Weak, 1, 121).IsCommitted());
		Fixture.ResetDeliveries();
		TestTrue(TEXT("Malformed-remove baseline freezes"), Fixture.FreezeCurrentState());
		UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
		TestTrue(TEXT("Malformed-remove ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true));
		UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
		Widget->bAcceptAsyncPlayback = true;
		UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
		TestTrue(TEXT("Malformed-remove Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, Widget));
		FPresentationStateSnapshot Baseline;
		if (!TestTrue(TEXT("Malformed-remove baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Baseline.BattleId;
		Envelope.ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Baseline.StateRevision;
		Envelope.FinalSnapshot = Baseline;
		Envelope.FinalSnapshot.Player.Statuses.Reset();
		FPresentationRecord Record = MakeSyntheticStatusRecord(
			Envelope, 1, Baseline.Player.PresentationId, TEXT("MalformedRemove"), 121, 1, 0,
			EStatusChangeReason::Removed, false, true);
		Record.StatusChanged.DescriptionAfter = FText::FromString(TEXT("Illegal remove-after text"));
		Envelope.Records.Add(Record);
		Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
		TestEqual(TEXT("Remove with non-empty DescriptionAfter never reaches Blueprint"), Widget->PlayCallCount, 0);
		TestFalse(TEXT("Malformed remove description collapses immediately"), Controller->IsWaitingForCompletionForTesting());
		TestNull(TEXT("Collapse still applies authoritative removed FinalSnapshot"), FindStatusView(ViewModel->Player, TEXT("MalformedRemove")));
		Controller->Shutdown();
	}
	return true;
}

#endif
