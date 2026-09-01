#pragma once

#include "CoreMinimal.h"
#include "BattleHUDWidgetBase.h"
#include "BattleHUDTypes.h"
#include "TimerManager.h"
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
 * formal Hand rebuild and card-request ownership. R5 adds the local Native
 * committed-presentation playback kernel. R6 adds EnergyChanged, BlockChanged
 * and DeckShuffled frozen Record visuals. R7 adds only Damage playback. R8
 * adds the committed CardPlayed/CardZoneChanged lifecycle, including one-card-
 * per-Record DrawPile-to-Hand movement. R9 adds formal Native Status rows and
 * exact-identity StatusChanged playback. R10 adds terminal Record rendering and
 * keeps PresentationUnavailable as a separate ViewModel-driven UI state.
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

	// Native card-request call sites intentionally resolve to this C++ overload.
	// Slow input preserves the inherited request path. A click that lands during
	// an actually active Native presentation catches up through formal Skip, then
	// retries the latest card RuntimeId on the next CoreTicker turn. The inherited
	// one-parameter Blueprint UFUNCTION remains unchanged.
	bool SelectCard(int32 RuntimeId, bool bAllowFastPresentationCatchUp = true);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnBattleHUDViewModelChanged() override;
	virtual bool BeginPresentationRecordPlayback_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token
	) override;
	virtual void CancelPresentationRecordPlayback_Implementation(
		const FPresentationPlaybackToken& Token
	) override;

	// R5 playback-kernel primitives. Later per-Record handlers validate and
	// prepare their own resources, then use these helpers to establish exact local
	// visual ownership and the token-captured finish boundary.
	bool CommitNativePresentationOwnership(
		EBattlePresentationRecordType RecordType,
		const FPresentationPlaybackToken& Token);
	bool StartNativePresentationFinishTimer(float DurationSeconds);
	void AbortNativePresentationStart();
	void FinishNativePresentation(const FPresentationPlaybackToken& ExpectedToken);
	void ClearNativePresentationFinishTimer();
	void ResetNativePresentationOwnership();
	void FinishNativePresentationVisual(EBattlePresentationRecordType RecordType);
	void CancelNativePresentationVisual(EBattlePresentationRecordType RecordType);
	void CleanupNativePresentationVisualsOnDestruct();
	void ResetNativeSimplePresentationState();

	bool BeginNativeEnergyChangedPresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativeBlockChangedPresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativeDeckShuffledPresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativeDamagePresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativeCardPlayedPresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativeCardZoneChangedPresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativeHandToDiscardPresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativeDrawToHandPresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativePlayAreaToDestinationPresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativeStatusChangedPresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativeTerminalPresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool IsNativeCardSnapshotValid(const FPresentationCardSnapshot& Snapshot) const;
	bool DoesNativeCardViewMatchSnapshot(
		const FBattleHUDCardView& View,
		const FPresentationCardSnapshot& Snapshot) const;
	bool FindExactHistoricalHandCard(
		const FPresentationCardSnapshot& Snapshot,
		int32 RequiredIndex,
		UBattleCardWidget*& OutCardWidget) const;
	bool IsRuntimeIdAbsentFromNativeCardVisuals(int32 RuntimeId) const;
	UBattleCardWidget* CreateNativePresentationCard(
		const FPresentationCardSnapshot& Snapshot) const;
	void ConfigureNativeCardAnimation(
		UBattleCardWidget* MovingCard,
		UWidget* StartAnchor,
		UWidget* EndAnchor,
		const FVector2D& FallbackStartTranslation,
		const FVector2D& FallbackEndTranslation,
		float StartScale,
		float EndScale,
		float StartOpacity,
		float EndOpacity);
	void UpdateNativeCardAnimation(float DeltaSeconds);
	void NormalizeNativeCardTransform(UBattleCardWidget* CardWidget) const;
	void FinishNativeCardPresentation(EBattlePresentationRecordType RecordType);
	void CancelNativeCardPresentation(EBattlePresentationRecordType RecordType);
	void CleanupNativeCardPresentationOnDestruct();
	void ResetNativeCardRecordState();
	bool IsNativeRecordTokenConsistent(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token) const;
	bool IsKnownCombatantPresentationId(FName PresentationId) const;
	UTextBlock* ResolveBlockTextForPresentationId(
		FName PresentationId,
		int32& OutHistoricalBlock) const;
	bool ResolveDamageTarget(
		FName PresentationId,
		UBattleHUDCombatantPresentationWidgetBase*& OutPresentation,
		UProgressBar*& OutHPProgress,
		UTextBlock*& OutHPText,
		UTextBlock*& OutBlockText,
		const FBattleHUDCombatantView*& OutHistoricalView) const;
	bool ResolveNativeStatusTarget(
		FName PresentationId,
		UWrapBox*& OutContainer,
		const FBattleHUDCombatantView*& OutHistoricalView) const;
	int32 CountHistoricalStatusIdentity(
		const TArray<FBattleHUDStatusView>& Statuses,
		FName StatusId,
		int64 RuntimeSequence) const;
	int32 CountNativeStatusWidgetIdentity(
		UWrapBox* Container,
		FName StatusId,
		int64 RuntimeSequence) const;
	bool FindHistoricalStatusByIdentity(
		const TArray<FBattleHUDStatusView>& Statuses,
		FName StatusId,
		int64 RuntimeSequence,
		const FBattleHUDStatusView*& OutStatus) const;
	bool FindNativeStatusWidgetByIdentity(
		UWrapBox* Container,
		FName StatusId,
		int64 RuntimeSequence,
		UBattleStatusWidget*& OutWidget) const;
	UBattleStatusWidget* CreateNativeStatusWidget(
		const FBattleHUDStatusView& View) const;
	bool RebuildNativeStatusRows(
		UWrapBox* Container,
		const TArray<FBattleHUDStatusView>& Statuses);
	void FinishNativeStatusPresentation();
	void CancelNativeStatusPresentation();
	void CleanupNativeStatusPresentationOnDestruct();
	void ResetNativeStatusPresentationState();
	void ApplyNativeTerminalOutcome(EBattleHUDOutcome Outcome);
	void FinishNativeTerminalPresentation();
	void CancelNativeTerminalPresentation();
	void CleanupNativeTerminalPresentationOnDestruct();
	void ApplyNativeEnergyValue(int32 Energy, int32 MaxEnergy);
	void ApplyNativeBlockValue(UTextBlock* BlockText, int32 Block);
	void ApplyNativePileCounts(int32 DrawCount, int32 DiscardCount);
	void ApplyNativeCombatantVitals(
		UProgressBar* HPProgress,
		UTextBlock* HPText,
		UTextBlock* BlockText,
		int32 HP,
		int32 MaxHP,
		int32 Block);
	void CleanupNativeDamageTransientVisuals();
	void ResetNativeDamagePresentationState();

	enum class ENativeCardPresentationKind : uint8
	{
		None,
		CardPlayed,
		HandToDiscard,
		DrawToHand,
		PlayAreaToDestination
	};

	ENativeCardPresentationKind GetActiveNativeCardPresentationKind() const
	{
		return ActiveNativeCardPresentationKind;
	}
	UBattleCardWidget* GetNativePlayedCardWidget() const
	{
		return NativePlayedCardWidget.Get();
	}
	UBattleCardWidget* GetNativeDrawnCardWidget() const
	{
		return ActiveNativeDrawnCardWidget.Get();
	}
	UBattleCardWidget* GetNativeZoneCardWidget() const
	{
		return ActiveNativeZoneCardWidget.Get();
	}
	UBattleCardWidget* GetNativeHistoricalHandCardWidget() const
	{
		return ActiveNativeHistoricalHandCardWidget.Get();
	}
	bool IsNativeCardAnimationInitialized() const
	{
		return bNativeCardAnimationInitialized;
	}
	UBattleStatusWidget* GetActiveNativeStatusPresentationWidget() const
	{
		return ActiveNativeStatusPresentationWidget.Get();
	}
	bool IsActiveNativeStatusCreatedTransient() const
	{
		return bActiveNativeStatusCreatedTransient;
	}

	bool HasActiveNativePresentation() const { return bHasActiveNativePresentation; }
	bool HasNativePresentationFinishTimer() const { return NativePresentationFinishTimer.IsValid(); }
	EBattlePresentationRecordType GetActiveNativePresentationType() const
	{
		return ActiveNativePresentationType;
	}
	const FPresentationPlaybackToken& GetActiveNativePresentationToken() const
	{
		return ActiveNativePresentationToken;
	}

	void RefreshHUDFromViewModel();
	void RefreshHand();
	void RefreshCombatants();
	void RefreshStatusRows();
	void RefreshEnergy();
	void RefreshPileCounts();
	void RefreshInputState();
	void RefreshFeedback();
	void RefreshTerminalFromViewModel();
	void RefreshPresentationAvailabilityFromViewModel();
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
	void RetryPendingFastCardSelection();

	bool bNativeBindingsValid = false;
	bool bNativeDelegatesBound = false;
	int32 PendingFastCardRuntimeId = INDEX_NONE;
	bool bFastCardSelectionRetryScheduled = false;

	// Controller/reducer state never lives here. These fields describe only the
	// one local visual currently owned by this HUD.
	bool bHasActiveNativePresentation = false;
	EBattlePresentationRecordType ActiveNativePresentationType = EBattlePresentationRecordType::None;
	FPresentationPlaybackToken ActiveNativePresentationToken;
	FTimerHandle NativePresentationFinishTimer;

	// Frozen R6 visual state only. These values are copied from the accepted
	// Record (plus frozen MaxEnergy, which is not carried by EnergyChanged) so
	// Finish/Cancel never query mutable Gameplay or infer historical values.
	TWeakObjectPtr<UTextBlock> ActiveNativeSimpleBlockText;
	int32 ActiveNativeSimplePrimaryBefore = 0;
	int32 ActiveNativeSimplePrimaryAfter = 0;
	int32 ActiveNativeSimpleSecondaryBefore = 0;
	int32 ActiveNativeSimpleSecondaryAfter = 0;
	int32 ActiveNativeSimpleEnergyMax = 0;

	// Frozen R7 Damage visual state. Target surfaces are weak local presentation
	// references; all historical values are copied from the accepted Record.
	TWeakObjectPtr<UBattleHUDCombatantPresentationWidgetBase> ActiveDamageTargetWidget;
	TWeakObjectPtr<UProgressBar> ActiveDamageTargetHPProgress;
	TWeakObjectPtr<UTextBlock> ActiveDamageTargetHPText;
	TWeakObjectPtr<UTextBlock> ActiveDamageTargetBlockText;
	int32 ActiveDamageHPBefore = 0;
	int32 ActiveDamageHPAfter = 0;
	int32 ActiveDamageBlockBefore = 0;
	int32 ActiveDamageBlockAfter = 0;
	int32 ActiveDamageMaxHP = 0;

	// R8 owns only presentation Widgets. NativePlayedCardWidget intentionally
	// survives CardPlayed Finish so the later PlayArea zone Record can retire the
	// same frozen transient. Draw-to-Hand owns exactly one transient until its
	// exact Token finishes; the Controller's per-Record snapshot refresh then
	// replaces it with the formal Hand Widget before starting the next draw.
	ENativeCardPresentationKind ActiveNativeCardPresentationKind =
		ENativeCardPresentationKind::None;
	TWeakObjectPtr<UBattleCardWidget> NativePlayedCardWidget;
	TWeakObjectPtr<UBattleCardWidget> ActiveNativeHistoricalHandCardWidget;
	TWeakObjectPtr<UBattleCardWidget> ActiveNativeDrawnCardWidget;
	TWeakObjectPtr<UBattleCardWidget> ActiveNativeZoneCardWidget;
	TWeakObjectPtr<UBattleCardWidget> ActiveNativeMovingCardWidget;
	TWeakObjectPtr<UWidget> ActiveNativeCardAnimationStartAnchor;
	TWeakObjectPtr<UWidget> ActiveNativeCardAnimationEndAnchor;
	ESlateVisibility ActiveNativeHistoricalHandVisibility = ESlateVisibility::Visible;
	int32 ActiveNativeDrawCountBefore = 0;
	int32 ActiveNativeDrawCountAfter = 0;
	int32 ActiveNativeCardDestinationIndex = INDEX_NONE;
	float ActiveNativeCardAnimationElapsedSeconds = 0.0f;
	FVector2D ActiveNativeCardAnimationStartTranslation = FVector2D::ZeroVector;
	FVector2D ActiveNativeCardAnimationEndTranslation = FVector2D::ZeroVector;
	FVector2D ActiveNativeCardAnimationFallbackStart = FVector2D::ZeroVector;
	FVector2D ActiveNativeCardAnimationFallbackEnd = FVector2D::ZeroVector;
	float ActiveNativeCardAnimationStartScale = 1.0f;
	float ActiveNativeCardAnimationEndScale = 1.0f;
	float ActiveNativeCardAnimationStartOpacity = 1.0f;
	float ActiveNativeCardAnimationEndOpacity = 1.0f;
	bool bNativeCardAnimationInitialized = false;

	// R9 stores only the exact formal/transient Status Widget currently touched
	// by StatusChanged. Before view/visibility are rollback data for a failed
	// Begin; exact Cancel rebuilds both formal rows from the historical ViewModel.
	TWeakObjectPtr<UBattleStatusWidget> ActiveNativeStatusPresentationWidget;
	FBattleHUDStatusView ActiveNativeStatusBeforeView;
	ESlateVisibility ActiveNativeStatusBeforeVisibility = ESlateVisibility::Visible;
	bool bActiveNativeStatusCreatedTransient = false;
};