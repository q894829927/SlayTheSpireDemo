#include "MoveSelectedHandCardsToDrawPileTopContinuation.h"

#include "../Actions/BattleActionQueue.h"
#include "../Actions/MoveHandCardToDrawPileTopAction.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"

void UMoveSelectedHandCardsToDrawPileTopContinuation::Initialize(
	UDeckRuntime* InDeck,
	ACombatant* InPresentationCardSource
)
{
	Deck = InDeck;
	PresentationCardSource = InPresentationCardSource;
}

bool UMoveSelectedHandCardsToDrawPileTopContinuation::BuildNextActions(
	const FSelectionResult& Result,
	UBattleActionQueue* Queue,
	TArray<UBattleAction*>& OutActions
) const
{
	OutActions.Reset();

	if (Result.Status != ESelectionStatus::Resolved
		|| Result.SelectedObjects.Num() == 0
		|| !IsValid(Queue)
		|| !IsValid(Deck.Get()))
	{
		return false;
	}

	TArray<UCardInstance*> SelectedCards;
	SelectedCards.Reserve(Result.SelectedObjects.Num());
	TSet<const UCardInstance*> SeenCards;
	for (const TObjectPtr<UObject>& SelectedObject : Result.SelectedObjects)
	{
		UCardInstance* ChosenCard = Cast<UCardInstance>(SelectedObject.Get());
		if (!IsValid(ChosenCard) || SeenCards.Contains(ChosenCard))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Selection] MoveSelectedHandCardsToDrawPileTopContinuation rejected an invalid or duplicate selected card."));
			return false;
		}
		SeenCards.Add(ChosenCard);
		SelectedCards.Add(ChosenCard);
	}

	OutActions.Reserve(SelectedCards.Num());
	for (UCardInstance* ChosenCard : SelectedCards)
	{
		UMoveHandCardToDrawPileTopAction* MoveAction = NewObject<UMoveHandCardToDrawPileTopAction>(Queue);
		MoveAction->Initialize(Deck.Get(), ChosenCard, PresentationCardSource.Get());
		OutActions.Add(MoveAction);
	}
	return true;
}
