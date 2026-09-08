#pragma once

#include "CoreMinimal.h"
#include "BattleHUDReconciledWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "BattleHUDSelectionWidget.generated.h"

class UBattleCardWidget;
class UOverlay;

/**
 * Shared Native Presentation surface for player card-selection interactions.
 *
 * This class owns only reusable Selection UI/presentation behavior:
 * explicit Confirm/Cancel routing, persistent SelectionArea hosting, and the
 * staged legacy selected-card transfer compatibility path. It contains no
 * CardId/Effect-specific branches.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDSelectionWidget : public UBattleHUDReconciledWidget
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

	UOverlay* GetSelectionAreaHostForTesting() const
	{
		return SelectionAreaHost;
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

	bool EnsureSelectionAreaHost();
	void RefreshSharedSelectionPresentation();
	void ResetSharedSelectionCardVisuals();
	void ClearSharedSelectionControlsAfterSubmit();
	void UpdateSelectionCardPositions();
	void SetPlayedCardSelectionHidden(bool bHidden);
	void SetSelectionLayoutActive(bool bActive);

	// G0-C persistent visual host. It is intentionally empty/dormant until G4/G5
	// switches selected-card production ownership away from formal Hand widgets.
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> SelectionAreaHost = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<class UBorder> SelectionBackdrop = nullptr;
	TMap<TWeakObjectPtr<UCanvasPanelSlot>, FAnchorData> SelectionOriginalLayouts;
	TMap<TWeakObjectPtr<UCanvasPanelSlot>, int32> SelectionOriginalZOrders;
	TMap<int32, FVector2D> ConfirmedCardCenters;
	bool bSelectionVisualMode = false;

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
