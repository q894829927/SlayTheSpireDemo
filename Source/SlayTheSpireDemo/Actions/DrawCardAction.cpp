#include "DrawCardAction.h"

#include "BattleActionQueue.h"
#include "ShuffleDeckAction.h"
#include "../Battle/BattleManager.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEventDispatcher.h"
#include "../Presentation/PresentationCardSnapshotBuilder.h"

void UDrawCardAction::Initialize(UDeckRuntime* InDeck)
{
	Initialize(InDeck, nullptr);
}

void UDrawCardAction::Initialize(UDeckRuntime* InDeck, ACombatant* InPresentationCardSource)
{
	Deck = InDeck;
	EventDispatcher = nullptr;
	EventCombatants.Reset();
	PresentationCardSource = InPresentationCardSource;
}

void UDrawCardAction::Initialize(
	UDeckRuntime* InDeck,
	UBattleEventDispatcher* InEventDispatcher,
	const TArray<ACombatant*>& InEventCombatants
)
{
	Initialize(InDeck, InEventDispatcher, InEventCombatants, nullptr);
}

void UDrawCardAction::Initialize(
	UDeckRuntime* InDeck,
	UBattleEventDispatcher* InEventDispatcher,
	const TArray<ACombatant*>& InEventCombatants,
	ACombatant* InPresentationCardSource
)
{
	Deck = InDeck;
	EventDispatcher = InEventDispatcher;
	EventCombatants.Reset();
	for (ACombatant* Combatant : InEventCombatants)
	{
		EventCombatants.Add(Combatant);
	}
	PresentationCardSource = InPresentationCardSource;
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
		const FCardZoneMutationResult CommitResult = Deck->TryDrawTopCardCommit(DrawnCard);
		if (!CommitResult.bCommitted)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Action] DrawCardAction failed to draw despite a non-empty DrawPile."));
			Finish();
			return;
		}

		const FPresentationRecordWriter& Writer = GetPresentationRecordWriter();
		if (Writer.IsAvailable())
		{
			FPresentationCardSnapshot CardSnapshot;
			if (!PresentationCardSnapshot::TryBuild(DrawnCard, PresentationCardSource.Get(), CardSnapshot)
				|| CardSnapshot.RuntimeId != CommitResult.CardRuntimeId
				|| CardSnapshot.CardId != CommitResult.CardId)
			{
				Writer.InvalidateCurrentResolution();
				UE_LOG(LogTemp, Warning, TEXT("[Presentation] Draw commit could not freeze a trustworthy card payload."));
			}
			else
			{
				FPresentationRecord Record;
				Record.Type = EBattlePresentationRecordType::CardZoneChanged;
				Record.CardZoneChanged.Card = MoveTemp(CardSnapshot);
				Record.CardZoneChanged.FromZone = CommitResult.FromZone;
				Record.CardZoneChanged.ToZone = CommitResult.ToZone;
				Record.CardZoneChanged.FromIndex = CommitResult.FromIndex;
				Record.CardZoneChanged.ToIndex = CommitResult.ToIndex;
				Writer.Append(MoveTemp(Record));
			}
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
		RetryDrawAction->Initialize(
			Deck.Get(),
			ResolvedEventDispatcher,
			RawCombatants,
			PresentationCardSource.Get()
		);
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
