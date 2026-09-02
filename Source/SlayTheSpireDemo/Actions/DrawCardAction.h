#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "DrawCardAction.generated.h"

class ACombatant;
class UBattleEventDispatcher;
class UDeckRuntime;

UCLASS()
class SLAYTHESPIREDEMO_API UDrawCardAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(UDeckRuntime* InDeck);
	void Initialize(UDeckRuntime* InDeck, ACombatant* InPresentationCardSource);
	void Initialize(
		UDeckRuntime* InDeck,
		UBattleEventDispatcher* InEventDispatcher,
		const TArray<ACombatant*>& InEventCombatants
	);
	void Initialize(
		UDeckRuntime* InDeck,
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

	// One authored draw attempt may cause at most one Shuffle -> RetryDraw cycle.
	// This is required for source-game zero-card shuffle semantics without
	// recursively shuffling forever when both DrawPile and DiscardPile stay empty.
	bool bRetriedAfterShuffle = false;
};
