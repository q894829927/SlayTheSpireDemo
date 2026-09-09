#pragma once

#include "CoreMinimal.h"
#include "BattleHUDReconciledWidget.h"
#include "Components/Widget.h"
#include "BattleHUDCardTransitionWidget.generated.h"

class UBattleCardWidget;
class UOverlay;

/**
 * One generic per-card transition child. G4 production uses exactly one child
 * for SingleRecord playback; G6 prepares N children from the same engine.
 * Gameplay zones remain immutable committed facts carried by the Record.
 */
USTRUCT()
struct FNativeCardTransitionInstance
{
	GENERATED_BODY()

	int32 RuntimeId = INDEX_NONE;
	ECardZone FromZone = ECardZone::Hand;
	ECardZone ToZone = ECardZone::Hand;
	ECardPresentationOwner SourceOwner = ECardPresentationOwner::Hand;
	int64 SelectionGeneration = 0;

	UPROPERTY(Transient)
	TObjectPtr<UBattleCardWidget> MovingVisual = nullptr;

	TWeakObjectPtr<UBattleCardWidget> HistoricalHandVisual;
	ESlateVisibility HistoricalHandVisibility = ESlateVisibility::Visible;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> StartAnchor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> EndAnchor = nullptr;

	FVector2D FallbackStartTranslation = FVector2D::ZeroVector;
	FVector2D FallbackEndTranslation = FVector2D::ZeroVector;
	FVector2D StartTranslation = FVector2D::ZeroVector;
	FVector2D EndTranslation = FVector2D::ZeroVector;
	FVector2D AbsoluteSourceCenter = FVector2D::ZeroVector;
	FWidgetTransform SourceRenderTransform;
	ESlateVisibility SourceVisibility = ESlateVisibility::Visible;
	float SourceRenderOpacity = 1.0f;
	float StartScale = 1.0f;
	float EndScale = 1.0f;
	float StartOpacity = 1.0f;
	float EndOpacity = 1.0f;
	float OpacityFadeStartAlpha = 0.0f;
	float ElapsedSeconds = 0.0f;
	bool bSourceWasEnabled = true;
	bool bHasAbsoluteSourceCenter = false;
	bool bKeepDestinationAtSource = false;
	bool bCreatedMovingVisual = false;
	bool bSelectionAreaVisualTransferred = false;
	bool bSelectionAreaVisualStateCaptured = false;
	bool bGeometryInitialized = false;
};

/**
 * Generic card-transition presentation layer.
 *
 * G4/G5 use one child for SingleRecord playback. G6 uses the same child engine
 * for an explicitly validated SelectionDestination Group and never changes
 * Gameplay/reducer chronology.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDCardTransitionWidget : public UBattleHUDReconciledWidget
{
	GENERATED_BODY()

public:
#if WITH_DEV_AUTOMATION_TESTS
	void FinishNativeCardTransitionForTesting(const FPresentationPlaybackToken& Token)
	{
		FinishNativeCardTransition(Token);
	}

	void FinishNativeCardTransitionGroupForTesting(const FPresentationPlaybackToken& Token)
	{
		FinishNativeCardTransitionGroup(Token);
	}

	int32 GetActiveNativeCardTransitionCountForTesting() const
	{
		return ActiveNativeCardTransitions.Num();
	}

	UBattleCardWidget* GetActiveNativeCardTransitionVisualForTesting() const
	{
		return ActiveNativeCardTransitions.Num() == 1
			? ActiveNativeCardTransitions[0].MovingVisual.Get()
			: nullptr;
	}
#endif

protected:
	void ReleaseObsoleteSelectionTransitions();
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool BeginPresentationRecordPlayback_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token) override;
	virtual void CancelPresentationRecordPlayback_Implementation(
		const FPresentationPlaybackToken& Token) override;
	virtual bool BeginPresentationGroupPlayback(
		const TArray<FPresentationRecord>& Records,
		const FPresentationGroupTag& Group,
		const FPresentationPlaybackToken& Token) override;
	virtual void CancelPresentationGroupPlayback(
		const FPresentationGroupTag& Group,
		const FPresentationPlaybackToken& Token) override;

	// G4 source adapter. Base/reconciled HUDs have no SelectionArea surface;
	// UBattleHUDSelectionWidget supplies its persistent Host without adding any
	// destination-specific animation branch.
	virtual UOverlay* GetCardTransitionSelectionAreaHost() const
	{
		return nullptr;
	}

	// Generic lifecycle hooks only. Derived Selection code may retire temporary
	// continuity bookkeeping here, but must not branch on destination/CardId.
	virtual void OnNativeCardTransitionAccepted(int32 RuntimeId) {}
	virtual void OnNativeCardTransitionEnded(int32 RuntimeId, bool bCancelled) {}

private:
	bool BeginNativeOutgoingCardTransition(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	bool BeginNativeOutgoingCardTransitionGroup(
		const TArray<FPresentationRecord>& Records,
		const FPresentationGroupTag& Group,
		const FPresentationPlaybackToken& Token);
	bool PrepareNativeOutgoingCardTransitionGroupMember(
		const FPresentationRecord& Record,
		int64& InOutSelectionGeneration,
		FNativeCardTransitionInstance& OutInstance) const;
	bool ValidateNativeCardTransitionDestination(
		const FCardZoneChangedPresentationPayload& Payload,
		FNativeCardTransitionInstance& OutInstance) const;
	bool ValidateNativeCardTransitionGroupDestination(
		const FCardZoneChangedPresentationPayload& Payload,
		FNativeCardTransitionInstance& OutInstance) const;
	bool ResolveSelectionAreaTransitionVisual(
		const FPresentationCardSnapshot& Snapshot,
		UBattleCardWidget*& OutVisual,
		FVector2D& OutAbsoluteCenter,
		bool& bOutHasAbsoluteCenter) const;
	bool RestoreSelectionAreaTransitionVisual(FNativeCardTransitionInstance& Instance);
	bool AttachTransitionVisualToPlayArea(UBattleCardWidget* Visual);
	FVector2D ResolveTransitionAnchorTranslation(
		const FNativeCardTransitionInstance& Instance,
		UWidget* Anchor,
		const FVector2D& Fallback) const;
	void UpdateNativeCardTransitions(float DeltaSeconds);
	bool StartNativeCardTransitionFinishTimer(float DurationSeconds);
	bool StartNativeCardTransitionGroupFinishTimer(float DurationSeconds);
	void FinishNativeCardTransition(const FPresentationPlaybackToken& ExpectedToken);
	void FinishNativeCardTransitionGroup(const FPresentationPlaybackToken& ExpectedToken);
	void FinishNativeCardTransitionVisuals();
	void CancelNativeCardTransitionVisuals();
	void RollbackPreparedSelectionAreaTransition();
	void RestorePreparedGroupSelectionAreaVisuals();
	void CleanupNativeCardTransitionsOnDestruct();
	void ClearNativeCardTransitionFinishTimer();
	void ResetNativeCardTransitionState();

	UPROPERTY(Transient)
	TArray<FNativeCardTransitionInstance> ActiveNativeCardTransitions;

	FTimerHandle NativeCardTransitionFinishTimer;
	FPresentationPlaybackToken NativeCardTransitionToken;
};
