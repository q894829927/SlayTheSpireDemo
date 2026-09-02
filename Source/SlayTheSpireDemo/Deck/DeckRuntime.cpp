#include "DeckRuntime.h"

#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"

namespace
{
	FString DescribeCards(const TArray<TObjectPtr<UCardInstance>>& Cards)
	{
		TArray<FString> Parts;
		Parts.Reserve(Cards.Num());

		for (const TObjectPtr<UCardInstance>& Card : Cards)
		{
			Parts.Add(IsValid(Card.Get()) ? Card->GetDebugLabel() : TEXT("InvalidCard"));
		}

		return FString::Join(Parts, TEXT(", "));
	}

	int32 FindCardIndex(const TArray<TObjectPtr<UCardInstance>>& Cards, const UCardInstance* Card)
	{
		return Cards.IndexOfByPredicate(
			[Card](const TObjectPtr<UCardInstance>& Entry)
			{
				return Entry.Get() == Card;
			}
		);
	}

	FCardZoneMutationResult MakeZoneResult(
		UCardInstance* Card,
		ECardZone FromZone,
		ECardZone ToZone,
		int32 FromIndex,
		int32 ToIndex
	)
	{
		FCardZoneMutationResult Result;
		if (!IsValid(Card))
		{
			return Result;
		}
		Result.bCommitted = true;
		Result.CardRuntimeId = Card->GetRuntimeId();
		Result.CardId = Card->GetCardId();
		Result.FromZone = FromZone;
		Result.ToZone = ToZone;
		Result.FromIndex = FromIndex;
		Result.ToIndex = ToIndex;
		return Result;
	}
}

void UDeckRuntime::InitializeFromDefinitions(const TArray<TObjectPtr<UCardData>>& Definitions, int32 Seed)
{
	DrawPile.Reset();
	Hand.Reset();
	DiscardPile.Reset();
	ExhaustPile.Reset();
	PlayArea.Reset();
	RemovedPile.Reset();

	InitialSeed = Seed;
	NextRuntimeId = 1;
	RandomStream.Initialize(InitialSeed);

	for (const TObjectPtr<UCardData>& DefinitionPtr : Definitions)
	{
		UCardData* Definition = DefinitionPtr.Get();
		if (!IsValid(Definition))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Deck] Skipped null card definition while building runtime deck."));
			continue;
		}

		UCardInstance* Card = NewObject<UCardInstance>(this);
		Card->Initialize(Definition, NextRuntimeId++);
		DrawPile.Add(Card);

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Deck] Created runtime card %s from definition %s."),
			*Card->GetDebugLabel(),
			*GetNameSafe(Definition)
		);
	}

	ShuffleDrawPileWithBattleRng();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Deck] Runtime deck initialized and initially shuffled. Seed=%d Cards=%d DrawPile top is the array end."),
		InitialSeed,
		DrawPile.Num()
	);
	LogState(TEXT("Initial"));
}

bool UDeckRuntime::HasCardsInDrawPile() const
{
	return DrawPile.Num() > 0;
}

bool UDeckRuntime::HasCardsInDiscardPile() const
{
	return DiscardPile.Num() > 0;
}

bool UDeckRuntime::IsHandFull() const
{
	return Hand.Num() >= MaxHandSize;
}

bool UDeckRuntime::IsCardInHand(const UCardInstance* Card) const
{
	return IsValid(Card) && FindCardIndex(Hand, Card) != INDEX_NONE;
}

bool UDeckRuntime::IsCardInPlayArea(const UCardInstance* Card) const
{
	return IsValid(Card) && FindCardIndex(PlayArea, Card) != INDEX_NONE;
}

FCardZoneMutationResult UDeckRuntime::TryDrawTopCardCommit(UCardInstance*& OutCard)
{
	OutCard = nullptr;
	FCardZoneMutationResult Result;

	if (!HasCardsInDrawPile() || IsHandFull())
	{
		return Result;
	}

	const int32 FromIndex = DrawPile.Num() - 1;
	UCardInstance* Card = DrawPile[FromIndex].Get();
	if (!IsValid(Card))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Deck] Draw failed: top DrawPile card is invalid; deck state was not mutated."));
		return Result;
	}

	const int32 ToIndex = Hand.Num();
	DrawPile.RemoveAt(FromIndex);
	Hand.Add(Card);
	OutCard = Card;
	Result = MakeZoneResult(Card, ECardZone::DrawPile, ECardZone::Hand, FromIndex, ToIndex);

	UE_LOG(LogTemp, Log, TEXT("[Deck] Drew %s from DrawPile to Hand."), *Card->GetDebugLabel());
	LogState(TEXT("AfterDraw"));
	return Result;
}

FCardZoneMutationResult UDeckRuntime::TryDiscardCardCommit(UCardInstance* Card)
{
	FCardZoneMutationResult Result;
	if (!IsValid(Card))
	{
		return Result;
	}

	const int32 FromIndex = FindCardIndex(Hand, Card);
	if (FromIndex == INDEX_NONE)
	{
		return Result;
	}

	const int32 ToIndex = DiscardPile.Num();
	Hand.RemoveAt(FromIndex);
	DiscardPile.Add(Card);
	Result = MakeZoneResult(Card, ECardZone::Hand, ECardZone::DiscardPile, FromIndex, ToIndex);

	UE_LOG(LogTemp, Log, TEXT("[Deck] Discarded %s from Hand to DiscardPile."), *Card->GetDebugLabel());
	LogState(TEXT("AfterDiscard"));
	return Result;
}

FCardZoneMutationResult UDeckRuntime::TryMoveHandCardToPlayAreaCommit(UCardInstance* Card)
{
	FCardZoneMutationResult Result;
	if (!IsValid(Card))
	{
		return Result;
	}

	const int32 FromIndex = FindCardIndex(Hand, Card);
	if (FromIndex == INDEX_NONE)
	{
		return Result;
	}

	const int32 ToIndex = PlayArea.Num();
	Hand.RemoveAt(FromIndex);
	PlayArea.Add(Card);
	Result = MakeZoneResult(Card, ECardZone::Hand, ECardZone::PlayArea, FromIndex, ToIndex);

	UE_LOG(LogTemp, Log, TEXT("[Deck] Moved %s from Hand to PlayArea."), *Card->GetDebugLabel());
	LogState(TEXT("AfterBeginPlay"));
	return Result;
}

FCardZoneMutationResult UDeckRuntime::TryReturnPlayAreaCardToHandAtIndexCommit(UCardInstance* Card, int32 HandIndex)
{
	FCardZoneMutationResult Result;
	if (!IsValid(Card) || IsHandFull())
	{
		return Result;
	}

	const int32 FromIndex = FindCardIndex(PlayArea, Card);
	if (FromIndex == INDEX_NONE || HandIndex < 0 || HandIndex > Hand.Num())
	{
		return Result;
	}

	PlayArea.RemoveAt(FromIndex);
	Hand.Insert(Card, HandIndex);
	Result = MakeZoneResult(Card, ECardZone::PlayArea, ECardZone::Hand, FromIndex, HandIndex);

	UE_LOG(LogTemp, Warning, TEXT("[Deck] Rolled back %s from PlayArea to Hand index %d."), *Card->GetDebugLabel(), HandIndex);
	LogState(TEXT("AfterPlayRollback"));
	return Result;
}

FCardZoneMutationResult UDeckRuntime::TryMovePlayAreaCardToDestinationCommit(UCardInstance* Card, ECardDestination Destination)
{
	FCardZoneMutationResult Result;
	if (!IsValid(Card))
	{
		return Result;
	}

	const int32 FromIndex = FindCardIndex(PlayArea, Card);
	if (FromIndex == INDEX_NONE)
	{
		return Result;
	}

	TArray<TObjectPtr<UCardInstance>>* DestinationPile = nullptr;
	ECardZone ToZone = ECardZone::DiscardPile;
	const TCHAR* DestinationName = TEXT("Unknown");

	switch (Destination)
	{
	case ECardDestination::Discard:
		DestinationPile = &DiscardPile;
		ToZone = ECardZone::DiscardPile;
		DestinationName = TEXT("DiscardPile");
		break;
	case ECardDestination::Exhaust:
		DestinationPile = &ExhaustPile;
		ToZone = ECardZone::ExhaustPile;
		DestinationName = TEXT("ExhaustPile");
		break;
	case ECardDestination::Removed:
		DestinationPile = &RemovedPile;
		ToZone = ECardZone::RemovedPile;
		DestinationName = TEXT("RemovedPile");
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("[Deck] Unsupported destination for %s; deck state was not mutated."), *Card->GetDebugLabel());
		return Result;
	}

	const int32 ToIndex = DestinationPile->Num();
	PlayArea.RemoveAt(FromIndex);
	DestinationPile->Add(Card);
	Result = MakeZoneResult(Card, ECardZone::PlayArea, ToZone, FromIndex, ToIndex);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Deck] Finished %s: PlayArea -> %s."),
		*Card->GetDebugLabel(),
		DestinationName
	);
	LogState(TEXT("AfterFinishPlay"));
	return Result;
}

FDeckShuffleCommitResult UDeckRuntime::ShuffleDiscardIntoDrawPileCommit()
{
	FDeckShuffleCommitResult Result;
	Result.DrawCountBefore = DrawPile.Num();
	Result.DiscardCountBefore = DiscardPile.Num();

	// Gameplay shuffle semantics follow the source-game draw loop: an empty
	// DrawPile may commit one shuffle attempt even when the DiscardPile is also
	// empty. This zero-card shuffle is still a committed gameplay fact and may
	// trigger shuffle-reactive mechanics such as Sundial. A non-empty DrawPile
	// remains an invalid shuffle boundary.
	if (DrawPile.Num() != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Deck] Shuffle skipped: DrawPile is not empty (Draw=%d)."), DrawPile.Num());
		Result.DrawCountAfter = DrawPile.Num();
		Result.DiscardCountAfter = DiscardPile.Num();
		return Result;
	}

	Result.MovedCardCount = DiscardPile.Num();
	if (DiscardPile.Num() > 0)
	{
		DrawPile.Append(DiscardPile);
		DiscardPile.Reset();
		ShuffleDrawPileWithBattleRng();
	}

	Result.bCommitted = true;
	Result.DrawCountAfter = DrawPile.Num();
	Result.DiscardCountAfter = DiscardPile.Num();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Deck] Gameplay shuffle committed. MovedCards=%d Draw=%d Discard=%d."),
		Result.MovedCardCount,
		Result.DrawCountAfter,
		Result.DiscardCountAfter
	);
	LogState(TEXT("AfterShuffle"));
	return Result;
}

bool UDeckRuntime::TryDrawTopCard(UCardInstance*& OutCard)
{
	return TryDrawTopCardCommit(OutCard).bCommitted;
}

UCardInstance* UDeckRuntime::GetFirstHandCard() const
{
	return Hand.Num() > 0 ? Hand[0].Get() : nullptr;
}

bool UDeckRuntime::TryDiscardCard(UCardInstance* Card)
{
	return TryDiscardCardCommit(Card).bCommitted;
}

bool UDeckRuntime::TryMoveHandCardToPlayArea(UCardInstance* Card)
{
	return TryMoveHandCardToPlayAreaCommit(Card).bCommitted;
}

bool UDeckRuntime::TryReturnPlayAreaCardToHand(UCardInstance* Card)
{
	return TryReturnPlayAreaCardToHandAtIndexCommit(Card, Hand.Num()).bCommitted;
}

bool UDeckRuntime::TryMovePlayAreaCardToDestination(UCardInstance* Card, ECardDestination Destination)
{
	return TryMovePlayAreaCardToDestinationCommit(Card, Destination).bCommitted;
}

bool UDeckRuntime::ShuffleDiscardIntoDrawPile()
{
	return ShuffleDiscardIntoDrawPileCommit().bCommitted;
}

const TArray<TObjectPtr<UCardInstance>>& UDeckRuntime::GetDrawCards() const
{
	return DrawPile;
}

const TArray<TObjectPtr<UCardInstance>>& UDeckRuntime::GetHandCards() const
{
	return Hand;
}

const TArray<TObjectPtr<UCardInstance>>& UDeckRuntime::GetDiscardCards() const
{
	return DiscardPile;
}

const TArray<TObjectPtr<UCardInstance>>& UDeckRuntime::GetExhaustCards() const
{
	return ExhaustPile;
}

const TArray<TObjectPtr<UCardInstance>>& UDeckRuntime::GetPlayAreaCards() const
{
	return PlayArea;
}

const TArray<TObjectPtr<UCardInstance>>& UDeckRuntime::GetRemovedCards() const
{
	return RemovedPile;
}

int32 UDeckRuntime::GetDrawCount() const
{
	return DrawPile.Num();
}

int32 UDeckRuntime::GetHandCount() const
{
	return Hand.Num();
}

int32 UDeckRuntime::GetDiscardCount() const
{
	return DiscardPile.Num();
}

int32 UDeckRuntime::GetExhaustCount() const
{
	return ExhaustPile.Num();
}

int32 UDeckRuntime::GetPlayAreaCount() const
{
	return PlayArea.Num();
}

int32 UDeckRuntime::GetRemovedCount() const
{
	return RemovedPile.Num();
}

int32 UDeckRuntime::GetMaxHandSize() const
{
	return MaxHandSize;
}

FString UDeckRuntime::DescribeState() const
{
	return FString::Printf(
		TEXT("Draw=%d [%s] Hand=%d [%s] Discard=%d [%s] Exhaust=%d [%s] Play=%d [%s] Removed=%d [%s]"),
		DrawPile.Num(),
		*DescribeCards(DrawPile),
		Hand.Num(),
		*DescribeCards(Hand),
		DiscardPile.Num(),
		*DescribeCards(DiscardPile),
		ExhaustPile.Num(),
		*DescribeCards(ExhaustPile),
		PlayArea.Num(),
		*DescribeCards(PlayArea),
		RemovedPile.Num(),
		*DescribeCards(RemovedPile)
	);
}

void UDeckRuntime::LogState(const TCHAR* Context) const
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Deck] %s: %s | DrawPile order is bottom -> top."),
		Context,
		*DescribeState()
	);
}

void UDeckRuntime::ShuffleDrawPileWithBattleRng()
{
	for (int32 Index = DrawPile.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = RandomStream.RandRange(0, Index);
		if (SwapIndex != Index)
		{
			DrawPile.Swap(Index, SwapIndex);
		}
	}
}
