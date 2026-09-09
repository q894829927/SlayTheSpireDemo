#pragma once

#include "CoreMinimal.h"
#include "BattleHUDCardTransitionWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "BattleHUDSelectionWidget.generated.h"

class UBattleCardWidget;
class UOverlay;

/**
 * Shared Native Presentation surface for player card-selection interactions.
 *
 * This class owns only reusable Selection UI/presentation behavior:
 * explicit Confirm/Cancel routing, persistent SelectionArea hosting, and the
 * staged legacy selected-card position compatibility path. Destination card
 * movement is owned by the generic G4 CardTransition layer below this class.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDSelectionWidget : public UBattleHUDCardTransitionWidget
{
	GENERATED_BODY()

public:
	virtual bool SelectCard(int32 RuntimeId, bool bAllowFastPresentationCatchUp = true) override;
#if WITH_DEV_AUTOMATION_TESTS
	// Kept as a compatibility test entry point while G4 migrates the old
	// Selection-subclass Hand->Draw path. It now drives the generic transition.
	void FinishSharedHandToDrawPilePresentationForTesting(
		const FPresentationPlaybackToken& ExpectedToken)
	{
		FinishNativeCardTransitionForTesting(ExpectedToken);
	}

	UOverlay* GetSelectionAreaHostForTesting() const
	{
		return SelectionAreaHost;
	}

	void SetSelectionAreaHostForTesting(UOverlay* InHost)
	{
		SelectionAreaHost = InHost;
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

	virtual UOverlay* GetCardTransitionSelectionAreaHost() const override
	{
		return SelectionAreaHost;
	}

	virtual bool TryGetCardTransitionCompatibilitySourceCenter(
		int32 RuntimeId,
		FVector2D& OutAbsoluteCenter) const override
	{
		if (const FVector2D* Center = ConfirmedCardCenters.Find(RuntimeId))
		{
			OutAbsoluteCenter = *Center;
			return true;
		}
		return false;
	}

	virtual void OnNativeCardTransitionAccepted(int32 RuntimeId) override
	{
		ConfirmedCardCenters.Remove(RuntimeId);
		SetPlayedCardSelectionHidden(true);
	}

	virtual void OnNativeCardTransitionEnded(int32 RuntimeId, bool bCancelled) override
	{
		SetPlayedCardSelectionHidden(!ConfirmedCardCenters.IsEmpty());
	}

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

	// G0-C persistent visual host. It remains dormant for production Selection in
	// G4, but the generic source resolver can consume an exact test/ future G5
	// SelectionArea-owned visual without adding destination logic here.
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> SelectionAreaHost = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<class UBorder> SelectionBackdrop = nullptr;
	TMap<TWeakObjectPtr<UCanvasPanelSlot>, FAnchorData> SelectionOriginalLayouts;
	TMap<TWeakObjectPtr<UCanvasPanelSlot>, int32> SelectionOriginalZOrders;
	TMap<int32, FVector2D> ConfirmedCardCenters;
	bool bSelectionVisualMode = false;

	// G4 compatibility shell. Begin delegates to the generic transition layer;
	// the remaining old fields/helpers are dormant and have an explicit G7
	// deletion target once G5 production ownership is proven.
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
