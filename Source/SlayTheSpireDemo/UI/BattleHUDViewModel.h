#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../Battle/BattleImmediatePreview.h"
#include "../Selection/SelectionTypes.h"
#include "BattleHUDTypes.h"
#include "../Presentation/PresentationTypes.h"
#include "BattleHUDViewModel.generated.h"

class ABattleManager;
class ACombatant;
class UCardInstance;
struct FPendingCardSelectionReadView;
struct FPresentationStateSnapshot;
enum class EBattleState : uint8;
enum class EGameplayRequestFailureReason : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBattleHUDViewModelChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBattleHUDPreviewChanged);
DECLARE_DELEGATE_OneParam(FSynchronizeCardPresentationSurfaces, const TArray<int32>&);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FBattleHUDViewModelNativeChanged,
	EBattleHUDDirtyFlags
);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FBattleHUDCardPresentationOwnershipChanged,
	const TArray<int32>&
);

UCLASS(BlueprintType)
class SLAYTHESPIREDEMO_API UBattleHUDViewModel : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	bool Initialize(
		ABattleManager* InBattleManager,
		bool bInPresentationDisplayOwned = false
	);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	void Shutdown();

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool SelectCardByRuntimeId(int32 RuntimeId);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	void CancelSelection();

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool SelectTargetById(int32 TargetId);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool ConfirmSelectedCard();

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Input")
	bool RequestEndTurn();

	// Wave 1C asynchronous Gameplay selection is intentionally separate from the
	// normal caught-up card-play binding. These helpers query/submit through the
	// Battle selection request facade using RuntimeId only; the ViewModel never
	// stores authoritative candidate UObject pointers.
	bool HasPendingCardSelection() const;

	// Fail-closed authority probe only. Unlike HasPendingCardSelection/TryGet..., this
	// intentionally does not require the displayed Presentation revision to have
	// caught up. It must never be used to expose candidate identities or enable UI;
	// it exists solely to prevent a real pending Gameplay selection from falling
	// through into ordinary card-play input during the boundary catch-up window.
	bool HasAuthoritativePendingCardSelection() const;
	bool TryGetPendingCardSelectionReadView(FPendingCardSelectionReadView& OutView) const;
	bool IsPendingCardSelectionCandidate(int32 RuntimeId) const;
	virtual bool SubmitPendingCardSelectionByRuntimeIds(const TArray<int32>& RuntimeIds);
	bool SubmitPendingCardSelectionByRuntimeId(int32 RuntimeId);
	bool CanConfirmPendingCardSelection() const;
	bool ConfirmPendingCardSelection();
	bool ConfirmPendingCardSelectionWithPresentation(int64 SelectionGeneration);
	bool IsSelectionPresentationSubmitInProgress() const { return bSelectionPresentationSubmitInProgress; }
	void AcceptSelectionPresentationOutcomes(const FPresentationResolutionEnvelope& Envelope);
	// One owning Native HUD commits its surfaces before public multicast observers.
	FSynchronizeCardPresentationSurfaces SynchronizeCardPresentationSurfaces;
	bool IsPendingCardSelectionRuntimeIdSelected(int32 RuntimeId) const;
	int32 GetPendingCardSelectionSelectedCount() const;
	void ClearPendingCardSelectionInputState();
	bool CanCancelPendingCardSelection() const;
	bool SubmitPendingCardSelectionCancel();

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Preview")
	bool SetPreviewTargetById(int32 TargetId);

	UFUNCTION(BlueprintCallable, Category = "Battle HUD|Preview")
	void ClearPreviewTarget();

	// Historical compatibility API. The standalone text Preview surface is no
	// longer a production display path; target-specific values render on the card.
	UFUNCTION(BlueprintPure, Category = "Battle HUD|Preview")
	FText GetImmediatePreviewDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Battle HUD|Selection")
	bool TryGetLegalTargetByPresentationId(
		FName PresentationId,
		FBattleHUDTargetView& OutTarget
	) const;

	// Historical display boundary. This function performs value copies only; it
	// does not query BattleManager, CardData/StatusData or mutable runtime objects.
	void ApplyPresentationSnapshot(
		const FPresentationStateSnapshot& Snapshot,
		bool bResetInteraction = true
	);

	bool RefreshLiveInputBindingsIfCaughtUp();
	void EnterPresentationUnavailable(const FText& Reason);
	bool IsPresentationDisplayOwned() const;
	void SetPresentationDisplayOwned(bool bOwned);

	// G0 ownership infrastructure, activated by the production G5 Selection HUD.
	// Native Presentation state only; Gameplay selection/card-zone truth remains
	// owned by BattleManager and the selection facade.
	int64 BeginCardPresentationSelectionLifecycle(int64 SelectionBoundaryRevision);
	bool CancelCardPresentationSelectionLifecycle(int64 SelectionGeneration);
	bool SetPendingCardPresentationSelection(
		int64 SelectionGeneration,
		int32 RuntimeId,
		bool bSelected);
	bool ConfirmCardPresentationSelection(
		int64 SelectionGeneration,
		const TArray<int32>& RuntimeIds);
	bool TryTransferCardPresentationOwnership(
		int64 SelectionGeneration,
		int32 RuntimeId,
		ECardPresentationOwner ExpectedOwner,
		ECardPresentationOwner NewOwner);
	// G6 all-or-nothing ownership transaction. Every member is validated before
	// any mutation; all entries change together and publish one ownership event.
	bool TryTransferCardPresentationOwnershipBatch(
		int64 SelectionGeneration,
		const TArray<int32>& RuntimeIds,
		ECardPresentationOwner ExpectedOwner,
		ECardPresentationOwner NewOwner);
	bool ArmRecordedCardPresentationCompletion(
		int64 SelectionGeneration,
		int64 ResolutionId);
	bool ArmDirectCardPresentationCompletion(
		int64 SelectionGeneration,
		int64 PostConfirmStateRevision);
	void MarkPresentationResolutionCompleted(int64 InBattleId, int64 ResolutionId);
	void ReconcileCardPresentationOwnership();
	ECardPresentationOwner GetCardPresentationOwner(int32 RuntimeId) const;
	bool TryGetCardPresentationOwnershipEntry(
		int32 RuntimeId,
		FCardPresentationOwnershipEntry& OutEntry) const;

	// Payload-bearing Native change path. Native HUD consumers bind here so one
	// synchronous/re-entrant Blueprint OnChanged listener cannot overwrite the
	// dirty descriptor seen by another Native listener.
	FBattleHUDViewModelNativeChanged OnNativeChanged;

	// Independent transient Presentation notification. Select/deselect/Confirm,
	// transition ownership and reconciliation may publish here even when no
	// historical FPresentationStateSnapshot field changed.
	FBattleHUDCardPresentationOwnershipChanged OnCardPresentationOwnershipChanged;

	// Compatibility/debug descriptor for the most recent historical publication.
	// Native HUD code should prefer the payload supplied by OnNativeChanged.
	EBattleHUDDirtyFlags GetLastChangeFlags() const { return LastChangeFlags; }

	// Structural/frozen HUD state and read-facing interaction changes. This
	// payload-free Blueprint event remains for compatibility; Native HUD code uses
	// OnNativeChanged instead.
	UPROPERTY(BlueprintAssignable, Category = "Battle HUD")
	FBattleHUDViewModelChanged OnChanged;

	// A3 transient Preview-only state. This MUST NOT cause formal Hand rebuilding;
	// it exists so a target hover/focus can restyle only the selected card face.
	UPROPERTY(BlueprintAssignable, Category = "Battle HUD|Preview")
	FBattleHUDPreviewChanged OnPreviewChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Revision")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Revision")
	int64 StateRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|State")
	EBattleHUDInteractionState InteractionState = EBattleHUDInteractionState::Resolving;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|State")
	EBattleHUDOutcome Outcome = EBattleHUDOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|State")
	bool bInputLocked = true;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|State")
	bool bCanEndTurn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|State")
	FText LastFeedback;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combat")
	FBattleHUDCombatantView Player;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combat")
	FBattleHUDCombatantView Enemy;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combat")
	int32 Energy = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Combat")
	int32 MaxEnergy = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Cards")
	TArray<FBattleHUDCardView> HandCards;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Cards")
	int32 DrawCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Cards")
	int32 DiscardCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Cards")
	int32 ExhaustCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Intent")
	FBattleHUDIntentView EnemyIntent;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Selection")
	int32 SelectedCardRuntimeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Selection")
	TArray<FBattleHUDTargetView> LegalTargets;

	// PreviewTarget is an explicit transient lifecycle and is intentionally
	// separate from inspection/tooltip state and authoritative target submission.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Preview")
	int32 PreviewTargetId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Preview")
	FName PreviewTargetPresentationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Preview")
	bool bHasImmediatePreview = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Preview")
	FImmediateCardPreview ImmediatePreview;

protected:
	virtual void BeginDestroy() override;

private:
	void HandleReadStateReady(uint64 InBattleId, uint64 InStateRevision);
	bool ApplyLatestFrozenBaselineAndRefresh(bool bResetInteraction);
	void RebuildLegalTargets(UCardInstance* Card);
	bool SubmitSelectedCard(ACombatant* Target);
	bool TryBuildImmediatePreviewForTarget(ACombatant* Target, int32 TargetId);
	void ClearImmediatePreviewInternal();
	void SetResolving();
	void ClearSelectionInternal();
	void ClearLiveInputBindings();
	void SetFeedback(EGameplayRequestFailureReason Reason);
	void ClearFeedback();
	void BroadcastChanged(EBattleHUDDirtyFlags DirtyFlags = EBattleHUDDirtyFlags::All);
	void BroadcastPreviewChanged();
	bool CanAcceptSelectionInput() const;
	bool IsLiveBindingCurrent() const;
	const FBattleHUDCardView* FindDisplayedCardByRuntimeId(int32 RuntimeId) const;
	UCardInstance* FindHandCardByRuntimeId(int32 RuntimeId) const;
	ACombatant* FindLegalTargetById(int32 TargetId) const;

	bool IsCardPresentationCompletionWatermarkReached(
		const FCardPresentationOwnershipEntry& Entry) const;
	TArray<int32> ReconcileCardPresentationOwnershipInternal();
	void PublishCardPresentationOwnershipChanged(const TArray<int32>& ChangedRuntimeIds);
	void ResetCardPresentationOwnershipState(bool bNotify);
	void ResolveSelectionPresentationReadEdge(uint64 InBattleId, uint64 InStateRevision);
	void ApplySelectionOutcomeReceipt(const FSelectionPresentationOutcomeReceipt& Receipt);
	bool bSelectionPresentationSubmitInProgress = false;
	bool bSelectionCorrelationFailed = false;
	int64 SubmittingSelectionBoundary = 0;
	TArray<FSelectionPresentationOutcomeReceipt> DeferredSelectionReceipts;
	UPROPERTY(Transient)
	FPresentationStateSnapshot DeferredSelectionSnapshot;
	bool bHasDeferredSelectionSnapshot = false;
	bool bDeferredSelectionResetInteraction = false;
	uint64 DeferredSelectionReadBattleId = 0;
	uint64 DeferredSelectionReadRevision = 0;

	TWeakObjectPtr<ABattleManager> BattleManager;
	TMap<int32, TWeakObjectPtr<UCardInstance>> LiveCardBindings;
	TMap<FName, TWeakObjectPtr<ACombatant>> LiveCombatantBindings;
	TArray<TWeakObjectPtr<ACombatant>> LegalTargetObjects;
	TArray<int32> PendingCardSelectionRuntimeIds;
	FPendingSelectionRequestIdentity PendingCardSelectionRequestIdentity;
	FName PendingCardSelectionSource = NAME_None;
	int32 PendingCardSelectionRequiredCount = 0;
	TArray<int32> PendingCardSelectionCandidateRuntimeIds;
	int64 LiveBindingBattleId = 0;
	int64 LiveBindingStateRevision = 0;
	EBattleState DisplayedBattleState = static_cast<EBattleState>(0);
	bool bDisplayedSnapshotCanEndTurn = false;
	bool bPresentationDisplayOwned = false;
	EBattleHUDDirtyFlags LastChangeFlags = EBattleHUDDirtyFlags::All;

	// G0-C transient Presentation ownership storage. This is deliberately not
	// copied from FPresentationStateSnapshot and is never Gameplay authority.
	TMap<int32, FCardPresentationOwnershipEntry> CardPresentationOwnershipEntries;
	int64 NextCardPresentationSelectionGeneration = 1;
	int64 ActiveCardPresentationSelectionGeneration = 0;
	int64 ActiveCardPresentationSelectionBattleId = 0;
	int64 ActiveCardPresentationSelectionBoundaryRevision = 0;
	int64 CompletedPresentationResolutionBattleId = 0;
	TSet<int64> CompletedPresentationResolutionIds;
};