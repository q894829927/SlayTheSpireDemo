#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2D4Test
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
			Battle->bEnableCommittedPresentationRecording = true;
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
		Test.AddError(TEXT("Failed to create Phase 6UI-A2D4 fixture."));
		return false;
	}

	FPresentationResolutionEnvelope MakeVictoryEnvelope(
		const FPresentationStateSnapshot& Baseline,
		int64 ResolutionId,
		bool bIncludeDamage,
		bool bCorruptIdentity = false
	)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Baseline.BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Baseline.StateRevision;
		Envelope.FinalSnapshot = Baseline;
		Envelope.FinalSnapshot.BattleState = EBattleState::Victory;
		Envelope.FinalSnapshot.Outcome = EBattleHUDOutcome::Victory;
		Envelope.FinalSnapshot.bCanEndTurn = false;
		Envelope.FinalSnapshot.Enemy.HP = 0;
		Envelope.FinalSnapshot.Enemy.bDead = true;

		int64 Sequence = 1;
		if (bIncludeDamage)
		{
			FPresentationRecord Damage;
			Damage.BattleId = Baseline.BattleId;
			Damage.ResolutionId = ResolutionId;
			Damage.PresentationSequence = Sequence++;
			Damage.Type = EBattlePresentationRecordType::Damage;
			Damage.Damage.SourcePresentationId = Baseline.Player.PresentationId;
			Damage.Damage.TargetPresentationId = Baseline.Enemy.PresentationId;
			Damage.Damage.HPBefore = Baseline.Enemy.HP;
			Damage.Damage.HPAfter = 0;
			Damage.Damage.BlockBefore = Baseline.Enemy.Block;
			Damage.Damage.BlockAfter = Baseline.Enemy.Block;
			Damage.Damage.HPDamage = FMath::Max(0, Baseline.Enemy.HP);
			Envelope.Records.Add(Damage);
		}

		FPresentationRecord Victory;
		Victory.BattleId = Baseline.BattleId;
		Victory.ResolutionId = ResolutionId;
		Victory.PresentationSequence = Sequence;
		Victory.Type = EBattlePresentationRecordType::Victory;
		Victory.Terminal.WinnerPresentationId = bCorruptIdentity
			? FName(TEXT("FakeWinner"))
			: Baseline.Player.PresentationId;
		Victory.Terminal.DefeatedPresentationId = Baseline.Enemy.PresentationId;
		Envelope.Records.Add(Victory);
		return Envelope;
	}

	FPresentationResolutionEnvelope MakeFaultEnvelope(
		const FPresentationStateSnapshot& Baseline,
		int64 ResolutionId
	)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Baseline.BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Baseline.StateRevision;
		Envelope.FinalSnapshot = Baseline;
		Envelope.FinalSnapshot.BattleState = EBattleState::ResolutionFaulted;
		Envelope.FinalSnapshot.Outcome = EBattleHUDOutcome::ResolutionFaulted;
		Envelope.FinalSnapshot.bCanEndTurn = false;

		FPresentationRecord Fault;
		Fault.BattleId = Baseline.BattleId;
		Fault.ResolutionId = ResolutionId;
		Fault.PresentationSequence = 1;
		Fault.Type = EBattlePresentationRecordType::ResolutionFault;
		Fault.ResolutionFault.Reason = TEXT("A2D4 synthetic fault");
		Fault.ResolutionFault.ExecutedActionCount = 0;
		Fault.ResolutionFault.LastActionName = NAME_None;
		Envelope.Records.Add(Fault);
		return Envelope;
	}

	bool InitializeController(
		FAutomationTestBase& Test,
		FFixture& Fixture,
		UBattleHUDViewModel*& OutViewModel,
		UPhase6UIA2APlaybackWidget*& OutWidget,
		UBattlePresentationController*& OutController,
		bool bAcceptAsync = true
	)
	{
		OutViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
		OutWidget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
		OutController = NewObject<UBattlePresentationController>(Fixture.World);
		if (!Test.TestNotNull(TEXT("ViewModel created"), OutViewModel)
			|| !Test.TestNotNull(TEXT("Playback widget created"), OutWidget)
			|| !Test.TestNotNull(TEXT("Controller created"), OutController))
		{
			return false;
		}
		OutWidget->bAcceptAsyncPlayback = bAcceptAsync;
		if (!Test.TestTrue(TEXT("ViewModel initializes"), OutViewModel->Initialize(Fixture.Battle, false)))
		{
			return false;
		}
		return Test.TestTrue(TEXT("Controller initializes"), OutController->Initialize(Fixture.Battle, OutViewModel, OutWidget));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D4VictoryProducerTest,
	"SlayTheSpireDemo.Phase6UIA2D4.Producer.VictoryPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D4VictoryProducerTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D4Test;
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;
	Fixture.Flush();

	TArray<FPresentationResolutionEnvelope> Deliveries;
	Fixture.Battle->OnPresentationResolutionReady.AddLambda(
		[&Deliveries](const FPresentationResolutionEnvelope& Envelope)
		{
			Deliveries.Add(Envelope);
		}
	);

	TestTrue(TEXT("Victory Resolution begins"), Fixture.Battle->BeginSystemPresentationResolutionForTesting());
	Fixture.Enemy->HP = 0;
	Fixture.Battle->CheckBattleResultForTesting();
	TestEqual(TEXT("Gameplay commits Victory"), Fixture.Battle->BattleState, EBattleState::Victory);
	TestTrue(TEXT("Victory Resolution seals"), Fixture.Battle->SealActivePresentationResolutionForTesting());
	Fixture.Flush();
	TestEqual(TEXT("Exactly one Victory envelope delivered"), Deliveries.Num(), 1);
	if (Deliveries.Num() != 1) return false;

	const FPresentationResolutionEnvelope& Envelope = Deliveries[0];
	TestEqual(TEXT("Victory has one terminal record"), Envelope.Records.Num(), 1);
	if (Envelope.Records.Num() != 1) return false;
	const FPresentationRecord& Record = Envelope.Records[0];
	TestEqual(TEXT("Victory record type"), Record.Type, EBattlePresentationRecordType::Victory);
	TestEqual(TEXT("Victory winner is Player"), Record.Terminal.WinnerPresentationId, Envelope.FinalSnapshot.Player.PresentationId);
	TestEqual(TEXT("Victory defeated is Enemy"), Record.Terminal.DefeatedPresentationId, Envelope.FinalSnapshot.Enemy.PresentationId);
	TestEqual(TEXT("Victory final state"), Envelope.FinalSnapshot.BattleState, EBattleState::Victory);
	TestEqual(TEXT("Victory final outcome"), Envelope.FinalSnapshot.Outcome, EBattleHUDOutcome::Victory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D4DefeatProducerTest,
	"SlayTheSpireDemo.Phase6UIA2D4.Producer.DefeatPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D4DefeatProducerTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D4Test;
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;
	Fixture.Flush();

	TArray<FPresentationResolutionEnvelope> Deliveries;
	Fixture.Battle->OnPresentationResolutionReady.AddLambda(
		[&Deliveries](const FPresentationResolutionEnvelope& Envelope)
		{
			Deliveries.Add(Envelope);
		}
	);

	TestTrue(TEXT("Defeat Resolution begins"), Fixture.Battle->BeginSystemPresentationResolutionForTesting());
	Fixture.Player->HP = 0;
	Fixture.Battle->CheckBattleResultForTesting();
	TestEqual(TEXT("Gameplay commits Defeat"), Fixture.Battle->BattleState, EBattleState::Defeat);
	TestTrue(TEXT("Defeat Resolution seals"), Fixture.Battle->SealActivePresentationResolutionForTesting());
	Fixture.Flush();
	TestEqual(TEXT("Exactly one Defeat envelope delivered"), Deliveries.Num(), 1);
	if (Deliveries.Num() != 1) return false;

	const FPresentationResolutionEnvelope& Envelope = Deliveries[0];
	TestEqual(TEXT("Defeat has one terminal record"), Envelope.Records.Num(), 1);
	if (Envelope.Records.Num() != 1) return false;
	const FPresentationRecord& Record = Envelope.Records[0];
	TestEqual(TEXT("Defeat record type"), Record.Type, EBattlePresentationRecordType::Defeat);
	TestEqual(TEXT("Defeat winner is Enemy"), Record.Terminal.WinnerPresentationId, Envelope.FinalSnapshot.Enemy.PresentationId);
	TestEqual(TEXT("Defeat defeated is Player"), Record.Terminal.DefeatedPresentationId, Envelope.FinalSnapshot.Player.PresentationId);
	TestEqual(TEXT("Defeat final state"), Envelope.FinalSnapshot.BattleState, EBattleState::Defeat);
	TestEqual(TEXT("Defeat final outcome"), Envelope.FinalSnapshot.Outcome, EBattleHUDOutcome::Defeat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D4FaultProducerTest,
	"SlayTheSpireDemo.Phase6UIA2D4.Producer.ResolutionFaultPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D4FaultProducerTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D4Test;
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;
	Fixture.Flush();

	TArray<FPresentationResolutionEnvelope> Deliveries;
	Fixture.Battle->OnPresentationResolutionReady.AddLambda(
		[&Deliveries](const FPresentationResolutionEnvelope& Envelope)
		{
			Deliveries.Add(Envelope);
		}
	);

	TestTrue(TEXT("Fault Resolution begins"), Fixture.Battle->BeginSystemPresentationResolutionForTesting());
	AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution fault requested:"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("[Battle] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("Framework fault request accepted"), Fixture.Battle->GetActionQueueForTesting()->RequestResolutionFault(TEXT("A2D4 typed fault")));
	Fixture.Flush();

	TestEqual(TEXT("Gameplay enters ResolutionFaulted"), Fixture.Battle->BattleState, EBattleState::ResolutionFaulted);
	TestEqual(TEXT("Exactly one fault envelope delivered"), Deliveries.Num(), 1);
	if (Deliveries.Num() != 1) return false;
	const FPresentationResolutionEnvelope& Envelope = Deliveries[0];
	TestTrue(TEXT("Fault envelope has records"), Envelope.Records.Num() > 0);
	if (Envelope.Records.Num() == 0) return false;
	const FPresentationRecord& Record = Envelope.Records.Last();
	TestEqual(TEXT("ResolutionFault is final record"), Record.Type, EBattlePresentationRecordType::ResolutionFault);
	TestEqual(TEXT("Typed fault reason freezes"), Record.ResolutionFault.Reason, FString(TEXT("A2D4 typed fault")));
	TestTrue(TEXT("Typed fault executed count is non-negative"), Record.ResolutionFault.ExecutedActionCount >= 0);
	TestEqual(TEXT("Fault final state"), Envelope.FinalSnapshot.BattleState, EBattleState::ResolutionFaulted);
	TestEqual(TEXT("Fault final outcome"), Envelope.FinalSnapshot.Outcome, EBattleHUDOutcome::ResolutionFaulted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D4TerminalCompletionTimingTest,
	"SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalCompletionTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D4TerminalCompletionTimingTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D4Test;
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;
	Fixture.Flush();

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
	UBattleHUDViewModel* ViewModel = nullptr;
	UPhase6UIA2APlaybackWidget* Widget = nullptr;
	UBattlePresentationController* Controller = nullptr;
	if (!InitializeController(*this, Fixture, ViewModel, Widget, Controller, true)) return false;

	const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
	const FPresentationResolutionEnvelope Envelope = MakeVictoryEnvelope(Baseline, ResolutionId, true);
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
	TestEqual(TEXT("Damage is first visible record"), Widget->PlayCallCount, 1);
	TestTrue(TEXT("Damage waits for callback"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Outcome remains non-terminal during damage playback"), ViewModel->Outcome, EBattleHUDOutcome::None);

	const FPresentationPlaybackToken DamageToken = Controller->GetActivePlaybackTokenForTesting();
	Controller->NotifyPresentationFinished(DamageToken);
	TestEqual(TEXT("Terminal record starts after damage completes"), Widget->PlayCallCount, 2);
	TestTrue(TEXT("Terminal record waits for callback"), Controller->IsWaitingForCompletionForTesting());
	TestTrue(TEXT("Damage reducer already shows defeated Enemy"), ViewModel->Enemy.bDead);
	TestEqual(TEXT("Working display is still non-terminal while Victory animates"), ViewModel->Outcome, EBattleHUDOutcome::None);

	const FPresentationPlaybackToken VictoryToken = Controller->GetActivePlaybackTokenForTesting();
	Controller->NotifyPresentationFinished(VictoryToken);
	TestFalse(TEXT("Victory completion clears wait"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Victory commits only after terminal completion"), ViewModel->Outcome, EBattleHUDOutcome::Victory);
	TestEqual(TEXT("ViewModel enters Terminal after completion"), ViewModel->InteractionState, EBattleHUDInteractionState::Terminal);
	TestEqual(TEXT("Terminal envelope completes"), Controller->GetLastCompletedResolutionIdForTesting(), ResolutionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D4TerminalSafetyTest,
	"SlayTheSpireDemo.Phase6UIA2D4.Safety.PreflightFallbackSkip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D4TerminalSafetyTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D4Test;

	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		Fixture.Flush();
		FPresentationStateSnapshot Baseline;
		if (!TestTrue(TEXT("Mismatch baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
		UBattleHUDViewModel* ViewModel = nullptr;
		UPhase6UIA2APlaybackWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;
		if (!InitializeController(*this, Fixture, ViewModel, Widget, Controller, true)) return false;
		const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
		const FPresentationResolutionEnvelope Envelope = MakeVictoryEnvelope(Baseline, ResolutionId, false, true);
		Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
		TestEqual(TEXT("Invalid terminal never reaches Blueprint"), Widget->PlayCallCount, 0);
		TestFalse(TEXT("Invalid terminal does not wait"), Controller->IsWaitingForCompletionForTesting());
		TestEqual(TEXT("Invalid terminal collapses to authoritative FinalSnapshot"), ViewModel->Outcome, EBattleHUDOutcome::Victory);
		TestEqual(TEXT("Collapsed terminal envelope completes"), Controller->GetLastCompletedResolutionIdForTesting(), ResolutionId);
	}

	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		Fixture.Flush();
		FPresentationStateSnapshot Baseline;
		if (!TestTrue(TEXT("Fallback baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
		UBattleHUDViewModel* ViewModel = nullptr;
		UPhase6UIA2APlaybackWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;
		if (!InitializeController(*this, Fixture, ViewModel, Widget, Controller, false)) return false;
		const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
		const FPresentationResolutionEnvelope Envelope = MakeFaultEnvelope(Baseline, ResolutionId);
		Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
		TestEqual(TEXT("Native false fallback still offers terminal once"), Widget->PlayCallCount, 1);
		TestFalse(TEXT("False fallback completes immediately"), Controller->IsWaitingForCompletionForTesting());
		TestEqual(TEXT("False fallback commits fault terminal state"), ViewModel->Outcome, EBattleHUDOutcome::ResolutionFaulted);
		TestEqual(TEXT("False fallback completes envelope"), Controller->GetLastCompletedResolutionIdForTesting(), ResolutionId);
	}

	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		Fixture.Flush();
		FPresentationStateSnapshot Baseline;
		if (!TestTrue(TEXT("Skip baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
		UBattleHUDViewModel* ViewModel = nullptr;
		UPhase6UIA2APlaybackWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;
		if (!InitializeController(*this, Fixture, ViewModel, Widget, Controller, true)) return false;
		const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
		const FPresentationResolutionEnvelope Envelope = MakeFaultEnvelope(Baseline, ResolutionId);
		Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
		TestTrue(TEXT("Async fault is waiting before skip"), Controller->IsWaitingForCompletionForTesting());
		TestEqual(TEXT("Fault is not terminal in display before completion"), ViewModel->Outcome, EBattleHUDOutcome::None);
		const FPresentationPlaybackToken StaleToken = Controller->GetActivePlaybackTokenForTesting();
		Controller->SkipPresentation();
		TestFalse(TEXT("Skip clears terminal wait"), Controller->IsWaitingForCompletionForTesting());
		TestEqual(TEXT("Skip applies terminal FinalSnapshot immediately"), ViewModel->Outcome, EBattleHUDOutcome::ResolutionFaulted);
		TestEqual(TEXT("Skip completes terminal envelope"), Controller->GetLastCompletedResolutionIdForTesting(), ResolutionId);
		Controller->NotifyPresentationFinished(StaleToken);
		TestEqual(TEXT("Stale callback after skip cannot change terminal result"), ViewModel->Outcome, EBattleHUDOutcome::ResolutionFaulted);
		TestEqual(TEXT("Stale callback cannot advance completion again"), Controller->GetLastCompletedResolutionIdForTesting(), ResolutionId);
	}

	return true;
}

#endif
