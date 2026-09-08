#pragma once

#include "CoreMinimal.h"
#include "BattleHUDWidget.h"
#include "BattleHUDSelectionWidget.generated.h"

class UBattleCardWidget;

/**
 * Shared Native Presentation surface for player card-selection interactions.
 *
 * This class owns only reusable Selection UI/presentation behavior:
 * explicit Confirm/Cancel routing and selected-card visual transfer according
 * to committed zone facts. It contains no CardId/Effect-specific branches.
 */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDSelectionWidget : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
#if WITH_DEV_AUTOMATION_TESTS
	// Exact-token presentation lifecycle probe, matching the existing Native HUD
	// test pattern. Production playback still completes through the timer path.
	void FinishSharedHandToDrawPilePresentationForTesting(
		const FPresentationPlaybackToken& ExpectedToken)
	{
		FinishSharedHandToDrawPilePresentation(ExpectedToken);
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
