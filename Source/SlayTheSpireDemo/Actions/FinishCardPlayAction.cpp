#include "FinishCardPlayAction.h"

#include "BattleActionQueue.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEvent.h"
#include "../Events/BattleEventDispatcher.h"
#include "../Presentation/PresentationCardSnapshotBuilder.h"

void UFinishCardPlayAction::Initialize(UDeckRuntime* InDeck, UCardInstance* InCard)
{
	Initialize(InDeck, InCard, nullptr);
}

void UFinishCardPlayAction::Initialize(
	UDeckRuntime* InDeck,
	UCardInstance* InCard,
	ACombatant* InPresentationCardSource
)
{
	Deck = InDeck;
	Card = InCard;
	PresentationCardSource = InPresentationCardSource;
	EventDispatcher = nullptr;
	EventCombatants.Reset();
}

void UFinishCardPlayAction::Initialize(
	UDeckRuntime* InDeck,
	UCardInstance* InCard,
	ACombatant* InPresentationCardSource,
	UBattleEventDispatcher* InEventDispatcher,
	const TArray<ACombatant*>& InEventCombatants
)
{
	Deck = InDeck;
	Card = InCard;
	PresentationCardSource = InPresentationCardSource;
	EventDispatcher = InEventDispatcher;
	EventCombatants.Reset();
	for (ACombatant* Combatant : InEventCombatants)
	{
		EventCombatants.Add(Combatant);
	}
}

void UFinishCardPlayAction::Execute(UBattleActionQueue* Queue)
{
	if (!IsValid(Queue) || !IsValid(Deck.Get()) || !IsValid(Card.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] FinishCardPlayAction skipped: invalid Queue, Deck or Card."));
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
	TArray<ACombatant*> RawEventCombatants;
	if (Destination == ECardDestination::Exhaust)
	{
		if (!IsValid(EventDispatcher.Get()) || EventCombatants.Num() == 0)
		{
			Queue->RequestResolutionFault(FString::Printf(
				TEXT("FinishCardPlayAction requires explicit event wiring before exhausting %s."),
				*Card->GetDebugLabel()
			));
			Finish();
			return;
		}

		RawEventCombatants.Reserve(EventCombatants.Num());
		for (const TObjectPtr<ACombatant>& Combatant : EventCombatants)
		{
			if (!IsValid(Combatant.Get()))
			{
				Queue->RequestResolutionFault(FString::Printf(
					TEXT("FinishCardPlayAction found invalid authoritative combatant wiring before exhausting %s."),
					*Card->GetDebugLabel()
				));
				Finish();
				return;
			}
			RawEventCombatants.Add(Combatant.Get());
		}
	}

	const FCardZoneMutationResult CommitResult = Deck->TryMovePlayAreaCardToDestinationCommit(Card.Get(), Destination);
	if (!CommitResult.bCommitted)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] FinishCardPlayAction failed to resolve destination for %s."),
			*Card->GetDebugLabel()
		);
		Finish();
		return;
	}

	const bool bCommittedExhaust = CommitResult.ToZone == ECardZone::ExhaustPile;
	if (bCommittedExhaust
		&& (CommitResult.CardRuntimeId != Card->GetRuntimeId()
			|| CommitResult.CardId != Card->GetCardId()
			|| CommitResult.FromZone != ECardZone::PlayArea))
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("FinishCardPlayAction committed Exhaust with inconsistent exact-card facts for %s."),
			*Card->GetDebugLabel()
		));
		Finish();
		return;
	}

	const FPresentationRecordWriter& Writer = GetPresentationRecordWriter();
	if (Writer.IsAvailable())
	{
		FPresentationCardSnapshot CardSnapshot;
		if (!PresentationCardSnapshot::TryBuild(Card.Get(), PresentationCardSource.Get(), CardSnapshot)
			|| CardSnapshot.RuntimeId != CommitResult.CardRuntimeId
			|| CardSnapshot.CardId != CommitResult.CardId)
		{
			Writer.InvalidateCurrentResolution();
			UE_LOG(LogTemp, Warning, TEXT("[Presentation] Finish-card commit could not freeze a trustworthy card payload."));
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

	if (bCommittedExhaust)
	{
		if (!IsValid(EventDispatcher.Get()) || RawEventCombatants.Num() == 0)
		{
			Queue->RequestResolutionFault(FString::Printf(
				TEXT("FinishCardPlayAction lost event wiring after committed Exhaust for %s."),
				*Card->GetDebugLabel()
			));
			Finish();
			return;
		}

		const FBattleEvent Event = FBattleEvent::MakeCardExhausted(Card.Get(), CommitResult);
		if (!EventDispatcher->Dispatch(
			Event,
			Queue,
			RawEventCombatants,
			nullptr,
			&Writer
		))
		{
			Queue->RequestResolutionFault(FString::Printf(
				TEXT("CardExhausted event dispatch failed after committed Exhaust for %s."),
				*Card->GetDebugLabel()
			));
			Finish();
			return;
		}
	}

	Finish();
}
