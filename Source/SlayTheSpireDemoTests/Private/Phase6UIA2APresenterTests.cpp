#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/BattleHUDViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2APresentationUnavailableHUDTest,
	"SlayTheSpireDemo.Phase6UIA2A.Infrastructure.PresentationUnavailableStillCreatesErrorCapableHUD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2APresentationUnavailableHUDTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("Test world created"), World)) return false;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatant* Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
	ACombatant* Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
	ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
	APlayerController* PlayerController = World->SpawnActor<APlayerController>(APlayerController::StaticClass(), FTransform::Identity, SpawnParameters);
	APhase6UIA2ATestPresenter* Presenter = World->SpawnActor<APhase6UIA2ATestPresenter>(APhase6UIA2ATestPresenter::StaticClass(), FTransform::Identity, SpawnParameters);

	const bool bActorsValid = IsValid(Player) && IsValid(Enemy) && IsValid(Battle)
		&& IsValid(PlayerController) && IsValid(Presenter);
	if (!TestTrue(TEXT("HUD assembly actors created"), bActorsValid))
	{
		World->DestroyWorld(false);
		return false;
	}

	Player->PresentationId = TEXT("DuplicatePresentationId");
	Enemy->PresentationId = TEXT("DuplicatePresentationId");
	Battle->Player = Player;
	Battle->Enemy = Enemy;
	Battle->OpeningHandDrawCount = 0;
	Battle->PlayerTurnDrawCount = 0;
	// Duplicate resolved IDs intentionally exercise the PresentationUnavailable
	// bootstrap path. Reset validation reports the configuration failure once and
	// the later frozen-snapshot attempt reports the unavailable freeze once more.
	AddExpectedErrorPlain(
		TEXT("[Presentation] Unavailable for BattleId="),
		EAutomationExpectedErrorFlags::Contains,
		2
	);
	Battle->StartBattle();

	TestFalse(TEXT("Duplicate resolved PresentationId disables presentation only"), Battle->IsPresentationAvailable());
	TestEqual(TEXT("Headless Gameplay remains request-eligible"), Battle->BattleState, EBattleState::PlayerTurn);

	Presenter->BattleManager = Battle;
	Presenter->WidgetClass = UPhase6UIA2APlaybackWidget::StaticClass();
	Presenter->bConfigureGameAndUIInput = false;
	Presenter->bEnableCommittedPresentation = true;
	Presenter->InvokeBeginPlayForTesting();

	TestTrue(TEXT("Presenter still creates the normal HUD Widget"), IsValid(Presenter->WidgetInstance));
	TestTrue(TEXT("Presenter still creates a ViewModel"), IsValid(Presenter->ViewModel));
	if (IsValid(Presenter->ViewModel))
	{
		TestEqual(
			TEXT("HUD exposes PresentationUnavailable state"),
			Presenter->ViewModel->InteractionState,
			EBattleHUDInteractionState::PresentationUnavailable
		);
		TestTrue(TEXT("PresentationUnavailable locks input"), Presenter->ViewModel->bInputLocked);
		TestTrue(TEXT("HUD exposes a development-facing error"), !Presenter->ViewModel->LastFeedback.IsEmpty());
	}

	World->DestroyWorld(false);
	return true;
}

#endif
