#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SelectionPresentationG3TestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG3Tests
{
	FPresentationPlaybackToken MakeRecordToken(
		int64 BattleId,
		int64 ResolutionId,
		int64 Sequence,
		int64 Generation
	)
	{
		FPresentationPlaybackToken Token;
		Token.BattleId = BattleId;
		Token.ResolutionId = ResolutionId;
		Token.PresentationSequence = Sequence;
		Token.LocalPlaybackGeneration = Generation;
		Token.UnitKind = EPresentationPlaybackUnitKind::SingleRecord;
		Token.GroupId = 0;
		return Token;
	}

	FPresentationPlaybackToken MakeGroupToken(
		int64 BattleId,
		int64 ResolutionId,
		int64 Sequence,
		int64 Generation,
		int64 GroupId
	)
	{
		FPresentationPlaybackToken Token = MakeRecordToken(
			BattleId,
			ResolutionId,
			Sequence,
			Generation);
		Token.UnitKind = EPresentationPlaybackUnitKind::Group;
		Token.GroupId = GroupId;
		return Token;
	}

	FPresentationGroupTag MakeGroup(int64 GroupId, int32 Count)
	{
		FPresentationGroupTag Group;
		Group.Kind = EPresentationGroupKind::SelectionDestination;
		Group.GroupId = GroupId;
		Group.ExpectedMemberCount = Count;
		return Group;
	}

	FPresentationRecord MakeTaggedEnergyRecord(
		int64 BattleId,
		int64 ResolutionId,
		int64 Sequence,
		int32 EnergyBefore,
		int32 EnergyAfter,
		const FPresentationGroupTag* Group = nullptr
	)
	{
		FPresentationRecord Record;
		Record.BattleId = BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::EnergyChanged;
		Record.EnergyChanged.EnergyBefore = EnergyBefore;
		Record.EnergyChanged.EnergyAfter = EnergyAfter;
		Record.EnergyChanged.Delta = EnergyAfter - EnergyBefore;
		if (Group != nullptr)
		{
			Record.Group = *Group;
		}
		return Record;
	}

	FBattleHUDCardView MakeDisplayedCard(int32 RuntimeId)
	{
		FBattleHUDCardView Card;
		Card.RuntimeId = RuntimeId;
		Card.CardId = FName(*FString::Printf(TEXT("G3Card%d"), RuntimeId));
		Card.DisplayName = FText::FromName(Card.CardId);
		return Card;
	}

	struct FControllerFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		USelectionPresentationG3PlaybackWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;
		FPresentationStateSnapshot Baseline;
		int64 BaselineResolutionId = 0;

		FControllerFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(),
				FTransform::Identity,
				SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(),
				FTransform(FVector(100.0, 0.0, 0.0)),
				SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(
				ABattleManager::StaticClass(),
				FTransform::Identity,
				SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

			Player->MaxHP = 80;
			Enemy->MaxHP = 50;
			Player->PresentationId = TEXT("PlayerStable");
			Enemy->PresentationId = TEXT("EnemyStable");
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->bEnableCommittedPresentationRecording = true;
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();

			if (!Battle->TryGetLatestFrozenPresentationBaseline(Baseline)) return;
			BaselineResolutionId = static_cast<int64>(
				Battle->GetLatestFrozenPresentationBaselineResolutionId());

			ViewModel = NewObject<UBattleHUDViewModel>(World);
			Widget = NewObject<USelectionPresentationG3PlaybackWidget>(World);
			Controller = NewObject<UBattlePresentationController>(World);
			if (!IsValid(ViewModel) || !IsValid(Widget) || !IsValid(Controller)) return;

			if (!ViewModel->Initialize(Battle, false)) return;
			Widget->SetViewModel(ViewModel);
			if (!Controller->Initialize(Battle, ViewModel, Widget)) return;
			Widget->SetPresentationController(Controller);
		}

		~FControllerFixture()
		{
			if (IsValid(Controller)) Controller->Shutdown();
			if (IsValid(ViewModel)) ViewModel->Shutdown();
			if (IsValid(World)) World->DestroyWorld(false);
		}

		bool IsReady() const
		{
			return IsValid(World)
				&& IsValid(Player)
				&& IsValid(Enemy)
				&& IsValid(Battle)
				&& IsValid(ViewModel)
				&& IsValid(Widget)
				&& IsValid(Controller)
				&& Baseline.BattleId > 0;
		}

		FPresentationResolutionEnvelope MakeEnergyEnvelope(
			int64 ResolutionId,
			const FPresentationStateSnapshot& Before,
			int32 EnergyAfter
		) const
		{
			FPresentationResolutionEnvelope Envelope;
			Envelope.BattleId = Before.BattleId;
			Envelope.ResolutionId = ResolutionId;
			Envelope.Origin = EPresentationResolutionOrigin::System;
			Envelope.FinalStateRevision = Before.StateRevision;
			Envelope.FinalSnapshot = Before;
			Envelope.FinalSnapshot.Energy = EnergyAfter;
			Envelope.Records.Add(MakeTaggedEnergyRecord(
				Before.BattleId,
				ResolutionId,
				1,
				Before.Energy,
				EnergyAfter));
			return Envelope;
		}

		FPresentationResolutionEnvelope MakeTwoRecordGroupEnvelope(
			int64 ResolutionId,
			const FPresentationStateSnapshot& Before,
			const FPresentationGroupTag& Group
		) const
		{
			FPresentationResolutionEnvelope Envelope;
			Envelope.BattleId = Before.BattleId;
			Envelope.ResolutionId = ResolutionId;
			Envelope.Origin = EPresentationResolutionOrigin::System;
			Envelope.FinalStateRevision = Before.StateRevision;
			Envelope.FinalSnapshot = Before;
			Envelope.FinalSnapshot.Energy = Before.Energy + 2;
			Envelope.Records.Add(MakeTaggedEnergyRecord(
				Before.BattleId,
				ResolutionId,
				1,
				Before.Energy,
				Before.Energy + 1,
				&Group));
			Envelope.Records.Add(MakeTaggedEnergyRecord(
				Before.BattleId,
				ResolutionId,
				2,
				Before.Energy + 1,
				Before.Energy + 2,
				&Group));
			return Envelope;
		}

		FPresentationResolutionEnvelope MakeEmptyEnvelope(
			int64 ResolutionId,
			const FPresentationStateSnapshot& FinalSnapshot
		) const
		{
			FPresentationResolutionEnvelope Envelope;
			Envelope.BattleId = FinalSnapshot.BattleId;
			Envelope.ResolutionId = ResolutionId;
			Envelope.Origin = EPresentationResolutionOrigin::System;
			Envelope.FinalStateRevision = FinalSnapshot.StateRevision;
			Envelope.FinalSnapshot = FinalSnapshot;
			return Envelope;
		}

		int64 PrepareRecordedOwnership(
			int32 RuntimeId,
			int64 ResolutionId
		)
		{
			if (ViewModel->HandCards.IndexOfByPredicate(
				[RuntimeId](const FBattleHUDCardView& Card)
				{
					return Card.RuntimeId == RuntimeId;
				}) == INDEX_NONE)
			{
				ViewModel->HandCards.Add(MakeDisplayedCard(RuntimeId));
			}

			const int64 Generation = ViewModel->BeginCardPresentationSelectionLifecycle(
				ViewModel->StateRevision);
			if (Generation <= 0) return 0;
			if (!ViewModel->SetPendingCardPresentationSelection(
				Generation,
				RuntimeId,
				true))
			{
				return 0;
			}
			if (!ViewModel->ConfirmCardPresentationSelection(
				Generation,
				{ RuntimeId }))
			{
				return 0;
			}
			if (!ViewModel->ArmRecordedCardPresentationCompletion(
				Generation,
				ResolutionId))
			{
				return 0;
			}
			return Generation;
		}

		FPresentationStateSnapshot SnapshotWithCurrentHand() const
		{
			FPresentationStateSnapshot Snapshot = Baseline;
			Snapshot.HandCards = ViewModel->HandCards;
			Snapshot.Energy = ViewModel->Energy;
			return Snapshot;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG3PlaybackUnitTrackingTest,
	"SlayTheSpireDemo.SelectionPresentation.G3.PlaybackUnitTracking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG3PlaybackUnitTrackingTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG3Tests;
	USelectionPresentationG3PlaybackWidget* Widget =
		NewObject<USelectionPresentationG3PlaybackWidget>();
	if (!TestNotNull(TEXT("Widget exists"), Widget)) return false;

	const FPresentationRecord Record = MakeTaggedEnergyRecord(91, 7, 1, 3, 2);
	const FPresentationPlaybackToken RecordToken = MakeRecordToken(91, 7, 1, 10);
	Widget->bAcceptAsyncRecordPlayback = false;
	TestFalse(
		TEXT("Failed SingleRecord Begin declines"),
		Widget->PlayPresentationRecord(Record, RecordToken, 3));
	TestFalse(
		TEXT("Failed SingleRecord Begin leaves no tracked owner"),
		Widget->HasTrackedPresentationPlayback());
	TestEqual(
		TEXT("Declined Begin emits no cancellation callback"),
		Widget->RecordCancelCallCount,
		0);

	const FPresentationGroupTag Group = MakeGroup(4, 2);
	TArray<FPresentationRecord> Records = {
		MakeTaggedEnergyRecord(91, 7, 1, 3, 2, &Group),
		MakeTaggedEnergyRecord(91, 7, 2, 2, 1, &Group)
	};
	const TArray<int32> Indices = { 3, 5 };
	const FPresentationPlaybackToken GroupToken = MakeGroupToken(91, 7, 1, 11, 4);
	TestTrue(
		TEXT("Group Begin accepts through dormant C++ surface"),
		Widget->PlayPresentationGroup(Records, Group, Indices, GroupToken));
	TestTrue(TEXT("Group is tracked"), Widget->HasTrackedPresentationPlayback());
	TestEqual(
		TEXT("Tracked token carries Group kind"),
		Widget->GetTrackedPresentationUnitKind(),
		EPresentationPlaybackUnitKind::Group);
	TestTrue(
		TEXT("Tracked token is exact Group token"),
		Widget->GetTrackedPresentationToken() == GroupToken);
	TestEqual(
		TEXT("Tracked Group keeps exact record-index set"),
		Widget->GetTrackedPresentationRecordIndices().Num(),
		2);
	TestFalse(
		TEXT("Different-kind token cannot cancel Group"),
		Widget->CancelTrackedPresentationPlayback(RecordToken));
	TestTrue(
		TEXT("Wrong cancellation leaves Group owner intact"),
		Widget->HasTrackedPresentationPlayback());
	TestTrue(
		TEXT("Exact Group token cancels tracked owner"),
		Widget->CancelTrackedPresentationPlayback(GroupToken));
	TestEqual(TEXT("Group cancel hook called once"), Widget->GroupCancelCallCount, 1);
	TestTrue(
		TEXT("Group cancel hook receives exact token"),
		Widget->LastCancelledGroupToken == GroupToken);
	TestEqual(
		TEXT("Group cancellation never dispatches Record hook"),
		Widget->RecordCancelCallCount,
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG3CrossKindStaleTest,
	"SlayTheSpireDemo.SelectionPresentation.G3.CrossKindStaleAndDeferred",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG3CrossKindStaleTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG3Tests;
	USelectionPresentationG3PlaybackWidget* Widget =
		NewObject<USelectionPresentationG3PlaybackWidget>();
	if (!TestNotNull(TEXT("Widget exists"), Widget)) return false;

	const FPresentationGroupTag Group = MakeGroup(5, 2);
	const TArray<FPresentationRecord> GroupRecords = {
		MakeTaggedEnergyRecord(92, 8, 1, 3, 2, &Group),
		MakeTaggedEnergyRecord(92, 8, 2, 2, 1, &Group)
	};
	const TArray<int32> Indices = { 0, 2 };
	const FPresentationPlaybackToken GroupToken = MakeGroupToken(92, 8, 1, 20, 5);
	const FPresentationRecord Record = MakeTaggedEnergyRecord(92, 8, 3, 1, 0);
	const FPresentationPlaybackToken RecordToken = MakeRecordToken(92, 8, 3, 21);

	Widget->bNotifySynchronouslyFromGroupPlay = true;
	TestTrue(
		TEXT("Group begins even when concrete surface notifies synchronously"),
		Widget->PlayPresentationGroup(GroupRecords, Group, Indices, GroupToken));
	TestTrue(TEXT("Group synchronous Notify is deferred"), Widget->HasTrackedPresentationPlayback());

	TestTrue(
		TEXT("New SingleRecord replaces Group before deferred callback"),
		Widget->PlayPresentationRecord(Record, RecordToken, 4));
	TestEqual(TEXT("Replacing Group cancels exact Group once"), Widget->GroupCancelCallCount, 1);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestTrue(
		TEXT("Stale Group callback cannot clear newer SingleRecord"),
		Widget->HasTrackedPresentationPlayback());
	TestTrue(
		TEXT("Newer SingleRecord token survives stale Group callback"),
		Widget->GetTrackedPresentationToken() == RecordToken);

	Widget->bNotifySynchronouslyFromGroupPlay = false;
	Widget->bNotifySynchronouslyFromRecordPlay = true;
	const FPresentationPlaybackToken RecordToken2 = MakeRecordToken(92, 8, 3, 22);
	TestTrue(
		TEXT("Second SingleRecord starts with synchronous deferred Notify"),
		Widget->PlayPresentationRecord(Record, RecordToken2, 4));
	const FPresentationPlaybackToken GroupToken2 = MakeGroupToken(92, 8, 1, 23, 5);
	TestTrue(
		TEXT("New Group replaces SingleRecord before deferred callback"),
		Widget->PlayPresentationGroup(GroupRecords, Group, Indices, GroupToken2));
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestTrue(
		TEXT("Stale SingleRecord callback cannot clear newer Group"),
		Widget->HasTrackedPresentationPlayback());
	TestTrue(
		TEXT("Newer Group token survives stale SingleRecord callback"),
		Widget->GetTrackedPresentationToken() == GroupToken2);

	Widget->NotifyPresentationFinished(GroupToken2);
	Widget->NotifyPresentationFinished(GroupToken2);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestFalse(
		TEXT("Exact completion clears Group once and duplicate is stale"),
		Widget->HasTrackedPresentationPlayback());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG3ActiveEnvelopeRecoveryTest,
	"SlayTheSpireDemo.SelectionPresentation.G3.ActiveEnvelopeRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG3ActiveEnvelopeRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG3Tests;
	FControllerFixture Fixture;
	if (!TestTrue(TEXT("Fixture created"), Fixture.IsReady())) return false;

	const int64 ResolutionA = Fixture.BaselineResolutionId + 1;
	const int64 ResolutionB = Fixture.BaselineResolutionId + 2;
	const FPresentationResolutionEnvelope EnvelopeA = Fixture.MakeEnergyEnvelope(
		ResolutionA,
		Fixture.Baseline,
		Fixture.Baseline.Energy + 1);
	const FPresentationResolutionEnvelope EnvelopeB = Fixture.MakeEnergyEnvelope(
		ResolutionB,
		EnvelopeA.FinalSnapshot,
		Fixture.Baseline.Energy + 2);

	Fixture.Battle->OnPresentationResolutionReady.Broadcast(EnvelopeA);
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(EnvelopeB);
	TestEqual(TEXT("A active plus B queued"), Fixture.Controller->GetBacklogCountForTesting(), 2);
	TestEqual(TEXT("Only A offered before recovery"), Fixture.Widget->RecordPlayCallCount, 1);
	const int32 CancelBefore = Fixture.Widget->RecordCancelCallCount;

	Fixture.Controller->ReconcileActiveEnvelopeToFinalSnapshotForTesting();
	TestEqual(
		TEXT("Recovery completes exact active Resolution A"),
		Fixture.Controller->GetLastCompletedResolutionIdForTesting(),
		ResolutionA);
	TestEqual(TEXT("A FinalSnapshot is displayed"), Fixture.ViewModel->Energy, EnvelopeA.FinalSnapshot.Energy);
	TestEqual(TEXT("B survives and becomes active"), Fixture.Controller->GetBacklogCountForTesting(), 1);
	TestEqual(TEXT("B is offered after A recovery"), Fixture.Widget->RecordPlayCallCount, 2);
	TestTrue(TEXT("B waits normally"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestEqual(
		TEXT("Recovery cancels only A playback unit"),
		Fixture.Widget->RecordCancelCallCount,
		CancelBefore + 1);
	TestFalse(
		TEXT("Presentation recovery never faults Gameplay"),
		Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG3GroupTimeoutScopeTest,
	"SlayTheSpireDemo.SelectionPresentation.G3.GroupTimeoutScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG3GroupTimeoutScopeTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG3Tests;
	FControllerFixture Fixture;
	if (!TestTrue(TEXT("Fixture created"), Fixture.IsReady())) return false;

	const int64 ResolutionA = Fixture.BaselineResolutionId + 1;
	const int64 ResolutionB = Fixture.BaselineResolutionId + 2;
	const FPresentationGroupTag Group = MakeGroup(7001, 2);
	const FPresentationResolutionEnvelope EnvelopeA = Fixture.MakeTwoRecordGroupEnvelope(
		ResolutionA,
		Fixture.Baseline,
		Group);
	const FPresentationResolutionEnvelope EnvelopeB = Fixture.MakeEnergyEnvelope(
		ResolutionB,
		EnvelopeA.FinalSnapshot,
		EnvelopeA.FinalSnapshot.Energy + 1);

	Fixture.Battle->OnPresentationResolutionReady.Broadcast(EnvelopeA);
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(EnvelopeB);
	TestTrue(
		TEXT("Test-only rebind establishes exact dormant Group unit"),
		Fixture.Controller->RebindActivePlaybackAsGroupForTesting(Group, { 0, 1 }));
	TestEqual(
		TEXT("Rebind cancels the previously offered leader SingleRecord"),
		Fixture.Widget->RecordCancelCallCount,
		1);
	TestEqual(TEXT("One Group Begin is offered"), Fixture.Widget->GroupPlayCallCount, 1);
	TestEqual(
		TEXT("Active token carries Group kind"),
		Fixture.Controller->GetActivePlaybackUnitKindForTesting(),
		EPresentationPlaybackUnitKind::Group);
	const FPresentationPlaybackToken GroupToken = Fixture.Controller->GetActivePlaybackTokenForTesting();

	Fixture.Controller->ExpireActivePlaybackForTesting();
	TestEqual(TEXT("Group timeout cancels exact Group once"), Fixture.Widget->GroupCancelCallCount, 1);
	TestTrue(
		TEXT("Group timeout cancellation uses exact active token"),
		Fixture.Widget->LastCancelledGroupToken == GroupToken);
	TestEqual(
		TEXT("Group timeout completes only active Resolution A"),
		Fixture.Controller->GetLastCompletedResolutionIdForTesting(),
		ResolutionA);
	TestEqual(TEXT("Group timeout displays A FinalSnapshot"), Fixture.ViewModel->Energy, EnvelopeA.FinalSnapshot.Energy);
	TestEqual(TEXT("Later B remains active backlog"), Fixture.Controller->GetBacklogCountForTesting(), 1);
	TestEqual(
		TEXT("B resumes as ordinary SingleRecord"),
		Fixture.Controller->GetActivePlaybackUnitKindForTesting(),
		EPresentationPlaybackUnitKind::SingleRecord);
	TestEqual(TEXT("B SingleRecord offered after recovery"), Fixture.Widget->RecordPlayCallCount, 2);
	TestFalse(
		TEXT("Group timeout never faults Gameplay"),
		Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG3ZeroRecordExactCompletionTest,
	"SlayTheSpireDemo.SelectionPresentation.G3.ZeroRecordExactCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG3ZeroRecordExactCompletionTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG3Tests;
	FControllerFixture Fixture;
	if (!TestTrue(TEXT("Fixture created"), Fixture.IsReady())) return false;

	const int64 ResolutionA = Fixture.BaselineResolutionId + 1;
	const int32 RuntimeId = 8101;
	TestTrue(
		TEXT("Recorded ownership lifecycle prepared"),
		Fixture.PrepareRecordedOwnership(RuntimeId, ResolutionA) > 0);
	TestEqual(
		TEXT("Confirmed card remains SelectionArea-owned before exact completion"),
		Fixture.ViewModel->GetCardPresentationOwner(RuntimeId),
		ECardPresentationOwner::SelectionArea);

	Fixture.ViewModel->MarkPresentationResolutionCompleted(
		Fixture.Baseline.BattleId,
		ResolutionA + 100);
	TestEqual(
		TEXT("Larger unrelated ResolutionId is not an ordinal completion proof"),
		Fixture.ViewModel->GetCardPresentationOwner(RuntimeId),
		ECardPresentationOwner::SelectionArea);

	FPresentationStateSnapshot FinalSnapshot = Fixture.SnapshotWithCurrentHand();
	const FPresentationResolutionEnvelope EmptyEnvelope = Fixture.MakeEmptyEnvelope(
		ResolutionA,
		FinalSnapshot);
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(EmptyEnvelope);
	TestEqual(
		TEXT("Zero-record Envelope completes normally"),
		Fixture.Controller->GetLastCompletedResolutionIdForTesting(),
		ResolutionA);
	TestEqual(
		TEXT("Exact zero-record completion restores Hand ownership"),
		Fixture.ViewModel->GetCardPresentationOwner(RuntimeId),
		ECardPresentationOwner::Hand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG3RecoveryExactWatermarkTest,
	"SlayTheSpireDemo.SelectionPresentation.G3.RecoveryExactWatermark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG3RecoveryExactWatermarkTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG3Tests;
	FControllerFixture Fixture;
	if (!TestTrue(TEXT("Fixture created"), Fixture.IsReady())) return false;

	const int64 ResolutionA = Fixture.BaselineResolutionId + 1;
	const int64 ResolutionB = Fixture.BaselineResolutionId + 2;
	const int32 RuntimeA = 8201;
	const int32 RuntimeB = 8202;
	TestTrue(TEXT("A ownership prepared"), Fixture.PrepareRecordedOwnership(RuntimeA, ResolutionA) > 0);
	TestTrue(TEXT("B ownership prepared"), Fixture.PrepareRecordedOwnership(RuntimeB, ResolutionB) > 0);

	FPresentationStateSnapshot Before = Fixture.SnapshotWithCurrentHand();
	const FPresentationResolutionEnvelope EnvelopeA = Fixture.MakeEnergyEnvelope(
		ResolutionA,
		Before,
		Before.Energy + 1);
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(EnvelopeA);
	Fixture.Controller->ReconcileActiveEnvelopeToFinalSnapshotForTesting();

	TestEqual(
		TEXT("Recovered A exact watermark restores only A"),
		Fixture.ViewModel->GetCardPresentationOwner(RuntimeA),
		ECardPresentationOwner::Hand);
	TestEqual(
		TEXT("Unreached B watermark remains owned"),
		Fixture.ViewModel->GetCardPresentationOwner(RuntimeB),
		ECardPresentationOwner::SelectionArea);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG3GlobalSkipTest,
	"SlayTheSpireDemo.SelectionPresentation.G3.GlobalSkipEntireBacklog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG3GlobalSkipTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG3Tests;
	FControllerFixture Fixture;
	if (!TestTrue(TEXT("Fixture created"), Fixture.IsReady())) return false;

	const int64 ResolutionA = Fixture.BaselineResolutionId + 1;
	const int64 ResolutionB = Fixture.BaselineResolutionId + 2;
	const int32 RuntimeA = 8301;
	const int32 RuntimeB = 8302;
	TestTrue(TEXT("A ownership prepared"), Fixture.PrepareRecordedOwnership(RuntimeA, ResolutionA) > 0);
	TestTrue(TEXT("B ownership prepared"), Fixture.PrepareRecordedOwnership(RuntimeB, ResolutionB) > 0);

	FPresentationStateSnapshot Before = Fixture.SnapshotWithCurrentHand();
	const FPresentationResolutionEnvelope EnvelopeA = Fixture.MakeEnergyEnvelope(
		ResolutionA,
		Before,
		Before.Energy + 1);
	const FPresentationResolutionEnvelope EnvelopeB = Fixture.MakeEnergyEnvelope(
		ResolutionB,
		EnvelopeA.FinalSnapshot,
		Before.Energy + 2);
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(EnvelopeA);
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(EnvelopeB);
	const int32 CancelBefore = Fixture.Widget->RecordCancelCallCount;

	Fixture.Controller->SkipPresentation();
	TestEqual(TEXT("Global Skip clears entire backlog"), Fixture.Controller->GetBacklogCountForTesting(), 0);
	TestEqual(
		TEXT("Global Skip catches up to newest Resolution"),
		Fixture.Controller->GetLastCompletedResolutionIdForTesting(),
		ResolutionB);
	TestEqual(TEXT("Global Skip displays newest FinalSnapshot"), Fixture.ViewModel->Energy, EnvelopeB.FinalSnapshot.Energy);
	TestEqual(TEXT("Global Skip completes A ownership"), Fixture.ViewModel->GetCardPresentationOwner(RuntimeA), ECardPresentationOwner::Hand);
	TestEqual(TEXT("Global Skip completes B ownership"), Fixture.ViewModel->GetCardPresentationOwner(RuntimeB), ECardPresentationOwner::Hand);
	TestEqual(
		TEXT("Global Skip cancels exact active visual once"),
		Fixture.Widget->RecordCancelCallCount,
		CancelBefore + 1);
	return true;
}

#endif