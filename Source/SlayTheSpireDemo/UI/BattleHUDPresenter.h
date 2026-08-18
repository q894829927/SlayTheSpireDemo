#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleHUDPresenter.generated.h"

class ABattleManager;
class UBattleHUDViewModel;
class UBattleHUDWidgetBase;

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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle HUD|Runtime")
	TObjectPtr<UBattleHUDViewModel> ViewModel = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle HUD|Runtime")
	TObjectPtr<UBattleHUDWidgetBase> WidgetInstance = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
