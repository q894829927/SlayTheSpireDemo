#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/PresentationDamageReducer.h"
#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG8ATest
{
	FPresentationStateSnapshot MakeDamageBaseline()
	{
		FPresentationStateSnapshot Snapshot;
		Snapshot.BattleId = 77;
		Snapshot.StateRevision = 10;
		Snapshot.BattleState = EBattleState::PlayerTurn;
		Snapshot.Outcome = EBattleHUDOutcome::None;
		Snapshot.Player.PresentationId = TEXT("Player");
		Snapshot.Player.HP = 80;
		Snapshot.Player.MaxHP = 80;
		Snapshot.Player.Block = 5;
		Snapshot.Player.bDead = false;
		Snapshot.Enemy.PresentationId = TEXT("Enemy");
		Snapshot.Enemy.HP = 50;
		Snapshot.Enemy.MaxHP = 50;
		Snapshot.Enemy.Block = 0;
		Snapshot.Enemy.bDead = false;
		return Snapshot;
	}

	FDamagePresentationPayload MakeValidDamage()
	{
		FDamagePresentationPayload Damage;
		Damage.SourcePresentationId = TEXT("Enemy");
		Damage.TargetPresentationId = TEXT("Player");
		Damage.DamageKind = EDamageKind::Attack;
		Damage.IncomingDamage = 12;
		Damage.HPBefore = 80;
		Damage.HPAfter = 73;
		Damage.BlockBefore = 5;
		Damage.BlockAfter = 0;
		Damage.BlockedDamage = 5;
		Damage.HPDamage = 7;
		return Damage;
	}

	struct FControllerFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

		FControllerFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

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
			Battle->bEnableCommittedPresentationRecording = true;
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
		}

		~FControllerFixture()
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
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}
	};

	FPresentationResolutionEnvelope MakeAsyncProbeEnvelope(
		const FPresentationStateSnapshot& Snapshot,
		int64 ResolutionId)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Snapshot.BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Snapshot.StateRevision;
		Envelope.FinalSnapshot = Snapshot;

		FPresentationRecord Record;
		Record.BattleId = Snapshot.BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = 1;
		Record.Type = EBattlePresentationRecordType::BlockChanged;
		Record.BlockChanged.SourcePresentationId = NAME_None;
		Record.BlockChanged.TargetPresentationId = Snapshot.Player.PresentationId;
		Record.BlockChanged.Reason = EBlockPresentationReason::Gain;
		Record.BlockChanged.BlockBefore = Snapshot.Player.Block;
		Record.BlockChanged.BlockAfter = Snapshot.Player.Block;
		Record.BlockChanged.BlockDelta = 0;
		Envelope.Records.Add(Record);
		return Envelope;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8ADamageReducerTest,
	"SlayTheSpireDemo.SelectionPresentation.G8A.DamageReducer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8ADamageReducerTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG8ATest;

	FPresentationStateSnapshot Snapshot = MakeDamageBaseline();
	const FDamagePresentationPayload ValidDamage = MakeValidDamage();
	TestTrue(
		TEXT("Valid committed Damage applies through the single reducer"),
		PresentationDamageReducer::TryApplyDamageRecord(Snapshot, ValidDamage));
	TestEqual(TEXT("Reducer applies HPAfter"), Snapshot.Player.HP, 73);
	TestEqual(TEXT("Reducer applies BlockAfter"), Snapshot.Player.Block, 0);
	TestFalse(TEXT("Surviving target remains alive"), Snapshot.Player.bDead);

	FPresentationStateSnapshot FullyBlockedSnapshot = MakeDamageBaseline();
	FDamagePresentationPayload FullyBlocked = MakeValidDamage();
	FullyBlocked.IncomingDamage = 3;
	FullyBlocked.HPAfter = 80;
	FullyBlocked.BlockAfter = 2;
	FullyBlocked.BlockedDamage = 3;
	FullyBlocked.HPDamage = 0;
	TestTrue(
		TEXT("Fully blocked positive Damage remains a valid historical fact"),
		PresentationDamageReducer::TryApplyDamageRecord(FullyBlockedSnapshot, FullyBlocked));
	TestEqual(TEXT("Fully blocked Damage keeps HP"), FullyBlockedSnapshot.Player.HP, 80);
	TestEqual(TEXT("Fully blocked Damage consumes Block"), FullyBlockedSnapshot.Player.Block, 2);

	FPresentationStateSnapshot InvalidSnapshot = MakeDamageBaseline();
	FDamagePresentationPayload BadAccounting = MakeValidDamage();
	BadAccounting.HPDamage = 6;
	TestFalse(
		TEXT("Accounting mismatch is rejected"),
		PresentationDamageReducer::TryApplyDamageRecord(InvalidSnapshot, BadAccounting));
	TestEqual(TEXT("Failed reducer leaves HP unchanged"), InvalidSnapshot.Player.HP, 80);
	TestEqual(TEXT("Failed reducer leaves Block unchanged"), InvalidSnapshot.Player.Block, 5);
	TestFalse(TEXT("Failed reducer leaves death flag unchanged"), InvalidSnapshot.Player.bDead);

	FDamagePresentationPayload UnknownSource = MakeValidDamage();
	UnknownSource.SourcePresentationId = TEXT("Unknown");
	FPresentationStateSnapshot UnknownSourceSnapshot = MakeDamageBaseline();
	TestFalse(
		TEXT("Unknown non-None source is rejected"),
		PresentationDamageReducer::TryApplyDamageRecord(UnknownSourceSnapshot, UnknownSource));

	FDamagePresentationPayload NoOp = MakeValidDamage();
	NoOp.IncomingDamage = 0;
	FPresentationStateSnapshot NoOpSnapshot = MakeDamageBaseline();
	TestFalse(
		TEXT("Zero IncomingDamage is not a committed Damage transition"),
		PresentationDamageReducer::TryApplyDamageRecord(NoOpSnapshot, NoOp));

	FPresentationStateSnapshot DuplicateIdentitySnapshot = MakeDamageBaseline();
	DuplicateIdentitySnapshot.Enemy.PresentationId = DuplicateIdentitySnapshot.Player.PresentationId;
	TestFalse(
		TEXT("Target identity must resolve to exactly one combatant"),
		PresentationDamageReducer::TryApplyDamageRecord(DuplicateIdentitySnapshot, ValidDamage));

	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>();
	TestNotNull(TEXT("Controller reducer probe created"), Controller);
	if (Controller)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = 77;
		Envelope.ResolutionId = 1;
		Envelope.FinalStateRevision = 11;
		Envelope.FinalSnapshot = MakeDamageBaseline();
		Envelope.FinalSnapshot.StateRevision = 11;
		Envelope.FinalSnapshot.Player.HP = 73;
		Envelope.FinalSnapshot.Player.Block = 0;
		FPresentationRecord Record;
		Record.BattleId = 77;
		Record.ResolutionId = 1;
		Record.PresentationSequence = 1;
		Record.Type = EBattlePresentationRecordType::Damage;
		Record.Damage = ValidDamage;
		Envelope.Records.Add(Record);

		FPresentationStateSnapshot Reduced;
		TestTrue(
			TEXT("Blocking Controller reducer uses the same Damage contract"),
			Controller->ReduceEnvelopeForTesting(MakeDamageBaseline(), Envelope, Reduced));
		TestEqual(TEXT("Controller reducer HP matches shared reducer"), Reduced.Player.HP, 73);
		TestEqual(TEXT("Controller reducer Block matches shared reducer"), Reduced.Player.Block, 0);

		Envelope.Records[0].Damage.HPDamage = 6;
		TestFalse(
			TEXT("Controller reducer rejects the same invalid Damage accounting"),
			Controller->ReduceEnvelopeForTesting(MakeDamageBaseline(), Envelope, Reduced));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8ASessionContinuityTest,
	"SlayTheSpireDemo.SelectionPresentation.G8A.SessionContinuity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8ASessionContinuityTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG8ATest;
	FControllerFixture Fixture;
	if (!Fixture.IsReady())
	{
		AddError(TEXT("Failed to create G8-A Controller fixture."));
		return false;
	}

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	TestTrue(TEXT("Controller-owned ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true));
	TestTrue(TEXT("Controller initializes with a real Widget binding"), Controller->Initialize(Fixture.Battle, ViewModel, Widget));

	FPresentationSessionToken InitialSession;
	TestTrue(TEXT("PresentationOwned binding mints a session"), Controller->TryGetPresentationSessionToken(InitialSession));
	TestTrue(TEXT("Initial session is valid"), InitialSession.IsValid());

	Controller->SkipPresentation();
	FPresentationSessionToken AfterSkip;
	TestTrue(TEXT("Session survives ordinary Skip"), Controller->TryGetPresentationSessionToken(AfterSkip));
	TestTrue(TEXT("Ordinary Skip keeps exact SessionToken"), AfterSkip == InitialSession);

	FPresentationStateSnapshot Baseline;
	TestTrue(TEXT("Frozen baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline));
	const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 100;
	const FPresentationResolutionEnvelope Envelope = MakeAsyncProbeEnvelope(Baseline, ResolutionId);
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
	TestTrue(TEXT("Probe enters asynchronous playback"), Controller->IsWaitingForCompletionForTesting());
	Controller->ReconcileActiveEnvelopeToFinalSnapshotForTesting();

	FPresentationSessionToken AfterReconcile;
	TestTrue(TEXT("Session survives ordinary active-envelope reconcile"), Controller->TryGetPresentationSessionToken(AfterReconcile));
	TestTrue(TEXT("Reconcile keeps exact SessionToken"), AfterReconcile == InitialSession);

	Controller->Shutdown();
	TestFalse(TEXT("Shutdown makes old session stale"), Controller->IsCurrentPresentationSession(InitialSession));

	UBattlePresentationController* Replacement = NewObject<UBattlePresentationController>(Fixture.World);
	TestTrue(TEXT("Replacement Controller initializes"), Replacement->Initialize(Fixture.Battle, ViewModel, Widget));
	FPresentationSessionToken ReplacementSession;
	TestTrue(TEXT("Replacement Controller mints session"), Replacement->TryGetPresentationSessionToken(ReplacementSession));
	TestTrue(TEXT("Replacement session is valid"), ReplacementSession.IsValid());
	TestTrue(TEXT("Replacement ControllerEpoch differs"), ReplacementSession.ControllerEpoch != InitialSession.ControllerEpoch);
	TestFalse(TEXT("Old Controller token cannot ABA-match replacement"), Replacement->IsCurrentPresentationSession(InitialSession));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8AWidgetReplacementTest,
	"SlayTheSpireDemo.SelectionPresentation.G8A.WidgetReplacementOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8AWidgetReplacementTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG8ATest;
	FControllerFixture Fixture;
	if (!Fixture.IsReady())
	{
		AddError(TEXT("Failed to create G8-A replacement fixture."));
		return false;
	}

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	UPhase6UIA2APlaybackWidget* OldWidget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	UPhase6UIA2APlaybackWidget* NewWidget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	TestTrue(TEXT("ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true));
	TestTrue(TEXT("Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, OldWidget));

	FPresentationSessionToken OldSession;
	TestTrue(TEXT("Old Widget binding has session"), Controller->TryGetPresentationSessionToken(OldSession));

	FPresentationStateSnapshot Baseline;
	TestTrue(TEXT("Frozen baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline));
	const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 200;
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(MakeAsyncProbeEnvelope(Baseline, ResolutionId));
	TestTrue(TEXT("Old Widget owns active playback"), Controller->IsWaitingForCompletionForTesting());
	const FPresentationPlaybackToken OldPlaybackToken = Controller->GetActivePlaybackTokenForTesting();
	TestTrue(TEXT("Old playback token is valid"), OldPlaybackToken.IsValid());

	Controller->SetWidget(NewWidget);
	TestEqual(TEXT("Old Widget receives exactly one playback cancel"), OldWidget->CancelCallCount, 1);
	TestTrue(TEXT("Old Widget cancel receives old exact token"), OldWidget->LastCancelledToken == OldPlaybackToken);
	TestEqual(TEXT("New Widget never receives old playback token cancel"), NewWidget->CancelCallCount, 0);
	TestFalse(TEXT("Replacement catch-up retires old waiting owner"), Controller->IsWaitingForCompletionForTesting());
	TestFalse(TEXT("Old Session is stale after binding replacement"), Controller->IsCurrentPresentationSession(OldSession));

	FPresentationSessionToken NewSession;
	TestTrue(TEXT("New Widget binding has a new session"), Controller->TryGetPresentationSessionToken(NewSession));
	TestTrue(TEXT("New session is current"), Controller->IsCurrentPresentationSession(NewSession));
	TestEqual(TEXT("Same Controller keeps epoch across Widget replacement"), NewSession.ControllerEpoch, OldSession.ControllerEpoch);
	TestTrue(TEXT("Widget replacement advances session generation"), NewSession.PresentationSessionGeneration > OldSession.PresentationSessionGeneration);
	return true;
}

#endif
