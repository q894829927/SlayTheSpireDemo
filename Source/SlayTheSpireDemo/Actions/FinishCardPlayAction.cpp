#include "FinishCardPlayAction.h"

#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
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
}

void UFinishCardPlayAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Deck.Get()) || !IsValid(Card.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] FinishCardPlayAction skipped: invalid Deck or Card."));
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

	Finish();
}
