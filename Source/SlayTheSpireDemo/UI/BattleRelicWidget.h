#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDTypes.h"
#include "BattleRelicWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * Native frozen-view boundary for one player Relic.
 *
 * The Widget owns only the supplied FBattleHUDRelicView and Designer rendering.
 * It never queries URelicInstance, URelicData or BattleManager and never mutates
 * the authoritative Relic counter.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleRelicWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Relic")
	void SetRelicView(const FBattleHUDRelicView& View);

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Relic")
	FBattleHUDRelicView GetRelicView() const { return NativeRelicView; }

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Relic")
	FName GetRelicId() const { return NativeRelicView.RelicId; }

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Relic")
	int64 GetRuntimeSequence() const { return NativeRelicView.RuntimeSequence; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_RelicIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_RelicName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_RelicCounter;

private:
	void RefreshFromRelicView();

	UPROPERTY(Transient)
	FBattleHUDRelicView NativeRelicView;

	bool bNativeBindingsValid = false;
};
