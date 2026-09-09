#include "SelectionCandidateSource.h"
#include "../Deck/DeckRuntime.h"
#include "../Cards/CardInstance.h"

ESelectionCandidateBuildStatus USelectionCandidateSource::BuildCandidates(TArray<FSelectionCandidate>& OutCandidates) const
{
	OutCandidates.Reset();
	return ESelectionCandidateBuildStatus::InvalidRuntimeDependency;
}

void UCurrentHandSelectionSource::Initialize(UDeckRuntime* InDeck)
{
	Deck = InDeck;
}

ESelectionCandidateBuildStatus UCurrentHandSelectionSource::BuildCandidates(TArray<FSelectionCandidate>& OutCandidates) const
{
	OutCandidates.Reset();
	if (!IsValid(Deck)) return ESelectionCandidateBuildStatus::InvalidRuntimeDependency;
	for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
	{
		if (!IsValid(Card))
		{
			OutCandidates.Reset();
			return ESelectionCandidateBuildStatus::InvalidRuntimeDependency;
		}
		FSelectionCandidate Candidate;
		Candidate.RuntimeObject = Card;
		Candidate.RuntimeSequence = Card->GetRuntimeId();
		Candidate.SelectionKey = Card->GetCardId();
		OutCandidates.Add(Candidate);
	}
	return OutCandidates.IsEmpty() ? ESelectionCandidateBuildStatus::NoCandidates : ESelectionCandidateBuildStatus::Success;
}
