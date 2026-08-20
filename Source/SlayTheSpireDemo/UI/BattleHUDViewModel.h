#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleHUDTypes.h"
#include "BattleHUDViewModel.generated.h"

class ABattleManager;
class ACombatant;
class UCardInstance;
struct FBattleReadSnapshot;
enum class EGameplayRequestFailureReason : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBattleHUDViewModelChanged);

UCLASS(BlueprintType)
class SLAYTHESPIREDEMO_API UBattleHUDViewModel : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Battle HUD")
	bool Initialize(ABattleManager* InBattleManager);

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

	// Presentation-only identity for a gameplay-validated target that is
	// submitted through explicit confirmation rather than target selection.
	// Self-target cards use this to highlight the Player without exposing a
	// clickable public target or changing the RequestPlayCard contract.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Selection")
	FName PendingConfirmationTargetPresentationId = NAME_None;

protected:
	virtual void BeginDestroy() override;

private:
	void HandleReadStateReady(uint64 InBattleId, uint64 InStateRevision);
	bool PullAndApplySnapshot(bool bResetInteraction);
	void ApplySnapshot(const FBattleReadSnapshot& Snapshot, bool bResetInteraction);
	void RebuildHandViews(const FBattleReadSnapshot& Snapshot);
	void RebuildLegalTargets(UCardInstance* Card);
	bool SubmitSelectedCard(ACombatant* Target);
	void SetResolving();
	void ClearSelectionInternal();
	void SetFeedback(EGameplayRequestFailureReason Reason);
	void ClearFeedback();
	void BroadcastChanged();
	bool CanAcceptSelectionInput() const;
	UCardInstance* FindHandCardByRuntimeId(int32 RuntimeId) const;
	ACombatant* FindLegalTargetById(int32 TargetId) const;

	TWeakObjectPtr<ABattleManager> BattleManager;
	TWeakObjectPtr<ACombatant> PendingConfirmationTarget;
	TArray<TWeakObjectPtr<ACombatant>> LegalTargetObjects;
	TArray<TWeakObjectPtr<UCardInstance>> CachedHandObjects;
};
