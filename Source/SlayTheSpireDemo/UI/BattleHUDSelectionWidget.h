#pragma once

#include "CoreMinimal.h"
#include "BattleHUDWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "BattleHUDSelectionWidget.generated.h"

class UBattleCardWidget;

/**
 * Shared Native Presentation surface for player card-selection interactions.
 *
 * This class owns only reusable Selection UI and confirmed visual handoff state.
 * Destination animation remains owned by the generic Native HUD zone handlers;
 * this layer only preserves exact RuntimeId source positions across reducer-owned
 * Hand rebuilds and restores them before a committed destination Record starts.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDSelectionWidget : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
	virtual bool SelectCard(int32 RuntimeId, bool bAllowFastPresentationCatchUp = true) override;
#if WITH_DEV_AUTOMATION_TESTS
	// Exact-token presentation lifecycle probe, matching the existing Native HUD
	// test pattern. Production playback still completes through the timer path.
	void FinishSharedHandToDrawPilePresentationForTesting(
		const FPresentationPlaybackToken& ExpectedToken)
	{
		FinishSharedHandToDrawPilePresentation(ExpectedToken);
	}

	void SetConfirmedCardCenterForTesting(int32 RuntimeId, const FVector2D& AbsoluteCenter)
	{
		ConfirmedCardCenters.Add(RuntimeId, AbsoluteCenter);
	}
	bool HasConfirmedCardCenterForTesting(int32 RuntimeId) const
	{
		return ConfirmedCardCenters.Contains(RuntimeId);
	}
	bool HasDeferredConfirmedDestinationForTesting() const
	{
		return bDeferredConfirmedHandDestinationStart;
	}
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnBattleHUDViewModelChanged() override;
	virtual bool BeginPresentationRecordPlayback_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token) override;
	virtual void CancelPresentationRecordPlayback_Implementation(
		const FPresentationPlaybackToken& Token) override;

private:
	UFUNCTION()
	void HandleSelectionAwareConfirmClicked();

	UFUNCTION()
	void HandleSelectionAwareCancelClicked();

	void RefreshSharedSelectionPresentation();
	void ResetSharedSelectionCardVisuals();
	void ClearSharedSelectionControlsAfterSubmit();
	void UpdateSelectionCardPositions();
	void UpdateConfirmedCardHandoffPositions();
	bool TryApplyConfirmedCardCenterToFormalWidget(int32 RuntimeId, bool bHideIfGeometryUnavailable);
	void ConsumeConfirmedCardHandoff(int32 RuntimeId);
	void RetryDeferredConfirmedHandDestinationPresentation();
	void ResetDeferredConfirmedHandDestinationPresentation();
	void SetPlayedCardSelectionHidden(bool bHidden);
	void SetSelectionLayoutActive(bool bActive);
	UPROPERTY(Transient)
	TObjectPtr<class UBorder> SelectionBackdrop = nullptr;
	TMap<TWeakObjectPtr<UCanvasPanelSlot>, FAnchorData> SelectionOriginalLayouts;
	TMap<TWeakObjectPtr<UCanvasPanelSlot>, int32> SelectionOriginalZOrders;
	TMap<int32, FVector2D> ConfirmedCardCenters;
	bool bSelectionVisualMode = false;

	FPresentationRecord DeferredConfirmedHandDestinationRecord;
	FPresentationPlaybackToken DeferredConfirmedHandDestinationToken;
	bool bDeferredConfirmedHandDestinationStart = false;

	bool BeginSharedHandToDrawPilePresentation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	void UpdateSharedHandToDrawPileAnimation(float DeltaSeconds);
	void FinishSharedHandToDrawPilePresentation(
		const FPresentationPlaybackToken& ExpectedToken);
	void CancelSharedHandToDrawPilePresentation();
	void ResetSharedHandToDrawPileState();

	UPROPERTY(Transient)
	TObjectPtr<UBattleCardWidget> SharedTransferMovingCard = nullptr;

	TWeakObjectPtr<UBattleCardWidget> SharedTransferHistoricalCard;
	ESlateVisibility SharedTransferHistoricalVisibility = ESlateVisibility::Visible;
	FPresentationPlaybackToken SharedTransferToken;
	FTimerHandle SharedTransferFinishTimer;
	FVector2D SharedTransferStartTranslation = FVector2D::ZeroVector;
	FVector2D SharedTransferEndTranslation = FVector2D::ZeroVector;
	float SharedTransferElapsedSeconds = 0.0f;
	bool bSharedTransferGeometryInitialized = false;
	bool bSharedTransferActive = false;
	bool bSelectionAwareDelegatesBound = false;
};
