#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Presentation/PresentationTypes.h"
#include "BattleHUDWidgetBase.generated.h"

class UBattleHUDViewModel;
class UBattlePresentationController;

UCLASS(Abstract, Blueprintable)
class SLAYTHESPIREDEMO_API UBattleHUDWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void SetViewModel(UBattleHUDViewModel* InViewModel);

	void SetPresentationController(UBattlePresentationController* InController);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool SelectCard(int32 RuntimeId);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	void CancelSelection();

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool SelectTarget(int32 TargetId);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool ConfirmSelectedCard();

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool EndTurn();

	// Converts an immutable committed-presentation card snapshot into the existing
	// HUD card DTO used by WBP_BattleCard. The transient presentation copy is never
	// gameplay-playable and carries no live legality state.
	UFUNCTION(BlueprintPure, Category = "Battle Presentation|Card", meta = (DisplayName = "Make Presentation Card View"))
	FBattleHUDCardView MakePresentationCardView(
		const FPresentationCardSnapshot& Snapshot
	) const;

	UFUNCTION(BlueprintPure, Category = "Battle Presentation|Status", meta = (DisplayName = "Make Presentation Status View"))
	FBattleHUDStatusView MakePresentationStatusView(
		const FStatusChangedPresentationPayload& StatusChanged
	) const;

	// Controller-facing wrapper. It tracks the exact Token before entering
	// Blueprint so timeout/collapse/unavailable paths can cancel only the currently
	// offered presentation visual. Returning false means immediate native fallback.
	bool PlayPresentationRecord(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token
	);

	// Blueprint override point used by the controller-facing wrapper above.
	// Return true only when Blueprint actually started asynchronous playback and
	// will later call NotifyPresentationFinished(Token). The native default returns
	// false, providing the A2A missing-callback immediate fallback without asset edits.
	UFUNCTION(BlueprintNativeEvent, Category = "Battle Presentation", meta = (DisplayName = "Play Presentation Record"))
	bool BeginPresentationRecordPlayback(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token
	);
	virtual bool BeginPresentationRecordPlayback_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token
	);

	// Presentation-only visual cancellation hook. Blueprint should stop only the
	// visual/transient state belonging to this Token. It must NOT call
	// NotifyPresentationFinished from this cancellation event. The base class
	// clears its tracked token before dispatching the event, so even a bad/stale
	// Blueprint callback cannot erase a newer visual owner.
	UFUNCTION(BlueprintNativeEvent, Category = "Battle Presentation", meta = (DisplayName = "Cancel Presentation Record Playback"))
	void CancelPresentationRecordPlayback(const FPresentationPlaybackToken& Token);
	virtual void CancelPresentationRecordPlayback_Implementation(
		const FPresentationPlaybackToken& Token
	);

	// Even if Blueprint accidentally invokes this from inside the playback event,
	// forwarding to the Controller is deferred to the CoreTicker so Controller
	// playback cannot re-enter StartNextRecord through the Blueprint call stack.
	UFUNCTION(BlueprintCallable, Category = "Battle Presentation")
	void NotifyPresentationFinished(const FPresentationPlaybackToken& Token);

	UFUNCTION(BlueprintCallable, Category = "Battle Presentation")
	void SkipPresentation();

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	TObjectPtr<UBattleHUDViewModel> ViewModel = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	TObjectPtr<UBattlePresentationController> PresentationController = nullptr;

protected:
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle HUD", meta = (DisplayName = "Battle HUD View Model Changed"))
	void BP_OnViewModelChanged();

private:
	UFUNCTION()
	void HandleViewModelChanged();

	void ForwardPresentationFinished(const FPresentationPlaybackToken& Token);
	void CancelTrackedPresentationPlayback();
	void ClearTrackedPresentationPlayback(const FPresentationPlaybackToken& Token);

	bool bHasTrackedPresentationPlayback = false;
	FPresentationPlaybackToken TrackedPresentationPlaybackToken;

	// Prevents a normal completion/explicit Skip from being interpreted as a
	// fail-safe visual cancellation when Controller state updates synchronously
	// broadcast the ViewModel change back to this Widget.
	bool bSuppressPresentationCancellation = false;
};
