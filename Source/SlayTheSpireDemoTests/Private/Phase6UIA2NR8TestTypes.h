#pragma once

#include "CoreMinimal.h"
#include "UI/BattleCardWidget.h"
#include "UI/BattleHUDWidget.h"
#include "Phase6UIA2NR8TestTypes.generated.h"

class UHorizontalBox;
class UImage;
class UOverlay;
class UButton;
class UBattleHUDViewModel;
class URichTextBlock;
class UTextBlock;
class UWorld;
class UBattleHandFanPanel;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR8CardProbe : public UBattleCardWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UButton> TestButton = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TestName = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TestCost = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URichTextBlock> TestDescription = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TestType = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TestArt = nullptr;
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR8HUDProbe : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
	void SetTestWorld(UWorld* InWorld);
	void ReconcileHandForTesting() { RefreshHand(); }
	void SetViewModelForTesting(UBattleHUDViewModel* InViewModel) { ViewModel = InViewModel; }
	void ConfigureCardSurfaces(
		UHorizontalBox* InHand,
		UOverlay* InPlayArea,
		UTextBlock* InDrawCount,
		UTextBlock* InDiscardCount,
		UTextBlock* InEnergy);

	virtual UWorld* GetWorld() const override;

	bool IsLocalPresentationActive() const { return HasActiveNativePresentation(); }
	bool IsLocalFinishTimerSet() const { return HasNativePresentationFinishTimer(); }
	FPresentationPlaybackToken ActiveLocalToken() const { return GetActiveNativePresentationToken(); }
	int32 ActiveCardKindForTesting() const
	{
		return static_cast<int32>(GetActiveNativeCardPresentationKind());
	}
	UBattleCardWidget* PlayedCardForTesting() const { return GetNativePlayedCardWidget(); }
	UBattleCardWidget* DrawnCardForTesting() const { return GetNativeDrawnCardWidget(); }
	UBattleCardWidget* ZoneCardForTesting() const { return GetNativeZoneCardWidget(); }
	UBattleCardWidget* HistoricalCardForTesting() const
	{
		return GetNativeHistoricalHandCardWidget();
	}
	bool DrawAnimationInitializedForTesting() const
	{
		return IsNativeCardAnimationInitialized();
	}

	bool InvokeBeginDirectForTesting(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	void InvokeFinishForTesting(const FPresentationPlaybackToken& Token);
	void InvokeCancelForTesting(const FPresentationPlaybackToken& Token);
	void InvokeNativeTickForTesting(float DeltaSeconds);
	void InvokeNativeDestructForTesting();
	void ConfigureFanForTesting(UBattleHandFanPanel* Fan);
	void PointerForTesting(FVector2D Pointer) { UpdatePointerCardVisuals(Pointer); }
	bool ConfirmPointerForTesting() { return ConfirmPointerCard(); }
	TOptional<FCardPlayVisualOrigin> OriginForTesting(int32 RuntimeId) const { return CaptureCardPlayVisualOrigin(RuntimeId); }
	bool BeginDragForTesting(int32 RuntimeId, FVector2D Position, uint32 Pointer = 0) { return BeginHandCardDrag(RuntimeId, Position, Pointer); }
	bool MoveDragForTesting(FVector2D Position, bool bLeftDown = true, uint32 Pointer = 0) { return UpdateHandCardDrag(Position, bLeftDown, Pointer); }
	bool ReleaseDragForTesting(FVector2D Position, uint32 Pointer = 0) { return ReleaseHandCardDrag(Position, Pointer); }
	void ConfigureDragControlsForTesting(UButton* EndTurn, UButton* Cancel, UButton* Confirm)
	{ Btn_EndTurn = EndTurn; Btn_Cancel = Cancel; Btn_Confirm = Confirm; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UWorld> TestWorld = nullptr;
};
