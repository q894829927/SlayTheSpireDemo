#include "DrawCardsAction.h"

#include "BattleActionQueue.h"
#include "DrawCardAction.h"
#include "ShuffleDeckAction.h"
#include "../Battle/BattleManager.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEventDispatcher.h"

void UDrawCardsAction::Initialize(UDeckRuntime* InDeck, int32 InDrawCount)
{
	Initialize(InDeck, InDrawCount, nullptr);
}

void UDrawCardsAction::Initialize(
	UDeckRuntime* InDeck,
	int32 InDrawCount,
	ACombatant* InPresentationCardSource
)
{
	Deck = InDeck;
	RemainingDraws = InDrawCount;
	EventDispatcher = nullptr;
	EventCombatants.Reset();
	PresentationCardSource = InPresentationCardSource;
}

void UDrawCardsAction::Initialize(
	UDeckRuntime* InDeck,
	int32 InDrawCount,
	UBattleEventDispatcher* InEventDispatcher,
	const TArray<ACombatant*>& InEventCombatants
)
{
	Initialize(InDeck, InDrawCount, InEventDispatcher, InEventCombatants, nullptr);
}

void UDrawCardsAction::Initialize(
	UDeckRuntime* InDeck,
	int32 InDrawCount,
	UBattleEventDispatcher* InEventDispatcher,
	const TArray<ACombatant*>& InEventCombatants,
	ACombatant* InPresentationCardSource
)
{
	Deck = InDeck;
	RemainingDraws = InDrawCount;
	EventDispatcher = InEventDispatcher;
	EventCombatants.Reset();
	for (ACombatant* Combatant : InEventCombatants)
	{
		EventCombatants.Add(Combatant);
	}
	PresentationCardSource = InPresentationCardSource;
}

void UDrawCardsAction::Execute(UBattleActionQueue* Queue)
{
	UDeckRuntime* RuntimeDeck = Deck.Get();
	if (!IsValid(RuntimeDeck) || !IsValid(Queue) || RemainingDraws <= 0)
	{
		if (RemainingDraws < 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Action] DrawCardsAction rejected negative RemainingDraws=%d."), RemainingDraws);
		}
		Finish();
		return;
	}

	const int32 FreeHandSlots = FMath::Max(0, RuntimeDeck->GetMaxHandSize() - RuntimeDeck->GetHandCount());
	if (FreeHandSlots <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Action] DrawCardsAction ended: Hand is full."));
		Finish();
		return;
	}

	const int32 EffectiveDraws = FMath::Min(RemainingDraws, FreeHandSlots);
	const int32 DrawPileCount = RuntimeDeck->GetDrawCount();
	const int32 DiscardPileCount = RuntimeDeck->GetDiscardCount();

	// A brand-new bulk draw request against a completely exhausted deck ends
	// immediately. Zero-card shuffles only occur when a prior bulk-draw planning
	// step already scheduled a ShuffleAction after consuming the available draw
	// cards.
	if (DrawPileCount == 0 && DiscardPileCount == 0)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Action] DrawCardsAction ended: no cards exist in DrawPile or DiscardPile. Remaining=%d"),
			EffectiveDraws
		);
		Finish();
		return;
	}

	const int32 ImmediateDraws = FMath::Min(EffectiveDraws, DrawPileCount);
	const int32 RemainingAfterImmediate = EffectiveDraws - ImmediateDraws;
	TArray<UBattleAction*> ContinuationBatch;
	ContinuationBatch.Reserve(ImmediateDraws + (RemainingAfterImmediate > 0 ? 2 : 0));

	for (int32 Index = 0; Index < ImmediateDraws; ++Index)
	{
		UDrawCardAction* DrawAction = NewObject<UDrawCardAction>(Queue);
		DrawAction->Initialize(RuntimeDeck, PresentationCardSource.Get());
		DrawAction->SetPresentationRecordWriter(GetPresentationRecordWriter());
		ContinuationBatch.Add(DrawAction);
	}

	if (RemainingAfterImmediate > 0)
	{
		UBattleEventDispatcher* ResolvedDispatcher = EventDispatcher.Get();
		TArray<ACombatant*> RawCombatants;
		if (IsValid(ResolvedDispatcher) && EventCombatants.Num() > 0)
		{
			RawCombatants.Reserve(EventCombatants.Num());
			for (const TObjectPtr<ACombatant>& Combatant : EventCombatants)
			{
				if (!IsValid(Combatant.Get()))
				{
					Queue->RequestResolutionFault(TEXT("DrawCardsAction found an invalid authoritative combatant in its event-dispatch context."));
					Finish();
					return;
				}
				RawCombatants.Add(Combatant.Get());
			}
		}
		else
		{
			ABattleManager* Battle = Cast<ABattleManager>(Queue->GetOuter());
			if (!IsValid(Battle) || !Battle->TryBuildEventDispatchContext(ResolvedDispatcher, RawCombatants))
			{
				Queue->RequestResolutionFault(TEXT("DrawCardsAction requires valid battle-event wiring before scheduling a bulk-draw shuffle."));
				Finish();
				return;
			}
		}

		UShuffleDeckAction* ShuffleAction = NewObject<UShuffleDeckAction>(Queue);
		ShuffleAction->Initialize(RuntimeDeck, ResolvedDispatcher, RawCombatants);
		ShuffleAction->SetPresentationRecordWriter(GetPresentationRecordWriter());
		ContinuationBatch.Add(ShuffleAction);

		UDrawCardsAction* RemainingAction = NewObject<UDrawCardsAction>(Queue);
		RemainingAction->Initialize(
			RuntimeDeck,
			RemainingAfterImmediate,
			ResolvedDispatcher,
			RawCombatants,
			PresentationCardSource.Get()
		);
		RemainingAction->SetPresentationRecordWriter(GetPresentationRecordWriter());
		ContinuationBatch.Add(RemainingAction);
	}

	if (!Queue->AddBatchToFrontPreserveOrder(ContinuationBatch))
	{
		Queue->RequestResolutionFault(TEXT("DrawCardsAction failed to enqueue its deterministic bulk-draw continuation batch."));
		Finish();
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Action] DrawCardsAction planned Remaining=%d Immediate=%d AfterImmediate=%d Draw=%d Discard=%d."),
		EffectiveDraws,
		ImmediateDraws,
		RemainingAfterImmediate,
		DrawPileCount,
		DiscardPileCount
	);
	Finish();
}
