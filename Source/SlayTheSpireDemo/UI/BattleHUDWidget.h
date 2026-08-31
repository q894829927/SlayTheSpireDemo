#pragma once

#include "CoreMinimal.h"
#include "BattleHUDWidgetBase.h"
#include "BattleHUDTypes.h"
#include "BattleHUDWidget.generated.h"

class UBattleCardWidget;
class UBattleStatusWidget;
class UBattleHUDCombatantPresentationWidgetBase;
class UButton;
class UHorizontalBox;
class UOverlay;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWrapBox;
class UWidget;

/**
 * Native HUD ownership boundary for the A2N migration.
 *
 * R3-A owns frozen static HUD refresh and long-lived input delegates. R4 adds
 * formal Hand rebuild and card-request ownership. Presentation Record playback
 * remains deliberately unimplemented until R5 and the later per-Record phases.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDWidget : public UBattleHUDWidgetBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Widgets")
	TSubclassOf<UBattleCardWidget> CardWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Battle HUD|Widgets")
	TSubclassOf<UBattleStatusWidget> StatusWidgetClass;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnBattleHUDViewModelChanged() override;
	virtual bool BeginPresentationRecordPlayback_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token
	) override;

	void RefreshHUDFromViewModel();
	void RefreshHand();
	void RefreshCombatants();
	void RefreshEnergy();
	void RefreshPileCounts();
	void RefreshInputState();
	void RefreshFeedback();
	void RefreshTerminalFromViewModel();
	void RefreshEnemyIntent();
	bool RefreshStatusTooltip(
		UWidget* StatusTooltip,
		const TArray<FBattleHUDStatusView>& Statuses);

	UFUNCTION()
	void HandleCardRequested(int32 RuntimeId);

	UFUNCTION()
	void HandleEndTurnClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleCombatantTargetRequested(int32 TargetId);

	UFUNCTION()
	void HandleCombatantInspectRequested(UBattleHUDCombatantPresentationWidgetBase* Presentation);

	UFUNCTION()
	void HandleCombatantInspectCleared(UBattleHUDCombatantPresentationWidgetBase* Presentation);

	bool AreNativeBindingsValid() const { return bNativeBindingsValid; }

	// Required Designer-backed controls.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBattleHUDCombatantPresentationWidgetBase> Combatant_PlayerPresentation;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBattleHUDCombatantPresentationWidgetBase> Combatant_EnemyPresentation;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HB_Hand;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_EndTurn;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Confirm;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Cancel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Feedback;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> OV_PlayArea;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_DamagePresentation;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_Terminal;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> PB_PlayerHP;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_PlayerHP;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_PlayerBlock;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> WB_PlayerStatuses;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> PB_EnemyHP;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_EnemyHP;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_EnemyBlock;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> WB_EnemyStatuses;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_DrawCount;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_DiscardCount;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_ExhaustCount;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Energy;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Outcome;

	// Truly optional surfaces. They are not required for the Native HUD shell.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> EnemyIntentPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StatusTooltip_Player;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StatusTooltip_Enemy;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_PlayerName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_EnemyName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_EnemyIntent;

private:
	bool bNativeBindingsValid = false;
	bool bNativeDelegatesBound = false;
};
