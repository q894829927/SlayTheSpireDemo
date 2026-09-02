#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "DrawCardsAction.generated.h"

class ACombatant;
class UBattleEventDispatcher;
class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UDrawCardsAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UDeckRuntime* InDeck, int32 InDrawCount);
	void Initialize(UDeckRuntime* InDeck, int32 InDrawCount, ACombatant* InPresentationCardSource);
	void Initialize(
		UDeckRuntime* InDeck,
		int32 InDrawCount,
		UBattleEventDispatcher* InEventDispatcher,
		const TArray<ACombatant*>& InEventCombatants
	);
	void Initialize(
		UDeckRuntime* InDeck,
		int32 InDrawCount,
		UBattleEventDispatcher* InEventDispatcher,
		const TArray<ACombatant*>& InEventCombatants,
		ACombatant* InPresentationCardSource
	);

	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleEventDispatcher> EventDispatcher = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatant>> EventCombatants;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> PresentationCardSource = nullptr;

	int32 RemainingDraws = 0;
};
