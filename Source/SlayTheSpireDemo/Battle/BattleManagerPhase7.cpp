#include "BattleManager.h"

#include "../Deck/DeckRuntime.h"
#include "../Presentation/EnergyPresentationRecord.h"

bool ABattleManager::IsAuthoritativeDeckRuntime(const UDeckRuntime* Deck) const
{
	return IsValid(DeckRuntime.Get()) && DeckRuntime.Get() == Deck;
}

void ABattleManager::AppendEnergyChangedPresentationRecord(
	const FEnergyCommitResult& CommitResult,
	const FPresentationRecordWriter& Writer)
{
	EnergyPresentationRecord::AppendCommittedEnergyChanged(CommitResult, Writer);
}
