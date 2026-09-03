#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDTypes.h"
#include "BattleRelicWidget.generated.h"

class UBattleRelicTooltipWidget;
class UButton;
class UImage;
class UTextBlock;
struct FGeometry;

/**
 * Native frozen-view boundary for one player Relic.
 *
 * The steady-state surface shows only the Relic icon plus an optional numeric
 * counter. Btn_RelicHover is the explicit Slate hit target for hover lifecycle;
 * hover presentation is delegated to a separate frozen Tooltip Widget.
 * This Widget never queries URelicInstance, URelicData or BattleManager and never
 * mutates the authoritative Relic counter.
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

	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Relic|Tooltip")
	TSubclassOf<UBattleRelicTooltipWidget> TooltipWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Relic|Tooltip")
	FVector2D TooltipCursorOffset = FVector2D(18.0f, 18.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Relic|Tooltip")
	int32 TooltipZOrder = 1000;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_RelicHover;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_RelicIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_RelicCounter;

private:
	UFUNCTION()
	void HandleRelicHovered();

	UFUNCTION()
	void HandleRelicUnhovered();

	void RefreshFromRelicView();
	UBattleRelicTooltipWidget* CreateRelicTooltipWidget() const;
	void ShowRelicTooltip();
	void HideRelicTooltip();
	void UpdateRelicTooltipPosition();

	UPROPERTY(Transient)
	FBattleHUDRelicView NativeRelicView;

	UPROPERTY(Transient)
	TObjectPtr<UBattleRelicTooltipWidget> ActiveTooltipWidget = nullptr;

	bool bNativeBindingsValid = false;
};
