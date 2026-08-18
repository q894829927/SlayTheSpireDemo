#include "BattleHUDPresenter.h"

#include "BattleHUDViewModel.h"
#include "BattleHUDWidgetBase.h"
#include "../Battle/BattleManager.h"
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

	ViewModel = NewObject<UBattleHUDViewModel>(this);
	if (!IsValid(ViewModel) || !ViewModel->Initialize(BattleManager.Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter failed to initialize the BattleHUD ViewModel."));
		return;
	}

	WidgetInstance = CreateWidget<UBattleHUDWidgetBase>(PlayerController, WidgetClass);
	if (!IsValid(WidgetInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleHUD] Presenter failed to create the Battle HUD Widget."));
		return;
	}

	WidgetInstance->SetViewModel(ViewModel.Get());
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
