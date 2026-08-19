#pragma once

#include "CoreMinimal.h"

class ABattleManager;
class ACombatant;
class UBattleEventDispatcher;
class UCardInstance;
class UDeckRuntime;
class UObject;

struct FCardPlayContext
{
	ABattleManager* Battle = nullptr;
	UCardInstance* Card = nullptr;
	ACombatant* Source = nullptr;
	ACombatant* Target = nullptr;
	UDeckRuntime* Deck = nullptr;
	UBattleEventDispatcher* EventDispatcher = nullptr;
	TArray<ACombatant*> EventCombatants;
	UObject* ActionOuter = nullptr;
};
