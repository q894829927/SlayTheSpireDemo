#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8AInvalidDamagePreflightTest,
	"SlayTheSpireDemo.SelectionPresentation.G8A.InvalidDamagePreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8AInvalidDamagePreflightTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("G8-A preflight World created"), World))
	{
		return false;
	}

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

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Frozen baseline exists"), Battle->TryGetLatestFrozenPresentationBaseline(Baseline)))
	{
		World->DestroyWorld(false);
		return false;
	}

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(World);
	UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(World);
	TestTrue(TEXT("Presentation-owned ViewModel initializes"), ViewModel->Initialize(Battle, true));
	TestTrue(TEXT("Controller initializes"), Controller->Initialize(Battle, ViewModel, Widget));

	FPresentationSessionToken SessionBefore;
	TestTrue(TEXT("Session exists before invalid historical record"), Controller->TryGetPresentationSessionToken(SessionBefore));

	const int64 ResolutionId = static_cast<int64>(Battle->GetLatestFrozenPresentationBaselineResolutionId()) + 100;
	FPresentationResolutionEnvelope Envelope;
	Envelope.BattleId = Baseline.BattleId;
	Envelope.ResolutionId = ResolutionId;
	Envelope.Origin = EPresentationResolutionOrigin::System;
	Envelope.FinalStateRevision = Baseline.StateRevision;
	Envelope.FinalSnapshot = Baseline;

	FPresentationRecord Record;
	Record.BattleId = Baseline.BattleId;
	Record.ResolutionId = ResolutionId;
	Record.PresentationSequence = 1;
	Record.Type = EBattlePresentationRecordType::Damage;
	Record.Damage.SourcePresentationId = Baseline.Enemy.PresentationId;
	Record.Damage.TargetPresentationId = Baseline.Player.PresentationId;
	Record.Damage.DamageKind = EDamageKind::Attack;
	Record.Damage.IncomingDamage = 10;
	Record.Damage.HPBefore = Baseline.Player.HP;
	Record.Damage.HPAfter = FMath::Max(0, Baseline.Player.HP - 5);
	Record.Damage.BlockBefore = Baseline.Player.Block;
	Record.Damage.BlockAfter = Baseline.Player.Block;
	Record.Damage.BlockedDamage = 0;
	Record.Damage.HPDamage = 4; // Deliberately inconsistent with HPBefore/HPAfter.
	Envelope.Records.Add(Record);

	Battle->OnPresentationResolutionReady.Broadcast(Envelope);

	TestEqual(
		TEXT("Invalid historical Damage is rejected before Widget playback"),
		Widget->PlayCallCount,
		0);
	TestFalse(
		TEXT("Invalid Damage recovers synchronously instead of waiting for visual completion"),
		Controller->IsWaitingForCompletionForTesting());
	TestEqual(
		TEXT("Recovery applies the sealed FinalSnapshot revision"),
		ViewModel->StateRevision,
		Baseline.StateRevision);

	FPresentationSessionToken SessionAfter;
	TestTrue(TEXT("Ordinary historical reconcile retains session"), Controller->TryGetPresentationSessionToken(SessionAfter));
	TestTrue(TEXT("Invalid-record reconcile keeps the same authority token"), SessionAfter == SessionBefore);

	Controller->Shutdown();
	ViewModel->Shutdown();
	World->DestroyWorld(false);
	return true;
}

#endif
