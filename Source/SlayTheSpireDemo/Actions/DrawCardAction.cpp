#include "DrawCardAction.h"

#include "BattleActionQueue.h"
#include "ShuffleDeckAction.h"
#include "../Cards/CardInstance.h"
#include "../Deck/DeckRuntime.h"

void UDrawCardAction::Initialize(UDeckRuntime* InDeck)
{
	Deck = InDeck;
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
		UShuffleDeckAction* ShuffleAction = NewObject<UShuffleDeckAction>(Queue);
		ShuffleAction->Initialize(Deck.Get());

		UDrawCardAction* RetryDrawAction = NewObject<UDrawCardAction>(Queue);
		RetryDrawAction->Initialize(Deck.Get());

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
