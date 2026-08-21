#include "ShuffleDeckAction.h"

#include "BattleActionQueue.h"
#include "../Battle/BattleManager.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEvent.h"
#include "../Events/BattleEventDispatcher.h"

void UShuffleDeckAction::Initialize(UDeckRuntime* InDeck)
{
	Deck = InDeck;
	EventDispatcher = nullptr;
	EventCombatants.Reset();
}

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

	// Expected Deck-level no-ops do not need event wiring because there will be
	// no committed DeckShuffled fact to publish. Delegate the no-op itself to
	// DeckRuntime so its authoritative validation/logging remains unchanged.
	if (!Deck->HasCardsInDiscardPile() || Deck->HasCardsInDrawPile())
	{
		Deck->ShuffleDiscardIntoDrawPile();
		Finish();
		return;
	}

	UBattleEventDispatcher* ResolvedEventDispatcher = EventDispatcher.Get();
	TArray<ACombatant*> RawCombatants;

	if (IsValid(ResolvedEventDispatcher) && EventCombatants.Num() > 0)
	{
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
	}
	else
	{
		ABattleManager* Battle = Cast<ABattleManager>(Queue->GetOuter());
		if (!IsValid(Battle) || !Battle->TryBuildEventDispatchContext(ResolvedEventDispatcher, RawCombatants))
		{
			Queue->RequestResolutionFault(TEXT("ShuffleDeckAction requires valid battle-event wiring before shuffle commit."));
			Finish();
			return;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Action] ShuffleDeckAction executing."));
	if (!Deck->ShuffleDiscardIntoDrawPile())
	{
		// A preflight-valid shuffle can still fail soft if DeckRuntime rejects it.
		// No commit means no FDeckShuffledEvent.
		Finish();
		return;
	}

	const FPresentationRecordWriter& PresentationWriter = GetPresentationRecordWriter();
	if (!ResolvedEventDispatcher->Dispatch(
		FBattleEvent::MakeDeckShuffled(Deck.Get()),
		Queue,
		RawCombatants,
		nullptr,
		&PresentationWriter
	))
	{
		Queue->RequestResolutionFault(TEXT("DeckShuffled event dispatch failed after a successful shuffle commit."));
		Finish();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Action] DeckShuffled event dispatched after successful shuffle commit."));
	Finish();
}
