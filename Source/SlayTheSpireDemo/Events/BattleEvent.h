#pragma once

#include "CoreMinimal.h"
#include "../Deck/DeckMutationTypes.h"

class ACombatant;
class UCardInstance;
class UDeckRuntime;

enum class EBattleEventType : uint8
{
	None,
	TurnEnded,
	DeckShuffled,
	CardExhausted
};

struct FTurnEndedEvent
{
	ACombatant* TurnOwner = nullptr;
};

struct FDeckShuffledEvent
{
	UDeckRuntime* Deck = nullptr;
};

struct FCardExhaustedEvent
{
	// Exact runtime subject. Consumers may use this identity/reference, but
	// committed scalar facts below remain the event-time authority.
	UCardInstance* Card = nullptr;
	int32 CardRuntimeId = INDEX_NONE;
	FName CardId = NAME_None;
	ECardZone FromZone{};
	ECardZone ToZone{};
};

struct FBattleEvent
{
	static FBattleEvent MakeTurnEnded(ACombatant* TurnOwner)
	{
		FBattleEvent Event;
		Event.Type = EBattleEventType::TurnEnded;
		Event.TurnEndedPayload.TurnOwner = TurnOwner;
		return Event;
	}

	static FBattleEvent MakeDeckShuffled(UDeckRuntime* Deck)
	{
		FBattleEvent Event;
		Event.Type = EBattleEventType::DeckShuffled;
		Event.DeckShuffledPayload.Deck = Deck;
		return Event;
	}

	static FBattleEvent MakeCardExhausted(
		UCardInstance* Card,
		const FCardZoneMutationResult& CommitResult
	)
	{
		FBattleEvent Event;
		Event.Type = EBattleEventType::CardExhausted;
		Event.CardExhaustedPayload.Card = Card;
		Event.CardExhaustedPayload.CardRuntimeId = CommitResult.CardRuntimeId;
		Event.CardExhaustedPayload.CardId = CommitResult.CardId;
		Event.CardExhaustedPayload.FromZone = CommitResult.FromZone;
		Event.CardExhaustedPayload.ToZone = CommitResult.ToZone;
		return Event;
	}

	template <typename T>
	const T* TryGet() const
	{
		return nullptr;
	}

private:
	EBattleEventType Type = EBattleEventType::None;
	FTurnEndedEvent TurnEndedPayload;
	FDeckShuffledEvent DeckShuffledPayload;
	FCardExhaustedEvent CardExhaustedPayload;
};

template <>
inline const FTurnEndedEvent* FBattleEvent::TryGet<FTurnEndedEvent>() const
{
	return Type == EBattleEventType::TurnEnded ? &TurnEndedPayload : nullptr;
}

template <>
inline const FDeckShuffledEvent* FBattleEvent::TryGet<FDeckShuffledEvent>() const
{
	return Type == EBattleEventType::DeckShuffled ? &DeckShuffledPayload : nullptr;
}

template <>
inline const FCardExhaustedEvent* FBattleEvent::TryGet<FCardExhaustedEvent>() const
{
	return Type == EBattleEventType::CardExhausted ? &CardExhaustedPayload : nullptr;
}
