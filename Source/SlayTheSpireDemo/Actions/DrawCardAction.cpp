#include "DrawCardAction.h"

#include "BattleActionQueue.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Presentation/PresentationCardSnapshotBuilder.h"

void UDrawCardAction::Initialize(UDeckRuntime* InDeck)
{
	Initialize(InDeck, nullptr);
}

void UDrawCardAction::Initialize(UDeckRuntime* InDeck, ACombatant* InPresentationCardSource)
{
	Deck = InDeck;
	PresentationCardSource = InPresentationCardSource;
}

void UDrawCardAction::Execute(UBattleActionQueue* /*Queue*/)
{
	UDeckRuntime* RuntimeDeck = Deck.Get();
	if (!IsValid(RuntimeDeck))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DrawCardAction skipped: invalid Deck."));
		Finish();
		return;
	}

	if (RuntimeDeck->IsHandFull())
	{
		UE_LOG(LogTemp, Log, TEXT("[Action] DrawCardAction skipped: Hand is full."));
		Finish();
		return;
	}

	UCardInstance* DrawnCard = nullptr;
	const FCardZoneMutationResult CommitResult = RuntimeDeck->TryDrawTopCardCommit(DrawnCard);
	if (!CommitResult.bCommitted)
	{
		UE_LOG(LogTemp, Log, TEXT("[Action] DrawCardAction ended without commit: DrawPile has no drawable card."));
		Finish();
		return;
	}

	const FPresentationRecordWriter& Writer = GetPresentationRecordWriter();
	if (Writer.IsAvailable())
	{
		FPresentationCardSnapshot CardSnapshot;
		if (!PresentationCardSnapshot::TryBuild(DrawnCard, PresentationCardSource.Get(), CardSnapshot)
			|| CardSnapshot.RuntimeId != CommitResult.CardRuntimeId
			|| CardSnapshot.CardId != CommitResult.CardId)
		{
			Writer.InvalidateCurrentResolution();
			UE_LOG(LogTemp, Warning, TEXT("[Presentation] Draw commit could not freeze a trustworthy card payload."));
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
				UE_LOG(LogTemp, Warning, TEXT("[Presentation] Draw CardZoneChanged append failed; Gameplay draw remains authoritative."));
			}
		}
	}

	Finish();
}
