#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2D4TimeoutTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		UPhase6UIA2APlaybackWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;
		FPresentationStateSnapshot Baseline;

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
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->bEnableCommittedPresentationRecording = true;
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();

			if (!Battle->TryGetLatestFrozenPresentationBaseline(Baseline))
			{
				return;
			}

			ViewModel = NewObject<UBattleHUDViewModel>(World);
			Widget = NewObject<UPhase6UIA2APlaybackWidget>(World);
			Controller = NewObject<UBattlePresentationController>(World);
			if (!IsValid(ViewModel) || !IsValid(Widget) || !IsValid(Controller))
			{
				return;
			}

			Widget->bAcceptAsyncPlayback = true;
			if (!ViewModel->Initialize(Battle, false))
			{
				return;
			}
			Widget->SetViewModel(ViewModel);
			if (!Controller->Initialize(Battle, ViewModel, Widget))
			{
				return;
			}
			Widget->SetPresentationController(Controller);
		}

		~FFixture()
		{
			if (IsValid(Controller))
			{
				Controller->Shutdown();
			}
			if (IsValid(ViewModel))
			{
				ViewModel->Shutdown();
			}
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
				&& IsValid(ViewModel)
				&& IsValid(Widget)
				&& IsValid(Controller)
				&& Baseline.BattleId > 0;
		}

		FPresentationResolutionEnvelope MakeFaultEnvelope(int64 ResolutionId, const TCHAR* Reason) const
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
			Fault.ResolutionFault.Reason = Reason;
			Fault.ResolutionFault.ExecutedActionCount = 0;
			Envelope.Records.Add(Fault);
			return Envelope;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D4TerminalTimeoutTest,
	"SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D4TerminalTimeoutTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D4TimeoutTest;

	// Sub-scenario 1: Controller timeout advances the frozen historical state and
	// the Widget is explicitly told, via its ViewModel boundary, to stop any stale
	// Blueprint visual that was still playing for the abandoned token.
	{
		FFixture Fixture;
		if (!TestTrue(TEXT("Timeout fixture created"), Fixture.IsReady()))
		{
			return false;
		}

		const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
		const FPresentationResolutionEnvelope Envelope = Fixture.MakeFaultEnvelope(ResolutionId, TEXT("A2D4 timeout fault"));
		const int32 CancelCountBeforePlayback = Fixture.Widget->CancelCallCount;

		Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
		TestEqual(TEXT("Terminal record reaches visible playback"), Fixture.Widget->PlayCallCount, 1);
		TestTrue(TEXT("Terminal record waits before timeout"), Fixture.Controller->IsWaitingForCompletionForTesting());
		TestEqual(TEXT("Displayed outcome remains non-terminal before timeout"), Fixture.ViewModel->Outcome, EBattleHUDOutcome::None);
		TestEqual(TEXT("Starting playback does not cancel itself"), Fixture.Widget->CancelCallCount, CancelCountBeforePlayback);

		Fixture.Controller->ExpireActivePlaybackForTesting();
		TestFalse(TEXT("Timeout clears wait"), Fixture.Controller->IsWaitingForCompletionForTesting());
		TestEqual(TEXT("Timeout commits terminal outcome"), Fixture.ViewModel->Outcome, EBattleHUDOutcome::ResolutionFaulted);
		TestEqual(TEXT("Timeout enters terminal interaction state"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Terminal);
		TestEqual(TEXT("Timeout completes terminal envelope"), Fixture.Controller->GetLastCompletedResolutionIdForTesting(), ResolutionId);
		TestFalse(TEXT("Timeout never faults Gameplay"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
		TestTrue(
			TEXT("Timeout-driven ViewModel advancement cancels stale Blueprint visual"),
			Fixture.Widget->CancelCallCount > CancelCountBeforePlayback
		);
	}

	// Sub-scenario 2: even a miswired Blueprint that calls Notify synchronously
	// from PlayPresentationRecord cannot re-enter Controller record progression.
	// The Widget base defers forwarding to the next CoreTicker turn.
	{
		FFixture Fixture;
		if (!TestTrue(TEXT("Synchronous-callback fixture created"), Fixture.IsReady()))
		{
			return false;
		}

		Fixture.Widget->bNotifySynchronouslyFromPlay = true;
		const int64 ResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
		const FPresentationResolutionEnvelope Envelope = Fixture.MakeFaultEnvelope(ResolutionId, TEXT("A2E deferred Blueprint completion"));
		const int32 CancelCountBeforePlayback = Fixture.Widget->CancelCallCount;

		Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
		TestEqual(TEXT("Synchronous-misuse Blueprint receives one record"), Fixture.Widget->PlayCallCount, 1);
		TestTrue(TEXT("Synchronous Notify cannot complete inside PlayPresentationRecord stack"), Fixture.Controller->IsWaitingForCompletionForTesting());
		TestTrue(TEXT("Terminal outcome remains historical before deferred callback"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);
		TestTrue(TEXT("Resolution watermark is not completed re-entrantly"), Fixture.Controller->GetLastCompletedResolutionIdForTesting() < ResolutionId);

		FTSTicker::GetCoreTicker().Tick(0.0f);

		TestFalse(TEXT("Deferred Blueprint completion clears wait"), Fixture.Controller->IsWaitingForCompletionForTesting());
		TestEqual(TEXT("Deferred Blueprint completion commits terminal outcome"), Fixture.ViewModel->Outcome, EBattleHUDOutcome::ResolutionFaulted);
		TestEqual(TEXT("Deferred Blueprint completion advances exactly one Resolution"), Fixture.Controller->GetLastCompletedResolutionIdForTesting(), ResolutionId);
		TestEqual(
			TEXT("Normal deferred completion does not issue fail-safe visual cancellation"),
			Fixture.Widget->CancelCallCount,
			CancelCountBeforePlayback
		);

		const int64 CompletedAfterDeferredCallback = Fixture.Controller->GetLastCompletedResolutionIdForTesting();
		Fixture.Controller->ExpireActivePlaybackForTesting();
		TestEqual(
			TEXT("No stale timeout remains after deferred completion"),
			Fixture.Controller->GetLastCompletedResolutionIdForTesting(),
			CompletedAfterDeferredCallback
		);
	}

	return true;
}

#endif
