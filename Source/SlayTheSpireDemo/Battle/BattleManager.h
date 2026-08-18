#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
enum class EDamageKind : uint8;

UENUM(BlueprintType)
enum class EBattleState : uint8
{
	BattleStart UMETA(DisplayName = "Battle Start"),
	PlayerTurn UMETA(DisplayName = "Player Turn"),
	PlayerTurnEnding UMETA(DisplayName = "Player Turn Ending"),
	EnemyTurn UMETA(DisplayName = "Enemy Turn"),
	EnemyTurnEnding UMETA(DisplayName = "Enemy Turn Ending"),
	Victory UMETA(DisplayName = "Victory"),
	Defeat UMETA(DisplayName = "Defeat"),
	ResolutionFaulted UMETA(DisplayName = "Resolution Faulted")
};

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

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void EndPlayerTurn();

	bool CanSpendEnergy(int32 Amount) const;
	bool TrySpendEnergy(int32 Amount);
	uint64 AllocateRuntimeSequence();

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
	void SetForceInvalidPlayerEndBatchForTesting(bool bForceInvalid);
	void SetForceInvalidEnemyTurnBatchForTesting(bool bForceInvalid);
	EBattleState GetStateBeforeLastResolutionFaultForTesting() const;
#endif

private:
	friend class UTurnEndedAction;

	void StartPlayerTurn();
	void StartEnemyTurn();
	void HandleActionQueueEmpty();
	void HandleActionQueueResolutionFaulted(const FString& Reason, int32 ExecutedCount, UBattleAction* LastAction);
	void HandleTurnEndedActionExecution(ACombatant* TurnOwner, UBattleActionQueue* Queue);
	void CheckBattleResult();

	void QueueDamageAction(ACombatant* Source, ACombatant* Target, int32 BaseAmount, EDamageKind DamageKind);
	void QueueGainBlockAction(ACombatant* Source, ACombatant* Target, int32 BaseAmount);
	void QueueDrawCardAction();
	void QueueDiscardCardAction(UCardInstance* Card);
	void QueuePlayCardAction(UCardInstance* Card, ACombatant* Target);
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

	uint64 NextRuntimeSequence = 1;

#if WITH_DEV_AUTOMATION_TESTS
	bool bForceInvalidPlayerEndBatchForTesting = false;
	bool bForceInvalidEnemyTurnBatchForTesting = false;
	EBattleState StateBeforeLastResolutionFaultForTesting = EBattleState::BattleStart;
#endif
};
