#include "FinishCardPlayAction.h"

#include "../Cards/CardInstance.h"
#include "../Deck/DeckRuntime.h"

void UFinishCardPlayAction::Initialize(UDeckRuntime* InDeck, UCardInstance* InCard)
{
	Deck = InDeck;
	Card = InCard;
}

void UFinishCardPlayAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Deck.Get()) || !IsValid(Card.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] FinishCardPlayAction skipped: invalid Deck or Card."));
		Finish();
		return;
	}

	if (!Deck->IsCardInPlayArea(Card.Get()))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] FinishCardPlayAction skipped: %s is no longer in PlayArea."),
			*Card->GetDebugLabel()
		);
		Finish();
		return;
	}

	const ECardDestination Destination = Card->ResolveDestination();
	if (!Deck->TryMovePlayAreaCardToDestination(Card.Get(), Destination))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] FinishCardPlayAction failed to resolve destination for %s."),
			*Card->GetDebugLabel()
		);
	}

	Finish();
}
