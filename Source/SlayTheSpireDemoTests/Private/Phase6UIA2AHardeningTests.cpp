#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/BattlePresentationRecorder.h"
#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2AHardeningTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

		explicit FFixture(bool bEnableRecording = true)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle))
			{
				return;
			}

			Player->MaxHP = 80;
			Enemy->MaxHP = 50;
			Player->PresentationId = TEXT("PlayerStable");
			Enemy->PresentationId = TEXT("EnemyStable");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->bEnableCommittedPresentationRecording = bEnableRecording;
			Battle->StartBattle();
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
				&& Battle->BattleState == EBattleState::PlayerTurn
				&& IsValid(Battle->GetActionQueueForTesting());
		}

		void Flush() const
		{
			if (IsValid(Battle))
			{
				Battle->FlushScheduledReadStateReadyForTesting();
			}
		}
	};

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (Fixture.IsReady())
		{
			return true;
		}
		Test.AddError(TEXT("Failed to create Phase 6UI-A2A hardening fixture."));
		return false;
	}

	FPresentationResolutionEnvelope MakeFaultEnvelope(
		const FPresentationStateSnapshot& Snapshot,
		int64 ResolutionId,
		int64 Sequence
	)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Snapshot.BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Snapshot.StateRevision;
		Envelope.FinalSnapshot = Snapshot;

		FPresentationRecord Fault;
		Fault.BattleId = Snapshot.BattleId;
		Fault.ResolutionId = ResolutionId;
		Fault.PresentationSequence = Sequence;
		Fault.Type = EBattlePresentationRecordType::ResolutionFault;
		Fault.FaultReason = TEXT("A2A hardening probe fault");
		Envelope.Records.Add(Fault);
		return Envelope;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2ARecorderHardeningTest,
	"SlayTheSpireDemo.Phase6UIA2A.Hardening.RecorderLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2ARecorderHardeningTest::RunTest(const FString& Parameters)
{
	UBattlePresentationRecorder* Recorder = NewObject<UBattlePresentationRecorder>();
	if (!TestNotNull(TEXT("Recorder created"), Recorder))
	{
		return false;
	}
	Recorder->ResetForBattle(77);

	FPresentationRecordWriter FirstWriter;
	TestTrue(TEXT("First Resolution begins"), Recorder->BeginResolution(EPresentationResolutionOrigin::System, FirstWriter));
	FPresentationRecord FirstRecord;
	FirstRecord.Type = EBattlePresentationRecordType::None;
	TestTrue(TEXT("First builder accepts a record"), FirstWriter.Append(FirstRecord));

	FPresentationRecordWriter OverlapWriter;
	TestFalse(TEXT("Overlapping Begin is rejected"), Recorder->BeginResolution(EPresentationResolutionOrigin::System, OverlapWriter));
	TestFalse(TEXT("Rejected overlapping Begin cannot leave stale builder active"), Recorder->HasActiveResolution());
	TestFalse(TEXT("Rejected overlapping Begin returns no usable writer"), OverlapWriter.IsAvailable());
	TestFalse(TEXT("Writer from cleared overlapping builder is no longer available"), FirstWriter.IsAvailable());

	FPresentationRecordWriter FaultWriter;
	TestTrue(TEXT("Fresh Resolution begins after stale builder was cleared"), Recorder->BeginResolution(EPresentationResolutionOrigin::System, FaultWriter));
	FPresentationRecord FaultRecord;
	FaultRecord.Type = EBattlePresentationRecordType::ResolutionFault;
	TestTrue(TEXT("ResolutionFault record appends"), FaultWriter.Append(FaultRecord));
	FPresentationRecord AfterFault;
	AfterFault.Type = EBattlePresentationRecordType::None;
	TestFalse(TEXT("No record may append after terminal ResolutionFault"), FaultWriter.Append(AfterFault));
	TestFalse(TEXT("Post-fault append invalidates active record batch"), Recorder->IsActiveResolutionValid());
	TestFalse(TEXT("Invalidated builder makes previously issued writer unavailable"), FaultWriter.IsAvailable());
	TestEqual(TEXT("Invalidated terminal batch discards unpublished records"), Recorder->GetActiveRecordCountForTesting(), 0);

	FPresentationStateSnapshot Snapshot;
	Snapshot.BattleId = 77;
	Snapshot.StateRevision = 1;
	FPresentationResolutionEnvelope Envelope;
	TestFalse(TEXT("Invalidated terminal batch cannot seal"), Recorder->SealResolution(Snapshot, Envelope));
	TestFalse(TEXT("Failed seal releases invalid builder"), Recorder->HasActiveResolution());
	TestFalse(TEXT("Writer remains unavailable after invalid builder is released"), FaultWriter.IsAvailable());

	FPresentationRecordWriter SealedWriter;
	TestTrue(TEXT("Clean Resolution begins for stale-writer seal regression"), Recorder->BeginResolution(EPresentationResolutionOrigin::System, SealedWriter));
	TestTrue(TEXT("Clean writer is available while its builder is active"), SealedWriter.IsAvailable());
	TestTrue(TEXT("Clean Resolution seals"), Recorder->SealResolution(Snapshot, Envelope));
	TestFalse(TEXT("Sealed writer no longer reports available"), SealedWriter.IsAvailable());
	FPresentationRecord StaleAppend;
	StaleAppend.Type = EBattlePresentationRecordType::None;
	TestFalse(TEXT("Sealed writer cannot append into a later builder"), SealedWriter.Append(StaleAppend));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2APresentationIdentityAndDirectModeTest,
	"SlayTheSpireDemo.Phase6UIA2A.Hardening.IdentityAndDirectMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2APresentationIdentityAndDirectModeTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2AHardeningTest;

	FFixture IdentityFixture;
	if (!RequireReady(*this, IdentityFixture))
	{
		return false;
	}
	IdentityFixture.Flush();

	FPresentationStateSnapshot FirstBaseline;
	TestTrue(TEXT("First frozen baseline exists"), IdentityFixture.Battle->TryGetLatestFrozenPresentationBaseline(FirstBaseline));
	const FName FrozenPlayerId = FirstBaseline.Player.PresentationId;
	const FName FrozenEnemyId = FirstBaseline.Enemy.PresentationId;

	IdentityFixture.Player->PresentationId = TEXT("MutatedPlayerId");
	IdentityFixture.Enemy->PresentationId = TEXT("MutatedEnemyId");
	FName ResolvedPlayerId = NAME_None;
	FName ResolvedEnemyId = NAME_None;
	TestTrue(TEXT("Player PresentationId still resolves after authored mutation"), IdentityFixture.Battle->TryResolveCombatantPresentationId(IdentityFixture.Player, ResolvedPlayerId));
	TestTrue(TEXT("Enemy PresentationId still resolves after authored mutation"), IdentityFixture.Battle->TryResolveCombatantPresentationId(IdentityFixture.Enemy, ResolvedEnemyId));
	TestEqual(TEXT("Player resolved PresentationId is immutable for battle lifetime"), ResolvedPlayerId, FrozenPlayerId);
	TestEqual(TEXT("Enemy resolved PresentationId is immutable for battle lifetime"), ResolvedEnemyId, FrozenEnemyId);

	TestTrue(TEXT("System Resolution begins after authored-id mutation"), IdentityFixture.Battle->BeginSystemPresentationResolutionForTesting());
	TestTrue(TEXT("System Resolution seals using frozen battle-scoped IDs"), IdentityFixture.Battle->SealActivePresentationResolutionForTesting());
	FPresentationStateSnapshot LaterBaseline;
	TestTrue(TEXT("Later frozen baseline exists"), IdentityFixture.Battle->TryGetLatestFrozenPresentationBaseline(LaterBaseline));
	TestEqual(TEXT("Later Player snapshot keeps original resolved ID"), LaterBaseline.Player.PresentationId, FrozenPlayerId);
	TestEqual(TEXT("Later Enemy snapshot keeps original resolved ID"), LaterBaseline.Enemy.PresentationId, FrozenEnemyId);

	FFixture DirectFixture(false);
	if (!RequireReady(*this, DirectFixture))
	{
		return false;
	}
	DirectFixture.Flush();
	UBattleHUDViewModel* DirectVM = NewObject<UBattleHUDViewModel>(DirectFixture.World);
	TestTrue(TEXT("No-history direct ViewModel initializes"), DirectVM->Initialize(DirectFixture.Battle, false));
	const int64 OldRevision = DirectVM->StateRevision;
	TestFalse(TEXT("No-history direct ViewModel is not Controller-owned"), DirectVM->IsPresentationDisplayOwned());
	TestTrue(TEXT("No-history direct ViewModel starts request-eligible"), !DirectVM->bInputLocked);

	TestTrue(TEXT("Gameplay request succeeds with committed recording disabled"), DirectFixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	TestEqual(TEXT("Gameplay remains authoritative and completes normally"), DirectFixture.Battle->BattleState, EBattleState::PlayerTurn);
	TestEqual(TEXT("Direct ViewModel does not update before deferred read edge"), DirectVM->StateRevision, OldRevision);
	DirectFixture.Flush();
	TestTrue(TEXT("Direct frozen-baseline owner advances after deferred read edge"), DirectVM->StateRevision > OldRevision);
	TestFalse(TEXT("Direct frozen-baseline owner refreshes current input bindings"), DirectVM->bInputLocked);

	// Recording configuration is immutable for the current BattleId. Editing the
	// exposed config during play must not orphan an already Controller-owned HUD.
	FFixture LatchedFixture(true);
	if (!RequireReady(*this, LatchedFixture))
	{
		return false;
	}
	LatchedFixture.Flush();
	UBattleHUDViewModel* LatchedVM = NewObject<UBattleHUDViewModel>(LatchedFixture.World);
	TestTrue(TEXT("Latched-config Controller-owned ViewModel initializes"), LatchedVM->Initialize(LatchedFixture.Battle, true));
	UBattlePresentationController* LatchedController = NewObject<UBattlePresentationController>(LatchedFixture.World);
	TestTrue(TEXT("Latched-config Controller initializes"), LatchedController->Initialize(LatchedFixture.Battle, LatchedVM, nullptr));
	const int64 LatchedOldRevision = LatchedVM->StateRevision;

	LatchedFixture.Battle->bEnableCommittedPresentationRecording = false;
	TestTrue(
		TEXT("Runtime config edit does not change current BattleId recording latch"),
		LatchedFixture.Battle->IsCommittedPresentationRecordingEnabledForBattle()
	);
	TestTrue(TEXT("Request still establishes Presentation for the latched battle"), LatchedFixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	TestEqual(TEXT("Controller-owned HUD waits for deferred delivery"), LatchedVM->StateRevision, LatchedOldRevision);
	LatchedFixture.Flush();
	TestTrue(TEXT("Latched recording keeps Controller-owned HUD advancing"), LatchedVM->StateRevision > LatchedOldRevision);
	TestFalse(TEXT("Latched recording HUD catches up and unlocks input"), LatchedVM->bInputLocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2AControllerStaleIsolationTest,
	"SlayTheSpireDemo.Phase6UIA2A.Hardening.ControllerStaleIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2AControllerStaleIsolationTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2AHardeningTest;

	// A Controller can subscribe after an Envelope has sealed but before its
	// deferred public callback. The frozen baseline already contains that result,
	// so bootstrap must seed a Resolution watermark and suppress replay.
	FFixture LatePendingFixture;
	if (!RequireReady(*this, LatePendingFixture))
	{
		return false;
	}
	TestTrue(TEXT("BattleStart Envelope is still pending before late Controller subscribes"), LatePendingFixture.Battle->GetPendingPresentationDeliveryCountForTesting() > 0);
	FPresentationStateSnapshot LatePendingBaseline;
	TestTrue(TEXT("Late Controller bootstrap baseline exists"), LatePendingFixture.Battle->TryGetLatestFrozenPresentationBaseline(LatePendingBaseline));
	const int64 LatePendingWatermark = static_cast<int64>(LatePendingFixture.Battle->GetLatestFrozenPresentationBaselineResolutionId());
	TestTrue(TEXT("Bootstrap baseline carries a sealed Resolution watermark"), LatePendingWatermark > 0);
	UBattleHUDViewModel* LatePendingVM = NewObject<UBattleHUDViewModel>(LatePendingFixture.World);
	TestTrue(TEXT("Late Controller-owned ViewModel initializes"), LatePendingVM->Initialize(LatePendingFixture.Battle, true));
	UBattlePresentationController* LatePendingController = NewObject<UBattlePresentationController>(LatePendingFixture.World);
	TestTrue(TEXT("Late Controller initializes from frozen baseline"), LatePendingController->Initialize(LatePendingFixture.Battle, LatePendingVM, nullptr));
	TestEqual(TEXT("Late Controller seeds completed watermark from baseline"), LatePendingController->GetLastCompletedResolutionIdForTesting(), LatePendingWatermark);
	LatePendingFixture.Flush();
	TestEqual(TEXT("Pending historical Envelope does not replay after late subscribe"), LatePendingController->GetLastCompletedResolutionIdForTesting(), LatePendingWatermark);
	TestFalse(TEXT("Late historical delivery cannot enter playback wait"), LatePendingController->IsWaitingForCompletionForTesting());

	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}
	Fixture.Flush();

	FPresentationStateSnapshot OldBaseline;
	TestTrue(TEXT("Old battle baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(OldBaseline));
	UBattleHUDViewModel* VM = NewObject<UBattleHUDViewModel>(Fixture.World);
	TestTrue(TEXT("Controller-owned ViewModel initializes"), VM->Initialize(Fixture.Battle, true));
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	TestTrue(TEXT("Controller initializes"), Controller->Initialize(Fixture.Battle, VM, nullptr));

	Fixture.Battle->StartBattle();
	FPresentationStateSnapshot NewBaseline;
	TestTrue(TEXT("Restart freezes a new battle baseline before public delivery"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(NewBaseline));
	TestTrue(TEXT("Restart changes BattleId"), NewBaseline.BattleId != OldBaseline.BattleId);
	const int64 CompletedBeforeStale = Controller->GetLastCompletedResolutionIdForTesting();
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(MakeFaultEnvelope(OldBaseline, 900, 900));
	TestEqual(TEXT("Old-battle Envelope is ignored after restart"), Controller->GetLastCompletedResolutionIdForTesting(), CompletedBeforeStale);
	TestFalse(TEXT("Old-battle Envelope cannot enter playback wait"), Controller->IsWaitingForCompletionForTesting());
	Fixture.Flush();
	TestEqual(TEXT("Deferred new-battle Envelope becomes displayed"), VM->BattleId, NewBaseline.BattleId);

	// A replaced widget invalidates old playback tokens, and destruction of the old
	// widget must not skip a newer playback owned by the replacement widget.
	UPhase6UIA2APlaybackWidget* OldWidget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	UPhase6UIA2APlaybackWidget* NewWidget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	Controller->SetWidget(OldWidget);
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(MakeFaultEnvelope(NewBaseline, 1000, 1000));
	TestTrue(TEXT("Old widget owns first async playback"), Controller->IsWaitingForCompletionForTesting());
	Controller->SetWidget(NewWidget);
	TestFalse(TEXT("Widget replacement deterministically catches up old playback"), Controller->IsWaitingForCompletionForTesting());

	Fixture.Battle->OnPresentationResolutionReady.Broadcast(MakeFaultEnvelope(NewBaseline, 1001, 1001));
	TestTrue(TEXT("Replacement widget owns new async playback"), Controller->IsWaitingForCompletionForTesting());
	Controller->NotifyWidgetLost(OldWidget);
	TestTrue(TEXT("Late destruction of stale widget cannot skip replacement playback"), Controller->IsWaitingForCompletionForTesting());
	Controller->NotifyPresentationFinished(NewWidget->LastToken);
	TestFalse(TEXT("Replacement widget valid token completes normally"), Controller->IsWaitingForCompletionForTesting());

	// PresentationUnavailable must invalidate an already in-flight playback token,
	// apply the newest frozen Gameplay result, and leave the HUD visibly locked in
	// the failure state rather than allowing a late timeout/callback to overwrite it.
	FFixture SealFailureFixture;
	if (!RequireReady(*this, SealFailureFixture))
	{
		return false;
	}
	SealFailureFixture.Flush();
	UBattleHUDViewModel* SealFailureVM = NewObject<UBattleHUDViewModel>(SealFailureFixture.World);
	TestTrue(TEXT("Seal-failure ViewModel initializes Controller-owned"), SealFailureVM->Initialize(SealFailureFixture.Battle, true));
	UPhase6UIA2APlaybackWidget* SealFailureWidget = NewObject<UPhase6UIA2APlaybackWidget>(SealFailureFixture.World);
	UBattlePresentationController* SealFailureController = NewObject<UBattlePresentationController>(SealFailureFixture.World);
	TestTrue(TEXT("Seal-failure Controller initializes"), SealFailureController->Initialize(SealFailureFixture.Battle, SealFailureVM, SealFailureWidget));
	FPresentationStateSnapshot SealFailureBaseline;
	TestTrue(TEXT("Seal-failure starting baseline exists"), SealFailureFixture.Battle->TryGetLatestFrozenPresentationBaseline(SealFailureBaseline));
	const int64 SealFailureOldRevision = SealFailureVM->StateRevision;
	const int64 SyntheticResolutionId = static_cast<int64>(SealFailureFixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1000;
	SealFailureFixture.Battle->OnPresentationResolutionReady.Broadcast(
		MakeFaultEnvelope(SealFailureBaseline, SyntheticResolutionId, SyntheticResolutionId)
	);
	TestTrue(TEXT("Synthetic historical record is in async playback before failure"), SealFailureController->IsWaitingForCompletionForTesting());

	SealFailureFixture.Battle->GetPresentationRecorderForTesting()->SetForceNextSealFailureForTesting(true);
	TestTrue(TEXT("Gameplay request remains accepted when Presentation seal fails"), SealFailureFixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	TestFalse(TEXT("Seal failure disables Presentation only"), SealFailureFixture.Battle->IsPresentationAvailable());
	FPresentationStateSnapshot SealFailureLatest;
	TestTrue(TEXT("Seal failure still leaves exact latest frozen baseline"), SealFailureFixture.Battle->TryGetLatestFrozenPresentationBaseline(SealFailureLatest));
	TestTrue(TEXT("Gameplay resolution advanced beyond displayed revision"), SealFailureLatest.StateRevision > SealFailureOldRevision);
	SealFailureFixture.Flush();
	TestFalse(TEXT("PresentationUnavailable cancels in-flight playback"), SealFailureController->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Unavailable HUD catches up to latest frozen revision"), SealFailureVM->StateRevision, SealFailureLatest.StateRevision);
	TestEqual(TEXT("Unavailable HUD catches up to latest frozen BattleId"), SealFailureVM->BattleId, SealFailureLatest.BattleId);
	TestEqual(TEXT("Unavailable state cannot be overwritten by old playback"), SealFailureVM->InteractionState, EBattleHUDInteractionState::PresentationUnavailable);
	TestTrue(TEXT("PresentationUnavailable keeps input locked"), SealFailureVM->bInputLocked);
	SealFailureController->NotifyPresentationFinished(SealFailureWidget->LastToken);
	TestEqual(TEXT("Stale completion after failure is ignored"), SealFailureVM->InteractionState, EBattleHUDInteractionState::PresentationUnavailable);

	// Append failure follows the same no-partial-history catch-up path. The failed
	// same-revision System batch disables Presentation, and the next normal Gameplay
	// resolution must still advance the frozen HUD before exposing the error state.
	FFixture AppendFailureFixture;
	if (!RequireReady(*this, AppendFailureFixture))
	{
		return false;
	}
	AppendFailureFixture.Flush();
	UBattleHUDViewModel* AppendFailureVM = NewObject<UBattleHUDViewModel>(AppendFailureFixture.World);
	TestTrue(TEXT("Append-failure ViewModel initializes Controller-owned"), AppendFailureVM->Initialize(AppendFailureFixture.Battle, true));
	UBattlePresentationController* AppendFailureController = NewObject<UBattlePresentationController>(AppendFailureFixture.World);
	TestTrue(TEXT("Append-failure Controller initializes"), AppendFailureController->Initialize(AppendFailureFixture.Battle, AppendFailureVM, nullptr));
	const int64 AppendFailureOldRevision = AppendFailureVM->StateRevision;
	TestTrue(TEXT("Append-failure System Resolution begins"), AppendFailureFixture.Battle->BeginSystemPresentationResolutionForTesting());
	FPresentationRecord FirstRecord;
	FirstRecord.Type = EBattlePresentationRecordType::None;
	TestTrue(TEXT("Append-failure first record commits"), AppendFailureFixture.Battle->GetActivePresentationRecordWriterForTesting().Append(FirstRecord));
	AppendFailureFixture.Battle->GetPresentationRecorderForTesting()->SetForceNextAppendFailureForTesting(true);
	FPresentationRecord FailedRecord;
	FailedRecord.Type = EBattlePresentationRecordType::None;
	TestFalse(TEXT("Forced append failure invalidates unpublished history"), AppendFailureFixture.Battle->GetActivePresentationRecordWriterForTesting().Append(FailedRecord));
	TestFalse(TEXT("Invalid append batch cannot seal"), AppendFailureFixture.Battle->SealActivePresentationResolutionForTesting());
	TestFalse(TEXT("Append failure disables Presentation only"), AppendFailureFixture.Battle->IsPresentationAvailable());
	TestTrue(TEXT("Gameplay still accepts later request after append failure"), AppendFailureFixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	FPresentationStateSnapshot AppendFailureLatest;
	TestTrue(TEXT("Append failure path still freezes latest Gameplay baseline"), AppendFailureFixture.Battle->TryGetLatestFrozenPresentationBaseline(AppendFailureLatest));
	AppendFailureFixture.Flush();
	TestTrue(TEXT("Append-failure HUD advances beyond old revision"), AppendFailureVM->StateRevision > AppendFailureOldRevision);
	TestEqual(TEXT("Append-failure HUD applies newest frozen revision"), AppendFailureVM->StateRevision, AppendFailureLatest.StateRevision);
	TestEqual(TEXT("Append-failure HUD remains PresentationUnavailable"), AppendFailureVM->InteractionState, EBattleHUDInteractionState::PresentationUnavailable);
	TestTrue(TEXT("Append-failure HUD remains input locked"), AppendFailureVM->bInputLocked);
	TestFalse(TEXT("Append-failure Controller has no stale playback"), AppendFailureController->IsWaitingForCompletionForTesting());
	return true;
}

#endif
