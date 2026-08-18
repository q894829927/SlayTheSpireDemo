#include "DrawCardAction.h"

#include "BattleActionQueue.h"
#include "ShuffleDeckAction.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEventDispatcher.h"

void UDrawCardAction::Initialize(
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

void UDrawCardAction::Execute(UBattleActionQueue* Queue)
{
	if (!IsValid(Deck.Get()) || !IsValid(Queue))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DrawCardAction skipped: invalid Deck or Queue."));
		Finish();
		return;
	}

	if (Deck->IsHandFull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DrawCardAction skipped: hand is full."));
		Finish();
		return;
	}

	if (Deck->HasCardsInDrawPile())
	{
		UCardInstance* DrawnCard = nullptr;
		if (!Deck->TryDrawTopCard(DrawnCard))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Action] DrawCardAction failed to draw despite a non-empty DrawPile."));
		}

		Finish();
		return;
	}

	if (Deck->HasCardsInDiscardPile())
	{
		if (!IsValid(EventDispatcher.Get()) || EventCombatants.Num() == 0)
		{
			Queue->RequestResolutionFault(TEXT("DrawCardAction requires valid battle-event wiring before scheduling Shuffle -> RetryDraw."));
			Finish();
			return;
		}

		TArray<ACombatant*> RawCombatants;
		RawCombatants.Reserve(EventCombatants.Num());
		for (const TObjectPtr<ACombatant>& Combatant : EventCombatants)
		{
			if (!IsValid(Combatant.Get()))
			{
				Queue->RequestResolutionFault(TEXT("DrawCardAction found an invalid authoritative combatant in its event-dispatch context."));
				Finish();
				return;
			}
			RawCombatants.Add(Combatant.Get());
		}

		UShuffleDeckAction* ShuffleAction = NewObject<UShuffleDeckAction>(Queue);
		ShuffleAction->Initialize(Deck.Get(), EventDispatcher.Get(), RawCombatants);

		UDrawCardAction* RetryDrawAction = NewObject<UDrawCardAction>(Queue);
		RetryDrawAction->Initialize(Deck.Get(), EventDispatcher.Get(), RawCombatants);

		TArray<UBattleAction*> ContinuationBatch;
		ContinuationBatch.Add(ShuffleAction);
		ContinuationBatch.Add(RetryDrawAction);

		if (!Queue->AddBatchToFrontPreserveOrder(ContinuationBatch))
		{
			Queue->RequestResolutionFault(TEXT("DrawCardAction failed to enqueue the atomic Shuffle -> RetryDraw continuation."));
			Finish();
			return;
		}

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Action] DrawCardAction found an empty DrawPile. Queued atomic ShuffleDeckAction -> RetryDraw continuation at the front.")
		);

		Finish();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Action] DrawCardAction skipped: DrawPile and DiscardPile are both empty."));
	Finish();
}
