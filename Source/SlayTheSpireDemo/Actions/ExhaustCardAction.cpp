#include "ExhaustCardAction.h"

#include "BattleActionQueue.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEvent.h"
#include "../Events/BattleEventDispatcher.h"
#include "../Presentation/PresentationCardSnapshotBuilder.h"

void UExhaustCardAction::Initialize(
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
	CommitResult = FCardZoneMutationResult{};
}

void UExhaustCardAction::Execute(UBattleActionQueue* Queue)
{
	CommitResult = FCardZoneMutationResult{};

	if (!IsValid(Queue) || !IsValid(Deck.Get()) || !IsValid(Card.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ExhaustCardAction skipped: invalid Queue, Deck or Card."));
		Finish();
		return;
	}

	if (!Deck->IsCardInHand(Card.Get()))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] ExhaustCardAction skipped: exact target %s is no longer in Hand."),
			*Card->GetDebugLabel()
		);
		Finish();
		return;
	}

	if (!IsValid(EventDispatcher.Get()) || EventCombatants.Num() == 0)
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("ExhaustCardAction requires explicit event wiring before exhausting %s."),
			*Card->GetDebugLabel()
		));
		Finish();
		return;
	}

	TArray<ACombatant*> RawEventCombatants;
	RawEventCombatants.Reserve(EventCombatants.Num());
	for (const TObjectPtr<ACombatant>& Combatant : EventCombatants)
	{
		if (!IsValid(Combatant.Get()))
		{
			Queue->RequestResolutionFault(FString::Printf(
				TEXT("ExhaustCardAction found invalid authoritative combatant wiring before exhausting %s."),
				*Card->GetDebugLabel()
			));
			Finish();
			return;
		}
		RawEventCombatants.Add(Combatant.Get());
	}

	CommitResult = Deck->TryExhaustHandCardCommit(Card.Get());
	if (!CommitResult.bCommitted)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] ExhaustCardAction failed to commit exact target %s from Hand."),
			*Card->GetDebugLabel()
		);
		Finish();
		return;
	}

	if (CommitResult.CardRuntimeId != Card->GetRuntimeId()
		|| CommitResult.CardId != Card->GetCardId()
		|| CommitResult.FromZone != ECardZone::Hand
		|| CommitResult.ToZone != ECardZone::ExhaustPile)
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("ExhaustCardAction committed inconsistent exact-card facts for %s."),
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
			UE_LOG(LogTemp, Warning, TEXT("[Presentation] Targeted Exhaust commit could not freeze a trustworthy card payload."));
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
			if (!Writer.Append(MoveTemp(Record)))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Presentation] Targeted Exhaust CardZoneChanged append failed; Gameplay commit remains authoritative."));
			}
		}
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
			TEXT("CardExhausted event dispatch failed after targeted Exhaust commit for %s."),
			*Card->GetDebugLabel()
		));
		Finish();
		return;
	}

	Finish();
}
