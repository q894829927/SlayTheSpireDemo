#include "DiscardCardAction.h"

#include "../Deck/DeckRuntime.h"

void UDiscardCardAction::Initialize(UDeckRuntime* InDeck, int32 InRuntimeId)
{
	Deck = InDeck;
	RuntimeId = InRuntimeId;
}

void UDiscardCardAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Deck.Get()) || RuntimeId <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DiscardCardAction skipped: invalid Deck or RuntimeId=%d."), RuntimeId);
		Finish();
		return;
	}

	FDeckCardToken DiscardedCard;
	if (!Deck->TryDiscardCardByRuntimeId(RuntimeId, DiscardedCard))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] DiscardCardAction skipped: RuntimeId=%d is no longer in Hand."),
			RuntimeId
		);
		Finish();
		return;
	}

	Finish();
}
