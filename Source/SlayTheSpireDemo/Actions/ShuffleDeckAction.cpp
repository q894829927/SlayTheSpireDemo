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

	// A gameplay shuffle is only legal when DrawPile is empty. DiscardPile may
	// also be empty: that zero-card shuffle is still a committed fact so each
	// exhausted draw attempt can trigger shuffle-reactive mechanics exactly once.
	if (Deck->HasCardsInDrawPile())
	{
		Deck->ShuffleDiscardIntoDrawPileCommit();
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
	const FDeckShuffleCommitResult CommitResult = Deck->ShuffleDiscardIntoDrawPileCommit();
	if (!CommitResult.bCommitted)
	{
		Finish();
		return;
	}

	const FPresentationRecordWriter& PresentationWriter = GetPresentationRecordWriter();
	if (PresentationWriter.IsAvailable())
	{
		FPresentationRecord Record;
		Record.Type = EBattlePresentationRecordType::DeckShuffled;
		Record.DeckShuffled.MovedCardCount = CommitResult.MovedCardCount;
		Record.DeckShuffled.DrawCountBefore = CommitResult.DrawCountBefore;
		Record.DeckShuffled.DrawCountAfter = CommitResult.DrawCountAfter;
		Record.DeckShuffled.DiscardCountBefore = CommitResult.DiscardCountBefore;
		Record.DeckShuffled.DiscardCountAfter = CommitResult.DiscardCountAfter;
		if (!PresentationWriter.Append(MoveTemp(Record)))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Presentation] DeckShuffled record append failed; Gameplay shuffle remains authoritative."));
		}
	}

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

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Action] DeckShuffled event dispatched after committed gameplay shuffle. MovedCards=%d."),
		CommitResult.MovedCardCount
	);
	Finish();
}
