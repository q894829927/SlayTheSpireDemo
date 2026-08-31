#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDTypes.h"
#include "BattleCardWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBattleCardRequested,
	int32,
	RuntimeId
);

/**
 * Native card presentation/input boundary for the A2N migration.
 *
 * The Widget owns only the supplied FBattleHUDCardView and emits a UI request
 * containing the frozen RuntimeId. It never owns or queries the HUD ViewModel,
 * Gameplay card instance, BattleManager or PresentationController.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Card")
	void SetCardView(const FBattleHUDCardView& View);

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Card")
	int32 GetRuntimeId() const { return CardView.RuntimeId; }

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Card")
	FName GetCardId() const { return CardView.CardId; }

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Card")
	FBattleHUDCardView GetCardView() const { return CardView; }

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Card")
	FOnBattleCardRequested OnBattleCardRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void RefreshFromCardView();

	UFUNCTION()
	void HandleCardClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Card;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_CardName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Cost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_CardDescription;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_CardType;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_CardArt;

	bool AreNativeBindingsValid() const { return bNativeBindingsValid; }

private:
	UPROPERTY(Transient)
	FBattleHUDCardView CardView;

	bool bNativeBindingsValid = false;
	bool bCardDelegateBound = false;
};
