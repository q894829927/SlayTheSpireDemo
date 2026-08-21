#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleHUDTypes.h"
#include "BattleHUDViewModel.generated.h"

class ABattleManager;
class ACombatant;
class UCardInstance;
struct FPresentationStateSnapshot;
enum class EBattleState : uint8;
enum class EGameplayRequestFailureReason : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBattleHUDViewModelChanged);

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

	// Presentation-only lookup over the current gameplay-provided legal-target
	// bindings. This does not grant permission or replace Request revalidation.
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

	// Only the newest displayed BattleId/Revision may rebuild weak runtime
	// bindings used to forward a new formal Request.
	bool RefreshLiveInputBindingsIfCaughtUp();
	void EnterPresentationUnavailable(const FText& Reason);
	bool IsPresentationDisplayOwned() const;

	UPROPERTY(BlueprintAssignable, Category = "Battle HUD")
	FBattleHUDViewModelChanged OnChanged;

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

protected:
	virtual void BeginDestroy() override;

private:
	void HandleReadStateReady(uint64 InBattleId, uint64 InStateRevision);
	bool ApplyLatestFrozenBaselineAndRefresh(bool bResetInteraction);
	void RebuildLegalTargets(UCardInstance* Card);
	bool SubmitSelectedCard(ACombatant* Target);
	void SetResolving();
	void ClearSelectionInternal();
	void ClearLiveInputBindings();
	void SetFeedback(EGameplayRequestFailureReason Reason);
	void ClearFeedback();
	void BroadcastChanged();
	bool CanAcceptSelectionInput() const;
	bool IsLiveBindingCurrent() const;
	const FBattleHUDCardView* FindDisplayedCardByRuntimeId(int32 RuntimeId) const;
	UCardInstance* FindHandCardByRuntimeId(int32 RuntimeId) const;
	ACombatant* FindLegalTargetById(int32 TargetId) const;

	TWeakObjectPtr<ABattleManager> BattleManager;
	TMap<int32, TWeakObjectPtr<UCardInstance>> LiveCardBindings;
	TMap<FName, TWeakObjectPtr<ACombatant>> LiveCombatantBindings;
	TArray<TWeakObjectPtr<ACombatant>> LegalTargetObjects;
	int64 LiveBindingBattleId = 0;
	int64 LiveBindingStateRevision = 0;
	EBattleState DisplayedBattleState;
	bool bDisplayedSnapshotCanEndTurn = false;
	bool bPresentationDisplayOwned = false;
};
