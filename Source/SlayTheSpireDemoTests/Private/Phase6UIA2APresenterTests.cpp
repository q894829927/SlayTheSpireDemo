#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
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

	// CreateWidget(APlayerController, ...) requires an attached ULocalPlayer.
	// The transient test world does not run the normal GameMode login path, so
	// provide only the ownership identity needed by UMG without faking BeginPlay.
	if (!TestNotNull(TEXT("Engine exists for ULocalPlayer ownership"), GEngine))
	{
		World->DestroyWorld(false);
		return false;
	}
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	if (!TestNotNull(TEXT("Local player created for HUD ownership"), LocalPlayer))
	{
		World->DestroyWorld(false);
		return false;
	}
	PlayerController->Player = LocalPlayer;
	LocalPlayer->PlayerController = PlayerController;

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
	TestTrue(
		TEXT("Presenter HUD assembly succeeds without manual BeginPlay dispatch"),
		Presenter->InvokeInitializeHUDForTesting(PlayerController)
	);

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

	// This fixture does not enter the normal Actor BeginPlay/EndPlay lifecycle.
	// Explicitly tear down HUD objects and sever the temporary Engine-owned
	// LocalPlayer <-> world PlayerController links before destroying the World so
	// no subsequent Automation case can inherit a stale world reference.
	Presenter->InvokeShutdownHUDForTesting();
	TestNull(TEXT("Explicit test cleanup releases Presenter Widget"), Presenter->WidgetInstance.Get());
	TestNull(TEXT("Explicit test cleanup releases Presenter ViewModel"), Presenter->ViewModel.Get());
	TestNull(TEXT("Explicit test cleanup releases Presenter Controller"), Presenter->PresentationController.Get());

	LocalPlayer->PlayerController = nullptr;
	PlayerController->Player = nullptr;
	TestNull(TEXT("Temporary LocalPlayer no longer references test PlayerController"), LocalPlayer->PlayerController);
	TestNull(TEXT("Test PlayerController no longer references temporary LocalPlayer"), PlayerController->Player);

	World->DestroyWorld(false);
	return true;
}

#endif
