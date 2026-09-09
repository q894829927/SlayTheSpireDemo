#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Presentation/PresentationTypes.h"
#include "BattleHUDWidgetBase.generated.h"

class UBattleHUDViewModel;
class UBattlePresentationController;

struct SLAYTHESPIREDEMO_API FTrackedPresentationPlaybackUnit
{
	FPresentationPlaybackToken Token;
	FPresentationGroupTag Group;
	TArray<int32> RecordIndices;

	bool IsValid() const
	{
		if (!Token.IsValid())
		{
			return false;
		}
		if (Token.UnitKind == EPresentationPlaybackUnitKind::SingleRecord)
		{
			return Token.GroupId == 0;
		}
		return Group.IsValid()
			&& Token.GroupId == Group.GroupId
			&& RecordIndices.Num() == Group.ExpectedMemberCount;
	}
};

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

	// Controller-facing SingleRecord wrapper. It tracks the exact Token before
	// entering Blueprint so timeout/recovery/unavailable paths can cancel only the
	// currently offered presentation visual. Returning false means immediate native fallback.
	bool PlayPresentationRecord(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token,
		int32 RecordIndex = INDEX_NONE
	);

	bool PlayPresentationGroup(
		const TArray<FPresentationRecord>& Records,
		const FPresentationGroupTag& Group,
		const TArray<int32>& RecordIndices,
		const FPresentationPlaybackToken& Token
	);

	// G6 upgrades the already-tracked leader offer before its concrete SingleRecord
	// Begin has started. If Group visual preflight declines, the exact original
	// SingleRecord tracked owner is restored with no cancellation side effect.
	bool TryReplaceTrackedPresentationRecordWithGroup(
		const FPresentationPlaybackToken& ExpectedSingleRecordToken,
		const TArray<FPresentationRecord>& Records,
		const FPresentationGroupTag& Group,
		const TArray<int32>& RecordIndices,
		const FPresentationPlaybackToken& GroupToken
	);

	// G6 completion uses a distinct Controller path from the historical G3
	// dormant Group callback, while retaining exact tracked-token deferral.
	void NotifyPresentationGroupFinishedG6(
		const FPresentationPlaybackToken& Token
	);

	// Exact-token cancellation never clears a newer tracked Record/Group unit.
	bool CancelTrackedPresentationPlayback(
		const FPresentationPlaybackToken& ExpectedToken
	);
	bool HasTrackedPresentationPlayback() const { return bHasTrackedPresentationPlayback; }
	FPresentationPlaybackToken GetTrackedPresentationToken() const { return TrackedPresentationPlaybackUnit.Token; }
	EPresentationPlaybackUnitKind GetTrackedPresentationUnitKind() const { return TrackedPresentationPlaybackUnit.Token.UnitKind; }
	TArray<int32> GetTrackedPresentationRecordIndices() const { return TrackedPresentationPlaybackUnit.RecordIndices; }

	// Blueprint override point used by the controller-facing SingleRecord wrapper.
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

	// C++-only Group override point. Returning false means zero Group visual
	// ownership was accepted.
	virtual bool BeginPresentationGroupPlayback(
		const TArray<FPresentationRecord>& Records,
		const FPresentationGroupTag& Group,
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

	virtual void CancelPresentationGroupPlayback(
		const FPresentationGroupTag& Group,
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

	// Native extension point for concrete HUD implementations. The base default
	// preserves the sealed Legacy WBP contract by forwarding to BP_OnViewModelChanged.
	virtual void NativeOnBattleHUDViewModelChanged();

	// Exact dirty payload for the Native change currently being dispatched. It is
	// scoped to the synchronous Native delegate callback and is never sourced from
	// mutable Blueprint OnChanged listener ordering.
	EBattleHUDDirtyFlags GetCurrentNativeViewModelDirtyFlags() const
	{
		return CurrentNativeViewModelDirtyFlags;
	}

	// Development diagnostic only. Called when a presentation Record is declined
	// by the native/Blueprint playback surface before Controller immediate fallback.
	// It must not mutate ViewModel, Gameplay, presentation state or Widget ownership.
	virtual void LogPresentationRecordRejection(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Battle HUD", meta = (DisplayName = "Battle HUD View Model Changed"))
	void BP_OnViewModelChanged();

private:
	void HandleNativeViewModelChanged(EBattleHUDDirtyFlags DirtyFlags);

	void ForwardPresentationFinished(const FPresentationPlaybackToken& Token);
	void CancelTrackedPresentationPlayback();
	bool ClearTrackedPresentationPlayback(const FPresentationPlaybackToken& Token);
	void DispatchTrackedPresentationCancellation(
		const FTrackedPresentationPlaybackUnit& Unit
	);

	bool bHasTrackedPresentationPlayback = false;
	FTrackedPresentationPlaybackUnit TrackedPresentationPlaybackUnit;
	EBattleHUDDirtyFlags CurrentNativeViewModelDirtyFlags = EBattleHUDDirtyFlags::All;

	// Prevents a normal completion/explicit Skip from being interpreted as a
	// fail-safe visual cancellation when Controller state updates synchronously
	// broadcast the ViewModel change back to this Widget.
	bool bSuppressPresentationCancellation = false;
};
