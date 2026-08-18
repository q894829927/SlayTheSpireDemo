#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "PlayCardAction.generated.h"

class ABattleManager;
class ACombatant;
class UBattleEventDispatcher;
class UCardInstance;
class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UPlayCardAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(
		ABattleManager* InBattle,
		UCardInstance* InCard,
		ACombatant* InSource,
		ACombatant* InRequestedTarget,
		UDeckRuntime* InDeck,
		UBattleEventDispatcher* InEventDispatcher,
		const TArray<ACombatant*>& InEventCombatants
	);

	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ABattleManager> Battle = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCardInstance> Card = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Source = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> RequestedTarget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleEventDispatcher> EventDispatcher = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatant>> EventCombatants;
};
