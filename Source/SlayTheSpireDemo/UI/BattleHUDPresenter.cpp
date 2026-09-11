#include "BattleHUDPresenter.h"

#include "BattleHUDViewModel.h"
#include "BattleHUDWidgetBase.h"
#include "../Battle/BattleManager.h"
#include "../Presentation/BattlePresentationController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "../MapToggle/MapToggle.h"

ABattleHUDPresenter::ABattleHUDPresenter()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABattleHUDPresenter::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	InitializeHUD(PlayerController);
}

bool ABattleHUDPresenter::InitializeHUD(APlayerController* PlayerController)
{
	if (!IsValid(BattleManager))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter has no BattleManager reference."));
		return false;
	}

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter has no WidgetClass."));
		return false;
	}

	if (!IsValid(PlayerController))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter could not find the local PlayerController."));
		return false;
	}

	const bool bUsePresentationController =
		bEnableCommittedPresentation
		&& BattleManager->IsCommittedPresentationRecordingEnabledForBattle()
		&& BattleManager->IsPresentationAvailable();

	ViewModel = NewObject<UBattleHUDViewModel>(this);
	if (!IsValid(ViewModel) || !ViewModel->Initialize(BattleManager.Get(), bUsePresentationController))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter failed to initialize the BattleHUD ViewModel."));
		return false;
	}

	// Widget creation is intentionally independent from Presentation availability.
	// PresentationUnavailable is a HUD-visible development error state rather than
	// a reason to suppress the normal error-capable HUD surface.
	WidgetInstance = CreateWidget<UBattleHUDWidgetBase>(PlayerController, WidgetClass);
	if (!IsValid(WidgetInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter failed to create the Battle HUD Widget."));
		return false;
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
	BindGlobalRightMouseCancel(PlayerController);

	if (bConfigureGameAndUIInput)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}

	return true;
}

void ABattleHUDPresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownHUD();
	Super::EndPlay(EndPlayReason);
}

void ABattleHUDPresenter::ShutdownHUD()
{
	UnbindGlobalRightMouseCancel();

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
}

void ABattleHUDPresenter::BindGlobalRightMouseCancel(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (GlobalRightMousePlayerController.Get() == PlayerController
		&& IsValid(GlobalRightMouseInputComponent))
	{
		return;
	}

	UnbindGlobalRightMouseCancel();

	GlobalRightMouseInputComponent = NewObject<UInputComponent>(
		this,
		TEXT("BattleHUDGlobalRightMouseInput"),
		RF_Transient);
	if (!IsValid(GlobalRightMouseInputComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Failed to create the global right-click input component."));
		return;
	}

	GlobalRightMouseInputComponent->Priority = 1000;
	GlobalRightMouseInputComponent->bBlockInput = false;
	FInputKeyBinding& Binding = GlobalRightMouseInputComponent->BindKey(
		EKeys::RightMouseButton,
		IE_Pressed,
		this,
		&ABattleHUDPresenter::HandleGlobalRightMouseButtonPressed);
	// The binding must coexist with world/UI input when no selection is active.
	// The handler itself only mutates the HUD when the shared cancel boundary
	// reports an active cancellable selection.
	Binding.bConsumeInput = false;

	FInputKeyBinding& MapToggleBinding = GlobalRightMouseInputComponent->BindKey(
		EKeys::M,
		IE_Pressed,
		this,
		&ABattleHUDPresenter::HandleMapTogglePressed);
	MapToggleBinding.bConsumeInput = true;

	GlobalRightMouseInputComponent->BindKey(
		EKeys::M,
		IE_Released,
		this,
		&ABattleHUDPresenter::HandleMapToggleReleased);

	GlobalRightMousePlayerController = PlayerController;
	PlayerController->PushInputComponent(GlobalRightMouseInputComponent.Get());
}

void ABattleHUDPresenter::UnbindGlobalRightMouseCancel()
{
	if (APlayerController* PlayerController = GlobalRightMousePlayerController.Get())
	{
		if (IsValid(GlobalRightMouseInputComponent))
		{
			PlayerController->PopInputComponent(GlobalRightMouseInputComponent.Get());
		}
	}

	GlobalRightMousePlayerController.Reset();
	GlobalRightMouseInputComponent = nullptr;
}

void ABattleHUDPresenter::HandleGlobalRightMouseButtonPressed()
{
	if (IsValid(WidgetInstance))
	{
		WidgetInstance->HandleRightMouseButtonCancelInput();
	}
}

void ABattleHUDPresenter::HandleMapTogglePressed()
{
	MapToggle::Toggle(GetWorld());
}

void ABattleHUDPresenter::HandleMapToggleReleased()
{
	MapToggle::HandleReleased(GetWorld());
}
