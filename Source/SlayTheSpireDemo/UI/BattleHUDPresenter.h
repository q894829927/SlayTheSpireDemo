#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleHUDPresenter.generated.h"

class ABattleManager;
class APlayerController;
class UBattleHUDViewModel;
class UBattleHUDWidgetBase;
class UBattlePresentationController;

UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API ABattleHUDPresenter : public AActor
{
	GENERATED_BODY()

public:
	ABattleHUDPresenter();

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Battle HUD|References")
	TObjectPtr<ABattleManager> BattleManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Widget")
	TSubclassOf<UBattleHUDWidgetBase> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Widget")
	int32 ZOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Input")
	bool bConfigureGameAndUIInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Presentation")
	bool bEnableCommittedPresentation = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle HUD|Runtime")
	TObjectPtr<UBattleHUDViewModel> ViewModel = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle HUD|Runtime")
	TObjectPtr<UBattlePresentationController> PresentationController = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle HUD|Runtime")
	TObjectPtr<UBattleHUDWidgetBase> WidgetInstance = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// BeginPlay resolves the real local player and delegates to this assembly
	// boundary. Keeping actor lifecycle dispatch outside the method lets the
	// Editor-only test module provide an explicit valid local PlayerController
	// without calling BeginPlay directly on an actor that has not begun play.
	bool InitializeHUD(APlayerController* PlayerController);
};
