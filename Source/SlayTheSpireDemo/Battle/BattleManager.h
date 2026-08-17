#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleManager.generated.h"

class ACombatant;
class UBattleActionQueue;
class UCardData;
class UCardInstance;
class UDeckRuntime;
class UStatusData;
enum class EDamageKind : uint8;

UENUM(BlueprintType)
enum class EBattleState : uint8
{
	BattleStart UMETA(DisplayName = "Battle Start"),
	PlayerTurn UMETA(DisplayName = "Player Turn"),
	EnemyTurn UMETA(DisplayName = "Enemy Turn"),
	Victory UMETA(DisplayName = "Victory"),
	Defeat UMETA(DisplayName = "Defeat")
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

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void EndPlayerTurn();

	bool CanSpendEnergy(int32 Amount) const;
	bool TrySpendEnergy(int32 Amount);
	uint64 AllocateRuntimeSequence();

private:
	void StartPlayerTurn();
	void StartEnemyTurn();
	void HandleActionQueueEmpty();
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
	bool IsActionQueueBusy() const;

	UPROPERTY(Transient)
	TObjectPtr<UBattleActionQueue> ActionQueue = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> DeckRuntime = nullptr;

	uint64 NextRuntimeSequence = 1;
};
