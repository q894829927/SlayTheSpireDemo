#pragma once

#include "CoreMinimal.h"
#include "BattleHUDCardTransitionWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "BattleHUDSelectionWidget.generated.h"

USTRUCT()
struct FSelectionAreaVisualState
{
	GENERATED_BODY()
	UPROPERTY(Transient) TObjectPtr<UBattleCardWidget> Card = nullptr;
	int64 BattleId = 0;
	int64 Generation = 0;
	int64 BoundaryRevision = 0;
};

/** Production SelectionArea owner; Gameplay Hand remains a separate historical surface. */
UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDSelectionWidget : public UBattleHUDCardTransitionWidget
{
	GENERATED_BODY()
public:
	virtual bool SelectCard(int32 RuntimeId, bool bAllowFastPresentationCatchUp = true) override;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FSelectionPresentationG5LifecycleTest;
	friend class FSelectionPresentationG5ConfirmTest;
	void FinishSharedHandToDrawPilePresentationForTesting(const FPresentationPlaybackToken& Token) { FinishNativeCardTransitionForTesting(Token); }
	UOverlay* GetSelectionAreaHostForTesting() const { return SelectionAreaHost; }
	void SetSelectionAreaHostForTesting(UOverlay* Host) { SelectionAreaHost = Host; }
#endif
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnBattleHUDViewModelChanged() override;
	virtual bool BeginPresentationRecordPlayback_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token) override;
	virtual void HandleCardPresentationOwnershipChanged(const TArray<int32>& RuntimeIds) override;
	virtual UOverlay* GetCardTransitionSelectionAreaHost() const override { return SelectionAreaHost; }
	virtual void OnNativeCardTransitionAccepted(int32 RuntimeId) override;
	virtual void OnNativeCardTransitionEnded(int32 RuntimeId, bool bCancelled) override;
	// Headless fixtures supply geometry; production requires a laid-out Host.
	virtual FVector2D GetSelectionAreaLayoutSize() const;
private:
	UFUNCTION() void HandleSelectionAwareConfirmClicked();
	UFUNCTION() void HandleSelectionAwareCancelClicked();
	UFUNCTION() void HandleSelectionAreaCardRequested(int32 RuntimeId);
	bool EnsureSelectionAreaHost();
	void RefreshSharedSelectionPresentation();
	void UpdatePendingSelectionAreaLayout();
	void SynchronizeSelectionSurfaces(const TArray<int32>& ChangedIds);
	void ReleaseSelectionAreaVisuals();
	void ClearSharedSelectionControlsAfterSubmit();
	void SetPlayedCardSelectionHidden(bool bHidden);
	void SetSelectionLayoutActive(bool bActive);
	UPROPERTY(Transient) TObjectPtr<UOverlay> SelectionAreaHost = nullptr;
	UPROPERTY(Transient) TObjectPtr<class UBorder> SelectionBackdrop = nullptr;
	UPROPERTY(Transient) TMap<int32, FSelectionAreaVisualState> SelectionVisuals;
	TWeakObjectPtr<UBattleHUDViewModel> SelectionBoundViewModel;
	TMap<TWeakObjectPtr<UCanvasPanelSlot>, FAnchorData> SelectionOriginalLayouts;
	TMap<TWeakObjectPtr<UCanvasPanelSlot>, int32> SelectionOriginalZOrders;
	int64 PendingGeneration = 0;
	int64 PendingBattleId = 0;
	int64 PendingBoundaryRevision = 0;
	bool bSynchronizingSelection = false;
	bool bSelectionAwareDelegatesBound = false;
};
