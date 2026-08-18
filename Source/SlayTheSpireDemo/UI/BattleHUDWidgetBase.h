#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDWidgetBase.generated.h"

class UBattleHUDViewModel;

UCLASS(Abstract, Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void SetViewModel(UBattleHUDViewModel* InViewModel);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool SelectCard(int32 RuntimeId);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	void CancelSelection();

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool SelectTarget(int32 TargetId);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool ConfirmSelectedCard();

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool EndTurn();

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	TObjectPtr<UBattleHUDViewModel> ViewModel = nullptr;

protected:
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle HUD", meta = (DisplayName = "Battle HUD View Model Changed"))
	void BP_OnViewModelChanged();

private:
	UFUNCTION()
	void HandleViewModelChanged();
};
