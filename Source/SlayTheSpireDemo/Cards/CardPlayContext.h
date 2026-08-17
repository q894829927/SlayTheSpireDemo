#pragma once

class ACombatant;
class UCardInstance;
class UDeckRuntime;
class UObject;

struct FCardPlayContext
{
	UCardInstance* Card = nullptr;
	ACombatant* Source = nullptr;
	ACombatant* Target = nullptr;
	UDeckRuntime* Deck = nullptr;
	UObject* ActionOuter = nullptr;
};
