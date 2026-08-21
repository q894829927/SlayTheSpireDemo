#include "BattleHUDPresenter.h"

#include "BattleHUDViewModel.h"
#include "BattleHUDWidgetBase.h"
#include "../Battle/BattleManager.h"
#include "../Presentation/BattlePresentationController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"

ABattleHUDPresenter::ABattleHUDPresenter()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABattleHUDPresenter::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(BattleManager))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter has no BattleManager reference."));
		return;
	}

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter has no WidgetClass."));
		return;
	}

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter could not find the local PlayerController."));
		return;
	}

	const bool bUsePresentationController =
		bEnableCommittedPresentation
		&& BattleManager->bEnableCommittedPresentationRecording
		&& BattleManager->IsPresentationAvailable();

	ViewModel = NewObject<UBattleHUDViewModel>(this);
	if (!IsValid(ViewModel) || !ViewModel->Initialize(BattleManager.Get(), bUsePresentationController))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter failed to initialize the BattleHUD ViewModel."));
		return;
	}

	// Widget creation is intentionally independent from Presentation availability.
	// PresentationUnavailable is a HUD-visible development error state rather than
	// a reason to suppress the normal error-capable HUD surface.
	WidgetInstance = CreateWidget<UBattleHUDWidgetBase>(PlayerController, WidgetClass);
	if (!IsValid(WidgetInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter failed to create the Battle HUD Widget."));
		return;
	}

	WidgetInstance->SetViewModel(ViewModel.Get());

	if (bUsePresentationController)
	{
		PresentationController = NewObject<UBattlePresentationController>(this);
		if (IsValid(PresentationController)
			&& PresentationController->Initialize(BattleManager.Get(), ViewModel.Get(), WidgetInstance.Get()))
		{
			WidgetInstance->SetPresentationController(PresentationController.Get());
		}
		else
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[BattleHUD] PresentationController initialization failed. Falling back to frozen latest-state HUD delivery.")
			);
			PresentationController = nullptr;
			ViewModel->SetPresentationDisplayOwned(false);
			ViewModel->RefreshLiveInputBindingsIfCaughtUp();
		}
	}
	else if (bEnableCommittedPresentation && !BattleManager->IsPresentationAvailable())
	{
		ViewModel->EnterPresentationUnavailable(BattleManager->GetPresentationUnavailableReason());
	}
	// Intentional no-history modes (Presenter disabled or battle recording disabled)
	// leave the ViewModel as the direct frozen-baseline owner. OnReadStateReady may
	// then apply the newest frozen baseline without any PresentationController.

	WidgetInstance->AddToViewport(ZOrder);

	if (bConfigureGameAndUIInput)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}
}

void ABattleHUDPresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(WidgetInstance))
	{
		WidgetInstance->SetPresentationController(nullptr);
	}

	if (IsValid(PresentationController))
	{
		PresentationController->Shutdown();
		PresentationController = nullptr;
	}

	if (IsValid(WidgetInstance))
	{
		WidgetInstance->RemoveFromParent();
		WidgetInstance = nullptr;
	}

	if (IsValid(ViewModel))
	{
		ViewModel->Shutdown();
		ViewModel = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}