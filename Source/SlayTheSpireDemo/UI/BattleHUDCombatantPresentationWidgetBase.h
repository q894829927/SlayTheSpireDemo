#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BattleHUDTypes.h"
#include "BattleHUDCombatantPresentationWidgetBase.generated.h"

class UBattleHUDCombatantPresentationWidgetBase;
class UImage;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FBattleHUDCombatantPresentationEvent,
	UBattleHUDCombatantPresentationWidgetBase*, Presentation
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FBattleHUDCombatantTargetRequested,
	int32, TargetId
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FBattleHUDCombatantPreviewRequested,
	int32, TargetId
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBattleHUDCombatantPreviewCleared);

UENUM(BlueprintType)
enum class EBattleHUDCombatantAnimation : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Hit UMETA(DisplayName = "Hit"),
	Attack UMETA(DisplayName = "Attack"),
	Victory UMETA(DisplayName = "Victory"),
	Defeat UMETA(DisplayName = "Defeat"),
	Death UMETA(DisplayName = "Death")
};

/**
 * Presentation-only interaction contract for one visible combatant.
 *
 * The widget never decides target legality and never submits gameplay requests.
 * Its owner supplies the current ViewModel snapshot plus legal-target mapping,
 * then handles the emitted inspection/preview/target events.
 */
UCLASS(Abstract, Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDCombatantPresentationWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Combatant Presentation")
	void SetPresentationData(
		const FBattleHUDCombatantView& InCombatantView,
		bool bInTargetSelectionActive,
		bool bInLegalTarget,
		int32 InTargetId,
		bool bInTargetHighlighted = false
	);

	// Wire the character hit-area Button's OnHovered / OnUnhovered events here.
	// Focus is tracked automatically through NativeOnAdded/RemovedFromFocusPath.
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Combatant Presentation")
	void SetPointerInspectionActive(bool bActive);

	// Explicit optional pin request for a future touch/accessibility policy.
	// Normal primary click does not call this function.
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Combatant Presentation")
	bool RequestPinnedInspection();

	// Intended for the transparent hit-area Button's OnClicked event. It emits
	// only a gameplay-provided legal TargetId during target selection. Outside
	// target selection, normal primary click is intentionally a no-op.
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Combatant Presentation")
	bool RequestPrimaryInteraction();

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Combatant Presentation")
	bool IsTransientInspectionActive() const;

	// Presentation-only animation request. The HUD calls this from committed
	// Presentation Records; Gameplay state is never changed here.
	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Combatant Animation")
	void PlayCombatantAnimation(EBattleHUDCombatantAnimation Animation);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Combatant Animation")
	void StopCombatantAnimation();

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Combatant Animation")
	EBattleHUDCombatantAnimation GetCurrentCombatantAnimation() const
	{
		return CurrentAnimation;
	}

	// Pure timing helper kept public so Blueprint or focused automation can
	// inspect the deterministic frame selection without a live widget.
	UFUNCTION(BlueprintPure, Category = "Battle HUD|Combatant Animation")
	static int32 GetAnimationFrameIndex(float ElapsedSeconds, float DurationSeconds, int32 FrameCount, bool bLoop);

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combatant Presentation")
	FBattleHUDCombatantView CombatantView;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combatant Presentation")
	bool bTargetSelectionActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combatant Presentation")
	bool bLegalTarget = false;

	// Visual selection affordance only. This remains independent from
	// bLegalTarget so later non-interactive emphasis never becomes target authority.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combatant Presentation")
	bool bTargetHighlighted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combatant Presentation")
	int32 TargetId = INDEX_NONE;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation")
	FBattleHUDCombatantPresentationEvent OnInspectRequested;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation")
	FBattleHUDCombatantPresentationEvent OnInspectCleared;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation")
	FBattleHUDCombatantPresentationEvent OnInspectPinRequested;

	// A3 PreviewTarget ownership is intentionally distinct from inspection even
	// though the same pointer/focus state may nominate both transient surfaces.
	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation|Preview")
	FBattleHUDCombatantPreviewRequested OnPreviewRequested;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation|Preview")
	FBattleHUDCombatantPreviewCleared OnPreviewCleared;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Combatant Presentation")
	FBattleHUDCombatantTargetRequested OnTargetRequested;

	// These controls are intentionally Blueprint-editable so the animation can
	// be tuned from the existing combatant Widget Blueprint without rebuilding
	// the animation importer or changing Gameplay timings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation")
	bool bEnableNativeCharacterAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation")
	bool bAnimateEnemyCharacter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float IdleAnimationDuration = 6.6666f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float HitAnimationDuration = 0.3333f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float AttackAnimationDuration = 0.30f;

	// The default enemy profile is the authored Awakened One sequence. These
	// timings match the source Spine animations and remain Blueprint-tunable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation|Enemy", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float EnemyIdleAnimationDuration = 2.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation|Enemy", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float EnemyHitAnimationDuration = 0.3333f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation|Enemy", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float EnemyAttackAnimationDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VictoryPulseScale = 1.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation")
	FVector2D AttackTranslation = FVector2D(42.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DefeatOpacity = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation")
	TArray<TSoftObjectPtr<UTexture2D>> IdleAnimationFrames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation")
	TArray<TSoftObjectPtr<UTexture2D>> HitAnimationFrames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation")
	TSoftObjectPtr<UTexture2D> CorpseTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation|Enemy")
	TArray<TSoftObjectPtr<UTexture2D>> EnemyIdleAnimationFrames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation|Enemy")
	TArray<TSoftObjectPtr<UTexture2D>> EnemyHitAnimationFrames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation|Enemy")
	TArray<TSoftObjectPtr<UTexture2D>> EnemyAttackAnimationFrames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle HUD|Combatant Animation|Enemy")
	TSoftObjectPtr<UTexture2D> EnemyCorpseTexture;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle HUD|Combatant Presentation", meta = (DisplayName = "Combatant Presentation Changed"))
	void BP_OnPresentationChanged();

private:
	void SetFocusInspectionActive(bool bActive);
	void PublishTransientInspectionState(bool bWasActive);
	void PublishTransientPreviewState();
	void ClearTransientInspection();
	bool RequestLegalTarget();
	void EnsureAnimationAssetsLoaded();
	UTexture2D* ResolveAnimationTexture(TSoftObjectPtr<UTexture2D>& Texture);
	UTexture2D* ResolveAnimatedFallbackTexture();
	void ApplyAnimationFrame();
	void ApplyCharacterTexture(UTexture2D* Texture);
	void ApplyCharacterTransform(float TranslationAlpha, float ScaleMultiplier, float Angle);
	bool ShouldUseNativeAnimationProfile() const;
	bool IsUsingEnemyAnimationProfile() const;
	float GetAnimationDuration(EBattleHUDCombatantAnimation Animation) const;
	TArray<TSoftObjectPtr<UTexture2D>>& GetFramesForAnimation(EBattleHUDCombatantAnimation Animation);
	const TArray<TSoftObjectPtr<UTexture2D>>& GetFramesForAnimation(EBattleHUDCombatantAnimation Animation) const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Img_Character;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FallbackCharacterTexture;

	// The Awakened One currently has no authored corpse texture. Keep the last
	// valid animation frame so terminal presentation does not reveal the
	// Blueprint's original static brush when the enemy dies.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LastAppliedAnimationFrame;

	// Soft paths describe the authored profile, but do not keep loaded textures
	// alive. Retain the loaded frames while this presentation widget exists so
	// a later Hit/Attack request cannot intermittently fall back to the static brush.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> LoadedAnimationTextures;

	FWidgetTransform BaseCharacterTransform;
	float BaseCharacterOpacity = 1.0f;
	EBattleHUDCombatantAnimation CurrentAnimation = EBattleHUDCombatantAnimation::Idle;
	float AnimationElapsedSeconds = 0.0f;
	bool bAnimationAssetsLoaded = false;
	bool bAnimationAssetsLoadAttempted = false;
	bool bAnimationWidgetReady = false;

	bool bPointerInspectionActive = false;
	bool bFocusInspectionActive = false;
	int32 PublishedPreviewTargetId = INDEX_NONE;
};
