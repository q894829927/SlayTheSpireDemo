#include "ShuffleDeckAction.h"

#include "../Deck/DeckRuntime.h"

void UShuffleDeckAction::Initialize(UDeckRuntime* InDeck)
{
	Deck = InDeck;
}

void UShuffleDeckAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Deck.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ShuffleDeckAction skipped: invalid Deck."));
		Finish();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Action] ShuffleDeckAction executing."));
	Deck->ShuffleDiscardIntoDrawPile();
	Finish();
}
