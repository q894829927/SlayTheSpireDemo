#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleManager.generated.h"

class ACombatant;
class UBattleActionQueue;
class UDeckRuntime;

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

	UFUNCTION(BlueprintCallable, Category = "Battle")
	void EndPlayerTurn();

private:
	void StartPlayerTurn();
	void StartEnemyTurn();
	void HandleActionQueueEmpty();
	void CheckBattleResult();

	void QueueDamageAction(ACombatant* Source, ACombatant* Target, int32 BaseAmount);
	void QueueGainBlockAction(ACombatant* Source, ACombatant* Target, int32 BaseAmount);
	void QueueDrawCardAction();
	void QueueDiscardCardAction(int32 RuntimeId);

	bool HasValidCombatants() const;
	bool HasValidActionQueue() const;
	bool HasValidDeckRuntime() const;
	bool IsActionQueueBusy() const;

	UPROPERTY(Transient)
	TObjectPtr<UBattleActionQueue> ActionQueue = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> DeckRuntime = nullptr;
};
