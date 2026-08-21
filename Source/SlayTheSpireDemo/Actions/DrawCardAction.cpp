#include "DrawCardAction.h"

#include "BattleActionQueue.h"
#include "ShuffleDeckAction.h"
#include "../Battle/BattleManager.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEventDispatcher.h"

void UDrawCardAction::Initialize(UDeckRuntime* InDeck)
{
	Deck = InDeck;
	EventDispatcher = nullptr;
	EventCombatants.Reset();
}

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
		UBattleEventDispatcher* ResolvedEventDispatcher = EventDispatcher.Get();
		TArray<ACombatant*> RawCombatants;

		if (IsValid(ResolvedEventDispatcher) && EventCombatants.Num() > 0)
		{
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
		}
		else
		{
			// Direct BattleManager debug draws still use the original Initialize(Deck)
			// entry point. Resolve the same narrow battle-scoped dependencies from the
			// Queue's authoritative owner without searching the world. Presentation
			// recording is deliberately not inferred from the Queue Outer; it is carried
			// independently by this Action's explicit writer value.
			ABattleManager* Battle = Cast<ABattleManager>(Queue->GetOuter());
			if (!IsValid(Battle) || !Battle->TryBuildEventDispatchContext(ResolvedEventDispatcher, RawCombatants))
			{
				Queue->RequestResolutionFault(TEXT("DrawCardAction requires valid battle-event wiring before scheduling Shuffle -> RetryDraw."));
				Finish();
				return;
			}
		}

		UShuffleDeckAction* ShuffleAction = NewObject<UShuffleDeckAction>(Queue);
		ShuffleAction->Initialize(Deck.Get(), ResolvedEventDispatcher, RawCombatants);
		ShuffleAction->SetPresentationRecordWriter(GetPresentationRecordWriter());

		UDrawCardAction* RetryDrawAction = NewObject<UDrawCardAction>(Queue);
		RetryDrawAction->Initialize(Deck.Get(), ResolvedEventDispatcher, RawCombatants);
		RetryDrawAction->SetPresentationRecordWriter(GetPresentationRecordWriter());

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
