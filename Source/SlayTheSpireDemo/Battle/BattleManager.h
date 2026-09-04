#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "BattleRequestTypes.h"
#include "BattleState.h"
#include "../Enemy/EnemyIntent.h"
#include "../Events/BattleEventDispatcher.h"
#include "../Presentation/BattlePresentationRecorder.h"
#include "../Presentation/PresentationTypes.h"
#include "BattleManager.generated.h"

class ACombatant;
class UBattleAction;
class UBattleActionQueue;
class UCardData;
class UCardInstance;
class UDeckRuntime;
class URelicContainer;
class URelicData;
class UStatusData;
class UTurnEndedAction;
struct FBattleReadSnapshot;
struct FEnergyCommitResult;
struct FImmediateCardPreview;
enum class EDamageKind : uint8;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBattleReadStateReady, uint64, uint64);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnPresentationResolutionReady,
	const FPresentationResolutionEnvelope&
);

UCLASS(Blueprintable)
class SLAYTHESPIREDEMO_API ABattleManager : public AActor
{
	GENERATED_BODY()

public:
	ABattleManager();

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Battle|References")
	TObjectPtr<ACombatant> Player = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Battle|References")
	TObjectPtr<ACombatant> Enemy = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle|State")
	EBattleState BattleState = EBattleState::BattleStart;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Battle|Energy")
	int32 Energy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Energy", meta = (ClampMin = "0"))
	int32 MaxEnergy = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Rules|Hand", meta = (ClampMin = "0"))
	int32 OpeningHandDrawCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Rules|Hand", meta = (ClampMin = "0"))
	int32 PlayerTurnDrawCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug", meta = (ClampMin = "0"))
	int32 PlayerTestAttackDamage = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug", meta = (ClampMin = "0"))
	int32 EnemyTestAttackDamage = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug", meta = (ClampMin = "0"))
	int32 PlayerTestBlockAmount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Deck")
	int32 DeckDebugSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Cards")
	TArray<TObjectPtr<UCardData>> DebugStartingDeck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Relics")
	TArray<TObjectPtr<URelicData>> DebugStartingRelics;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Status")
	TArray<TObjectPtr<UStatusData>> DebugPhase5AStatuses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Status")
	TArray<TObjectPtr<UStatusData>> DebugPhase5B2Statuses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Status")
	TArray<TObjectPtr<UStatusData>> DebugPhase5CStatuses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Presentation")
	bool bEnableCommittedPresentationRecording = true;

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StartBattle();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug")
	void TestAttack();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug")
	void TestGainBlock();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug")
	void TestActionQueueOrder();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug|Deck")
	void TestDrawCard();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug|Deck")
	void TestDiscardCard();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug|Cards")
	void TestPlayFirstCard();

	// Focused visual/debug hook for the upgrade refactor. It upgrades the first
	// runtime Hand card through the real UUpgradeCardAction boundary rather than
	// mutating UCardData or UCardInstance directly.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Battle|Debug|Cards")
	void TestUpgradeFirstHandCard();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug|Status")
	void TestApplyPhase5AStatuses();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug|Status")
	void TestApplyPhase5B1Strength();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug|Damage")
	void TestPhase5B1EffectDamage();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug|Status")
	void TestApplyPhase5B2DamageStatuses();

	UFUNCTION(BlueprintCallable, Category = "Battle|Debug|Block")
	void TestPhase5CBlockPipeline();

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void EndPlayerTurn();

	FGameplayValidationResult QueryCardPlayability(const UCardInstance* Card) const;
	FGameplayValidationResult QueryPlayCard(const UCardInstance* Card, const ACombatant* RequestedTarget) const;
	FGameplayRequestResult RequestPlayCard(UCardInstance* Card, ACombatant* RequestedTarget);
	bool TryBuildImmediateCardPreview(
		const UCardInstance* Card,
		const ACombatant* Target,
		FImmediateCardPreview& OutPreview
	) const;

	FGameplayValidationResult QueryEndPlayerTurn() const;
	FGameplayRequestResult RequestEndPlayerTurn();

	void GetLegalTargetsForCard(const UCardInstance* Card, TArray<ACombatant*>& OutTargets) const;
	bool TryBuildReadSnapshot(FBattleReadSnapshot& OutSnapshot) const;
	bool TryBuildPlayerFacingReadSnapshot(FBattleReadSnapshot& OutSnapshot) const;
	const FEnemyIntent& GetCommittedEnemyIntent() const;

	bool CanSpendEnergy(int32 Amount) const;
	bool TrySpendEnergy(int32 Amount);
	bool IsAuthoritativeDeckRuntime(const UDeckRuntime* Deck) const;
	uint64 AllocateRuntimeSequence();
	URelicContainer* GetPlayerRelicContainer();
	const URelicContainer* GetPlayerRelicContainer() const;

	bool TryResolveCombatantPresentationId(
		const ACombatant* Combatant,
		FName& OutPresentationId
	) const;

	bool TryGetLatestFrozenPresentationBaseline(FPresentationStateSnapshot& OutSnapshot) const;
	bool IsPresentationAvailable() const;
	FText GetPresentationUnavailableReason() const;
	bool IsCommittedPresentationRecordingEnabledForBattle() const;
	uint64 GetLatestFrozenPresentationBaselineResolutionId() const;

	FOnPresentationResolutionReady OnPresentationResolutionReady;
	FOnBattleReadStateReady OnReadStateReady;

	bool TryBuildEventDispatchContext(
		UBattleEventDispatcher*& OutDispatcher,
		TArray<ACombatant*>& OutCombatants
	)
	{
		OutDispatcher = EventDispatcher.Get();
		OutCombatants.Reset();
		if (OutDispatcher == nullptr || Player.Get() == nullptr || Enemy.Get() == nullptr)
		{
			return false;
		}
		if (!OutDispatcher->BindBattleContext(this))
		{
			return false;
		}
		OutCombatants.Add(Player.Get());
		OutCombatants.Add(Enemy.Get());
		return true;
	}

#if WITH_DEV_AUTOMATION_TESTS
	UBattleActionQueue* GetActionQueueForTesting() const;
	UDeckRuntime* GetDeckRuntimeForTesting() const;
	void SetForceInvalidPlayerEndBatchForTesting(bool bForceInvalid);
	void SetForceInvalidEnemyTurnBatchForTesting(bool bForceInvalid);
	void SetCommittedEnemyAttackIntentForTesting(int32 BaseAmount);
	EBattleState GetStateBeforeLastResolutionFaultForTesting() const;
	void CheckBattleResultForTesting();
	void FlushScheduledReadStateReadyForTesting();

	UBattlePresentationRecorder* GetPresentationRecorderForTesting() const;
	FPresentationRecordWriter GetActivePresentationRecordWriterForTesting() const;
	bool BeginSystemPresentationResolutionForTesting();
	bool SealActivePresentationResolutionForTesting();
	int32 GetPendingPresentationDeliveryCountForTesting() const;
	uint64 GetLastSealedPresentationResolutionIdForTesting() const;
	void SetForcePresentationFreezeFailureForTesting(bool bForce);

	void SetForceNextEnergySpendFailureForTesting(bool bForce)
	{
		bForceNextEnergySpendFailureForTesting = bForce;
	}

	bool ConsumeForceNextEnergySpendFailureForTesting()
	{
		const bool bForced = bForceNextEnergySpendFailureForTesting;
		bForceNextEnergySpendFailureForTesting = false;
		return bForced;
	}
#endif

private:
	friend class UTurnEndedAction;

	void StartOpeningHand();
	void StartPlayerTurn();
	void CompletePlayerTurnStart();
	void StartEnemyTurn();
	void CommitNextEnemyIntent();
	FEnemyIntent ChooseNextEnemyIntent() const;
	bool InitializeRelicsForBattle();

	void HandleActionQueueEmpty();
	void HandleActionQueueResolutionIdle();
	void HandleActionQueueResolutionFaulted(const FString& Reason, int32 ExecutedCount, UBattleAction* LastAction);
	void HandleTurnEndedActionExecution(
		ACombatant* TurnOwner,
		UBattleActionQueue* Queue,
		const FPresentationRecordWriter& PresentationRecordWriter
	);
	void CheckBattleResult();
	void ScheduleReadStateReadyPublish();
	bool HandleScheduledReadStateReady(float DeltaTime);
	void TryPublishReadStateReady();

	FGameplayValidationResult ValidatePlayerCommandBase() const;
	FGameplayValidationResult ValidateCardPlayBase(const UCardInstance* Card) const;
	FGameplayValidationResult ValidatePlayCard(const UCardInstance* Card, const ACombatant* RequestedTarget) const;

	bool BuildDrawActionBatch(int32 DrawCount, TArray<UBattleAction*>& OutActions);
	bool BuildPlayerTurnEndBatch(TArray<UBattleAction*>& OutActions);
	void AdvanceStateRevision();

	void QueueDamageAction(ACombatant* Source, ACombatant* Target, int32 BaseAmount, EDamageKind DamageKind);
	void QueueGainBlockAction(ACombatant* Source, ACombatant* Target, int32 BaseAmount);
	void QueueDrawCardAction();
	void QueueDiscardCardAction(UCardInstance* Card);
	void QueueApplyStatusAction(ACombatant* Source, ACombatant* Target, UStatusData* StatusDefinition, int32 AmountToAdd);

	bool HasValidCombatants() const;
	bool HasValidActionQueue() const;
	bool HasValidDeckRuntime() const;
	bool HasValidEventDispatcher() const;
	bool IsActionQueueBusy() const;

	static void AppendEnergyChangedPresentationRecord(
		const FEnergyCommitResult& CommitResult,
		const FPresentationRecordWriter& Writer);

	bool BeginPresentationResolution(EPresentationResolutionOrigin Origin);
	void AbortPresentationResolution();
	FPresentationRecordWriter GetActivePresentationRecordWriter() const;
	void FinalizePresentationResolutionAtStableBoundary();
	void AppendPresentationResolutionFault(
		const FString& Reason,
		int32 ExecutedCount,
		const UBattleAction* LastAction
	);
	bool TryFreezePresentationStateSnapshot(
		const FBattleReadSnapshot& ReadSnapshot,
		FPresentationStateSnapshot& OutSnapshot
	) const;
	bool ValidateResolvedPresentationIds(FString& OutReason) const;
	void ResetPresentationForBattle();
	void MarkPresentationUnavailable(const FString& Reason);
	void EnqueuePendingPublicPresentation(FPresentationResolutionEnvelope&& Envelope);
	void DrainPendingPublicPresentationDeliveries();
	void FreezeLatestPresentationBaselineWithoutResolution();

	UPROPERTY(Transient)
	TObjectPtr<UBattleActionQueue> ActionQueue = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleEventDispatcher> EventDispatcher = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> DeckRuntime = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URelicContainer> PlayerRelicContainer = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattlePresentationRecorder> PresentationRecorder = nullptr;

	UPROPERTY(Transient)
	TArray<FPresentationResolutionEnvelope> PendingPublicDeliveryQueue;

	UPROPERTY(Transient)
	FPresentationStateSnapshot LatestFrozenPresentationBaseline;

	UPROPERTY(Transient)
	FText PresentationUnavailableReason;

	FEnemyIntent CommittedEnemyIntent;
	uint64 BattleId = 0;
	uint64 StateRevision = 0;
	uint64 NextRuntimeSequence = 1;
	uint64 LastPublishedBattleId = 0;
	uint64 LastPublishedReadStateRevision = 0;
	uint64 LastSealedPresentationResolutionId = 0;
	uint64 LastDeliveredPresentationResolutionId = 0;
	uint64 LatestFrozenPresentationBaselineResolutionId = 0;
	bool bHasLatestFrozenPresentationBaseline = false;
	bool bPresentationAvailable = true;
	bool bLastPublishedPresentationAvailable = true;
	bool bCommittedPresentationRecordingEnabledForBattle = true;
	bool bReadStateReadyPublishScheduled = false;
	FTSTicker::FDelegateHandle ReadStateReadyTickerHandle;

	static constexpr int32 MaxPendingPublicPresentationEnvelopes = 8;

#if WITH_DEV_AUTOMATION_TESTS
	bool bForceInvalidPlayerEndBatchForTesting = false;
	bool bForceInvalidEnemyTurnBatchForTesting = false;
	bool bForcePresentationFreezeFailureForTesting = false;
	bool bForceNextEnergySpendFailureForTesting = false;
	EBattleState StateBeforeLastResolutionFaultForTesting = EBattleState::BattleStart;
#endif
};
