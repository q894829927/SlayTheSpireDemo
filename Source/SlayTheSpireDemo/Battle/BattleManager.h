#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleRequestTypes.h"
#include "../Enemy/EnemyIntent.h"
#include "BattleManager.generated.h"

class ACombatant;
class UBattleAction;
class UBattleActionQueue;
class UBattleEventDispatcher;
class UCardData;
class UCardInstance;
class UDeckRuntime;
class UStatusData;
class UTurnEndedAction;
struct FBattleReadSnapshot;
enum class EDamageKind : uint8;

UENUM(BlueprintType)
enum class EBattleState : uint8
{
	BattleStart UMETA(DisplayName = "Battle Start"),
	PlayerTurnStarting UMETA(DisplayName = "Player Turn Starting"),
	PlayerTurn UMETA(DisplayName = "Player Turn"),
	PlayerTurnEnding UMETA(DisplayName = "Player Turn Ending"),
	EnemyTurn UMETA(DisplayName = "Enemy Turn"),
	EnemyTurnEnding UMETA(DisplayName = "Enemy Turn Ending"),
	Victory UMETA(DisplayName = "Victory"),
	Defeat UMETA(DisplayName = "Defeat"),
	ResolutionFaulted UMETA(DisplayName = "Resolution Faulted")
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBattleReadStateReady, uint64, uint64);

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

	// Temporary fixed-enemy intent generator input. Once an Intent is committed,
	// EnemyTurn executes the committed Intent rather than reading this value again.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug", meta = (ClampMin = "0"))
	int32 EnemyTestAttackDamage = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug", meta = (ClampMin = "0"))
	int32 PlayerTestBlockAmount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Deck")
	int32 DeckDebugSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Cards")
	TArray<TObjectPtr<UCardData>> DebugStartingDeck;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Status")
	TArray<TObjectPtr<UStatusData>> DebugPhase5AStatuses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Status")
	TArray<TObjectPtr<UStatusData>> DebugPhase5B2Statuses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Battle|Debug|Status")
	TArray<TObjectPtr<UStatusData>> DebugPhase5CStatuses;

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

	// Legacy Blueprint/debug wrapper. Formal UI should call RequestEndPlayerTurn.
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void EndPlayerTurn();

	FGameplayValidationResult QueryCardPlayability(const UCardInstance* Card) const;
	FGameplayValidationResult QueryPlayCard(const UCardInstance* Card, const ACombatant* RequestedTarget) const;
	FGameplayRequestResult RequestPlayCard(UCardInstance* Card, ACombatant* RequestedTarget);

	FGameplayValidationResult QueryEndPlayerTurn() const;
	FGameplayRequestResult RequestEndPlayerTurn();

	void GetLegalTargetsForCard(const UCardInstance* Card, TArray<ACombatant*>& OutTargets) const;

	// Raw coherent gameplay snapshot used by existing runtime/tests. It preserves
	// the committed Intent plan but does not derive player-facing damage display.
	bool TryBuildReadSnapshot(FBattleReadSnapshot& OutSnapshot) const;

	// Formal UI/ViewModel snapshot boundary. It starts from the same coherent
	// gameplay snapshot and enriches the committed Intent with a gameplay-derived
	// current-state value by reusing the Damage Modifier Pipeline. The value is not
	// a guarantee of damage at a future EnemyTurn after intervening reactions.
	bool TryBuildPlayerFacingReadSnapshot(FBattleReadSnapshot& OutSnapshot) const;
	const FEnemyIntent& GetCommittedEnemyIntent() const;

	bool CanSpendEnergy(int32 Amount) const;
	bool TrySpendEnergy(int32 Amount);
	uint64 AllocateRuntimeSequence();

	// UI/ViewModel-facing stable-read notification. It is battle-scoped and
	// revision-scoped; Queue-level OnResolutionIdle is intentionally not the public
	// presentation boundary. Publication is deferred by at least one CoreTicker
	// turn after Queue settlement so it cannot re-enter a public Request before
	// that Request has returned AcceptedForResolution to its caller.
	FOnBattleReadStateReady OnReadStateReady;

	// Internal owner callbacks invoked only by this BattleManager's ActionQueue
	// after a healthy PumpQueue has fully exited or after fault state has committed.
	// These are public solely to keep the Queue->owner bridge explicit; Widgets
	// must never call them.
	void NotifyActionQueueResolutionIdle(UBattleActionQueue* SettledQueue);
	void NotifyActionQueueResolutionFaultSettled(UBattleActionQueue* FaultedQueue);

	// Narrow runtime dependency bridge used while BattleManager still owns the
	// battle-scoped dispatcher and authoritative combatant references. Action and
	// card-effect code receives the returned references explicitly; it does not
	// search the world or own trigger-source membership.
	bool TryBuildEventDispatchContext(
		UBattleEventDispatcher*& OutDispatcher,
		TArray<ACombatant*>& OutCombatants
	) const
	{
		OutDispatcher = EventDispatcher.Get();
		OutCombatants.Reset();
		if (OutDispatcher == nullptr || Player.Get() == nullptr || Enemy.Get() == nullptr)
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
#endif

private:
	friend class UTurnEndedAction;

	void StartOpeningHand();
	void StartPlayerTurn();
	void CompletePlayerTurnStart();
	void StartEnemyTurn();
	void CommitNextEnemyIntent();
	FEnemyIntent ChooseNextEnemyIntent() const;

	void HandleActionQueueEmpty();
	void HandleActionQueueResolutionFaulted(const FString& Reason, int32 ExecutedCount, UBattleAction* LastAction);
	void HandleTurnEndedActionExecution(ACombatant* TurnOwner, UBattleActionQueue* Queue);
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

	UPROPERTY(Transient)
	TObjectPtr<UBattleActionQueue> ActionQueue = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleEventDispatcher> EventDispatcher = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> DeckRuntime = nullptr;

	FEnemyIntent CommittedEnemyIntent;
	uint64 BattleId = 0;
	uint64 StateRevision = 0;
	uint64 NextRuntimeSequence = 1;
	uint64 LastPublishedBattleId = 0;
	uint64 LastPublishedReadStateRevision = 0;
	bool bReadStateReadyPublishScheduled = false;

#if WITH_DEV_AUTOMATION_TESTS
	bool bForceInvalidPlayerEndBatchForTesting = false;
	bool bForceInvalidEnemyTurnBatchForTesting = false;
	EBattleState StateBeforeLastResolutionFaultForTesting = EBattleState::BattleStart;
#endif
};
