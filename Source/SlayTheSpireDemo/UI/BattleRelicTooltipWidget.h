#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDTypes.h"
#include "BattleRelicTooltipWidget.generated.h"

class UTextBlock;

/**
 * Native hover-tooltip surface for one frozen Relic view.
 *
 * It displays only immutable/frozen presentation data and never queries Gameplay
 * runtime objects. Positioning and lifetime are owned by UBattleRelicWidget.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleRelicTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Relic")
	void SetRelicView(const FBattleHUDRelicView& View);

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Relic")
	FBattleHUDRelicView GetRelicView() const { return NativeRelicView; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_RelicName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_RelicDescription;

private:
	void RefreshFromRelicView();

	UPROPERTY(Transient)
	FBattleHUDRelicView NativeRelicView;

	bool bNativeBindingsValid = false;
};
