#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleRelicStripWidget.generated.h"

class UBattleHUDViewModel;
class UBattleHUDWidgetBase;
class UBattleRelicWidget;
class UHorizontalBox;

/**
 * Native Phase 7D Relic-strip boundary embedded in the existing Native HUD.
 *
 * It observes only the HUD's frozen ViewModel. It does not query BattleManager,
 * URelicInstance, URelicData, or infer per-Record counter progression. Without a
 * Relic counter Record, visible counters therefore remain on the last historical
 * snapshot until the owning Controller applies Envelope.FinalSnapshot.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleRelicStripWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Relic")
	TSubclassOf<UBattleRelicWidget> RelicWidgetClass;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HB_Relics;

private:
	UFUNCTION()
	void HandleViewModelChanged();

	UBattleHUDWidgetBase* ResolveOwningBattleHUD() const;
	void BindToOwningViewModel();
	void UnbindFromViewModel();
	void RefreshRelics();
	bool CanReuseCurrentWidgets() const;
	UBattleRelicWidget* CreateRelicWidget() const;

	UPROPERTY(Transient)
	TObjectPtr<UBattleHUDViewModel> BoundViewModel = nullptr;

	bool bNativeBindingsValid = false;
};
