#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D4TerminalTimeoutTest,
	"SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalTimeout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D4TerminalTimeoutTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("World created"), World)) return false;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatant* Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
	ACombatant* Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
	ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!TestNotNull(TEXT("Player created"), Player)
		|| !TestNotNull(TEXT("Enemy created"), Enemy)
		|| !TestNotNull(TEXT("Battle created"), Battle))
	{
		World->DestroyWorld(false);
		return false;
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

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Battle->TryGetLatestFrozenPresentationBaseline(Baseline)))
	{
		World->DestroyWorld(false);
		return false;
	}

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(World);
	UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(World);
	if (!TestNotNull(TEXT("ViewModel created"), ViewModel)
		|| !TestNotNull(TEXT("Widget created"), Widget)
		|| !TestNotNull(TEXT("Controller created"), Controller))
	{
		World->DestroyWorld(false);
		return false;
	}

	Widget->bAcceptAsyncPlayback = true;
	TestTrue(TEXT("ViewModel initializes"), ViewModel->Initialize(Battle, false));
	TestTrue(TEXT("Controller initializes"), Controller->Initialize(Battle, ViewModel, Widget));

	const int64 ResolutionId = static_cast<int64>(Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 1;
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
	Fault.ResolutionFault.Reason = TEXT("A2D4 timeout fault");
	Fault.ResolutionFault.ExecutedActionCount = 0;
	Envelope.Records.Add(Fault);

	Battle->OnPresentationResolutionReady.Broadcast(Envelope);
	TestEqual(TEXT("Terminal record reaches visible playback"), Widget->PlayCallCount, 1);
	TestTrue(TEXT("Terminal record waits before timeout"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Displayed outcome remains non-terminal before timeout"), ViewModel->Outcome, EBattleHUDOutcome::None);

	Controller->ExpireActivePlaybackForTesting();
	TestFalse(TEXT("Timeout clears wait"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Timeout commits terminal outcome"), ViewModel->Outcome, EBattleHUDOutcome::ResolutionFaulted);
	TestEqual(TEXT("Timeout enters terminal interaction state"), ViewModel->InteractionState, EBattleHUDInteractionState::Terminal);
	TestEqual(TEXT("Timeout completes terminal envelope"), Controller->GetLastCompletedResolutionIdForTesting(), ResolutionId);
	TestFalse(TEXT("Timeout never faults Gameplay"), Battle->GetActionQueueForTesting()->IsResolutionFaulted());

	Controller->Shutdown();
	ViewModel->Shutdown();
	World->DestroyWorld(false);
	return true;
}

#endif
