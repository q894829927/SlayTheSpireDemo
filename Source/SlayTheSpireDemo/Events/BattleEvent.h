#pragma once

#include "CoreMinimal.h"

class ACombatant;

struct FTurnEndedEvent
{
	ACombatant* TurnOwner = nullptr;
};

struct FBattleEvent
{
	static FBattleEvent MakeTurnEnded(ACombatant* TurnOwner)
	{
		FBattleEvent Event;
		Event.TurnEndedPayload.TurnOwner = TurnOwner;
		return Event;
	}

	template <typename T>
	const T* TryGet() const
	{
		return nullptr;
	}

private:
	FTurnEndedEvent TurnEndedPayload;
};

template <>
inline const FTurnEndedEvent* FBattleEvent::TryGet<FTurnEndedEvent>() const
{
	return &TurnEndedPayload;
}
