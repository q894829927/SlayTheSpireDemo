#include "DiscardCardAction.h"

#include "../Cards/CardInstance.h"
#include "../Deck/DeckRuntime.h"

void UDiscardCardAction::Initialize(UDeckRuntime* InDeck, UCardInstance* InCard)
{
	Deck = InDeck;
	Card = InCard;
}

void UDiscardCardAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Deck.Get()) || !IsValid(Card.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DiscardCardAction skipped: invalid Deck or Card."));
		Finish();
		return;
	}

	if (!Deck->TryDiscardCard(Card.Get()))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] DiscardCardAction skipped: %s is no longer in Hand."),
			*Card->GetDebugLabel()
		);
		Finish();
		return;
	}

	Finish();
}
