#include "ExhaustSelectedContinuation.h"

#include "../Actions/BattleActionQueue.h"
#include "../Actions/ExhaustCardAction.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEventDispatcher.h"

void UExhaustSelectedContinuation::Initialize(
	UDeckRuntime* InDeck,
	ACombatant* InPresentationCardSource,
	UBattleEventDispatcher* InEventDispatcher,
	const TArray<ACombatant*>& InEventCombatants
)
{
	Deck = InDeck;
	PresentationCardSource = InPresentationCardSource;
	EventDispatcher = InEventDispatcher;
	EventCombatants.Reset();
	for (ACombatant* Combatant : InEventCombatants)
	{
		EventCombatants.Add(Combatant);
	}
}

bool UExhaustSelectedContinuation::BuildNextActions(
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
			UE_LOG(LogTemp, Warning, TEXT("[Selection] ExhaustSelectedContinuation rejected an invalid or duplicate selected card."));
			return false;
		}
		SeenCards.Add(ChosenCard);
		SelectedCards.Add(ChosenCard);
	}

	TArray<ACombatant*> RawEventCombatants;
	RawEventCombatants.Reserve(EventCombatants.Num());
	for (const TObjectPtr<ACombatant>& Combatant : EventCombatants)
	{
		if (!IsValid(Combatant.Get()))
		{
			return false;
		}
		RawEventCombatants.Add(Combatant.Get());
	}

	OutActions.Reserve(SelectedCards.Num());
	for (UCardInstance* ChosenCard : SelectedCards)
	{
		UExhaustCardAction* ExhaustAction = NewObject<UExhaustCardAction>(Queue);
		ExhaustAction->Initialize(
			Deck.Get(),
			ChosenCard,
			PresentationCardSource.Get(),
			EventDispatcher.Get(),
			RawEventCombatants
		);
		OutActions.Add(ExhaustAction);
	}
	return true;
}
