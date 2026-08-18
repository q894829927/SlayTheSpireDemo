#include "ShuffleDeckAction.h"

#include "BattleActionQueue.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEvent.h"
#include "../Events/BattleEventDispatcher.h"

void UShuffleDeckAction::Initialize(
	UDeckRuntime* InDeck,
	UBattleEventDispatcher* InEventDispatcher,
	const TArray<ACombatant*>& InEventCombatants
)
{
	Deck = InDeck;
	EventDispatcher = InEventDispatcher;
	EventCombatants.Reset();
	for (ACombatant* Combatant : InEventCombatants)
	{
		EventCombatants.Add(Combatant);
	}
}

void UShuffleDeckAction::Execute(UBattleActionQueue* Queue)
{
	if (!IsValid(Deck.Get()) || !IsValid(Queue))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ShuffleDeckAction skipped: invalid Deck or Queue."));
		Finish();
		return;
	}

	if (!IsValid(EventDispatcher.Get()) || EventCombatants.Num() == 0)
	{
		Queue->RequestResolutionFault(TEXT("ShuffleDeckAction requires valid battle-event wiring before shuffle commit."));
		Finish();
		return;
	}

	TArray<ACombatant*> RawCombatants;
	RawCombatants.Reserve(EventCombatants.Num());
	for (const TObjectPtr<ACombatant>& Combatant : EventCombatants)
	{
		if (!IsValid(Combatant.Get()))
		{
			Queue->RequestResolutionFault(TEXT("ShuffleDeckAction found an invalid authoritative combatant in its event-dispatch context."));
			Finish();
			return;
		}
		RawCombatants.Add(Combatant.Get());
	}

	UE_LOG(LogTemp, Log, TEXT("[Action] ShuffleDeckAction executing."));
	if (!Deck->ShuffleDiscardIntoDrawPile())
	{
		// Failed/no-op shuffles are not committed gameplay facts and therefore
		// emit no FDeckShuffledEvent.
		Finish();
		return;
	}

	if (!EventDispatcher->Dispatch(FBattleEvent::MakeDeckShuffled(Deck.Get()), Queue, RawCombatants))
	{
		Queue->RequestResolutionFault(TEXT("DeckShuffled event dispatch failed after a successful shuffle commit."));
		Finish();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Action] DeckShuffled event dispatched after successful shuffle commit."));
	Finish();
}
