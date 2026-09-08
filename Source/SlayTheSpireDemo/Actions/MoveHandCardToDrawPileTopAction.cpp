#include "MoveHandCardToDrawPileTopAction.h"

#include "BattleActionQueue.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Presentation/PresentationCardSnapshotBuilder.h"

void UMoveHandCardToDrawPileTopAction::Initialize(
	UDeckRuntime* InDeck,
	UCardInstance* InCard,
	ACombatant* InPresentationCardSource
)
{
	Deck = InDeck;
	Card = InCard;
	PresentationCardSource = InPresentationCardSource;
	CommitResult = FCardZoneMutationResult{};
}

void UMoveHandCardToDrawPileTopAction::Execute(UBattleActionQueue* Queue)
{
	CommitResult = FCardZoneMutationResult{};

	if (!IsValid(Queue) || !IsValid(Deck.Get()) || !IsValid(Card.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] MoveHandCardToDrawPileTopAction skipped: invalid Queue, Deck or Card."));
		Finish();
		return;
	}

	if (!Deck->IsCardInHand(Card.Get()))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] MoveHandCardToDrawPileTopAction skipped: exact target %s is no longer in Hand."),
			*Card->GetDebugLabel()
		);
		Finish();
		return;
	}

	CommitResult = Deck->TryMoveHandCardToDrawPileTopCommit(Card.Get());
	if (!CommitResult.bCommitted)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] MoveHandCardToDrawPileTopAction failed to commit exact target %s."),
			*Card->GetDebugLabel()
		);
		Finish();
		return;
	}

	if (CommitResult.CardRuntimeId != Card->GetRuntimeId()
		|| CommitResult.CardId != Card->GetCardId()
		|| CommitResult.FromZone != ECardZone::Hand
		|| CommitResult.ToZone != ECardZone::DrawPile)
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("MoveHandCardToDrawPileTopAction committed inconsistent exact-card facts for %s."),
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
			UE_LOG(LogTemp, Warning, TEXT("[Presentation] Hand-to-DrawPileTop commit could not freeze a trustworthy card payload."));
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
				UE_LOG(LogTemp, Warning, TEXT("[Presentation] Hand-to-DrawPileTop CardZoneChanged append failed; Gameplay commit remains authoritative."));
			}
		}
	}

	Finish();
}
