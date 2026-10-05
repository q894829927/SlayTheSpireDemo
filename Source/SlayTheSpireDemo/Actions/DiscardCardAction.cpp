#include "DiscardCardAction.h"

#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Presentation/PresentationCardSnapshotBuilder.h"

void UDiscardCardAction::Initialize(UDeckRuntime* InDeck, UCardInstance* InCard)
{
	Initialize(InDeck, InCard, nullptr);
}

void UDiscardCardAction::Initialize(
	UDeckRuntime* InDeck,
	UCardInstance* InCard,
	ACombatant* InPresentationCardSource
)
{
	Deck = InDeck;
	Card = InCard;
	PresentationCardSource = InPresentationCardSource;
	TurnEndDiscardGroup = FPresentationGroupTag{};
	TurnEndDiscardRuntimeId = INDEX_NONE;
}

void UDiscardCardAction::SetTurnEndDiscardPresentationGroup(const FPresentationGroupTag& Group, int32 RuntimeId)
{
	if (Group.IsValid() && Group.Kind == EPresentationGroupKind::TurnEndDiscard
		&& Group.ExpectedMemberCount > 1 && IsValid(Card) && Card->GetRuntimeId() == RuntimeId)
	{
		TurnEndDiscardGroup = Group;
		TurnEndDiscardRuntimeId = RuntimeId;
	}
}

void UDiscardCardAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Deck.Get()) || !IsValid(Card.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DiscardCardAction skipped: invalid Deck or Card."));
		Finish();
		return;
	}

	const FCardZoneMutationResult CommitResult = Deck->TryDiscardCardCommit(Card.Get());
	if (!CommitResult.bCommitted)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] DiscardCardAction skipped: %s is no longer in Hand."),
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
			UE_LOG(LogTemp, Warning, TEXT("[Presentation] Discard commit could not freeze a trustworthy card payload."));
		}
		else
		{
			FPresentationRecord Record;
			Record.Type = EBattlePresentationRecordType::CardZoneChanged;
			TryGetSelectionPresentationGroupForRuntimeId(CommitResult.CardRuntimeId, Record.Group);
			if (TurnEndDiscardGroup.IsValid() && CommitResult.CardRuntimeId == TurnEndDiscardRuntimeId)
				Record.Group = TurnEndDiscardGroup;
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
