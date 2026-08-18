#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "ShuffleDeckAction.generated.h"

class ACombatant;
class UBattleEventDispatcher;
class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UShuffleDeckAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UDeckRuntime* InDeck);
	void Initialize(
		UDeckRuntime* InDeck,
		UBattleEventDispatcher* InEventDispatcher,
		const TArray<ACombatant*>& InEventCombatants
	);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleEventDispatcher> EventDispatcher = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatant>> EventCombatants;
};
