#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Presentation/PresentationTypes.h"
#include "BattleHUDWidgetBase.generated.h"

class UBattleHUDViewModel;
class UBattlePresentationController;

UCLASS(Abstract, Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void SetViewModel(UBattleHUDViewModel* InViewModel);

	void SetPresentationController(UBattlePresentationController* InController);

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

	// Return true only when Blueprint actually started asynchronous playback and
	// will later call NotifyPresentationFinished(Token). The native default returns
	// false, providing the A2A missing-callback immediate fallback without asset edits.
	UFUNCTION(BlueprintNativeEvent, Category = "Battle Presentation")
	bool PlayPresentationRecord(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token
	);
	virtual bool PlayPresentationRecord_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token
	);

	UFUNCTION(BlueprintCallable, Category = "Battle Presentation")
	void NotifyPresentationFinished(const FPresentationPlaybackToken& Token);

	UFUNCTION(BlueprintCallable, Category = "Battle Presentation")
	void SkipPresentation();

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	TObjectPtr<UBattleHUDViewModel> ViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	TObjectPtr<UBattlePresentationController> PresentationController = nullptr;

protected:
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle HUD", meta = (DisplayName = "Battle HUD View Model Changed"))
	void BP_OnViewModelChanged();

private:
	UFUNCTION()
	void HandleViewModelChanged();
};
