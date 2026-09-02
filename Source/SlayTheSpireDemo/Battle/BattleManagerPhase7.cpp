#include "BattleManager.h"

#include "../Deck/DeckRuntime.h"

bool ABattleManager::IsAuthoritativeDeckRuntime(const UDeckRuntime* Deck) const
{
	return IsValid(DeckRuntime.Get()) && DeckRuntime.Get() == Deck;
}
