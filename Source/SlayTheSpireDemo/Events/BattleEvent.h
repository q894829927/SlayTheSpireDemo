#pragma once

#include "CoreMinimal.h"

class ACombatant;
class UDeckRuntime;

enum class EBattleEventType : uint8
{
	None,
	TurnEnded,
	DeckShuffled
};

struct FTurnEndedEvent
{
	ACombatant* TurnOwner = nullptr;
};

struct FDeckShuffledEvent
{
	UDeckRuntime* Deck = nullptr;
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

	template <typename T>
	const T* TryGet() const
	{
		return nullptr;
	}

private:
	EBattleEventType Type = EBattleEventType::None;
	FTurnEndedEvent TurnEndedPayload;
	FDeckShuffledEvent DeckShuffledPayload;
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
