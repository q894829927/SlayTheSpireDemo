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

	// Battle setup randomization is deterministic and consumes the same
	// battle-scoped RNG stream used by future gameplay reshuffles. This is setup,
	// not a ShuffleDeckAction commit, so it intentionally emits no DeckShuffled.
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

bool UDeckRuntime::TryDrawTopCard(UCardInstance*& OutCard)
{
	OutCard = nullptr;

	if (!HasCardsInDrawPile() || IsHandFull())
	{
		return false;
	}

	TObjectPtr<UCardInstance> CardPtr = DrawPile.Pop();
	UCardInstance* Card = CardPtr.Get();
	if (!IsValid(Card))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Deck] Draw failed: top DrawPile card is invalid."));
		return false;
	}

	Hand.Add(Card);
	OutCard = Card;

	UE_LOG(LogTemp, Log, TEXT("[Deck] Drew %s from DrawPile to Hand."), *Card->GetDebugLabel());
	LogState(TEXT("AfterDraw"));
	return true;
}

UCardInstance* UDeckRuntime::GetFirstHandCard() const
{
	return Hand.Num() > 0 ? Hand[0].Get() : nullptr;
}

bool UDeckRuntime::TryDiscardCard(UCardInstance* Card)
{
	if (!IsValid(Card))
	{
		return false;
	}

	const int32 HandIndex = FindCardIndex(Hand, Card);
	if (HandIndex == INDEX_NONE)
	{
		return false;
	}

	Hand.RemoveAt(HandIndex);
	DiscardPile.Add(Card);

	UE_LOG(LogTemp, Log, TEXT("[Deck] Discarded %s from Hand to DiscardPile."), *Card->GetDebugLabel());
	LogState(TEXT("AfterDiscard"));
	return true;
}

bool UDeckRuntime::TryMoveHandCardToPlayArea(UCardInstance* Card)
{
	if (!IsValid(Card))
	{
		return false;
	}

	const int32 HandIndex = FindCardIndex(Hand, Card);
	if (HandIndex == INDEX_NONE)
	{
		return false;
	}

	Hand.RemoveAt(HandIndex);
	PlayArea.Add(Card);

	UE_LOG(LogTemp, Log, TEXT("[Deck] Moved %s from Hand to PlayArea."), *Card->GetDebugLabel());
	LogState(TEXT("AfterBeginPlay"));
	return true;
}

bool UDeckRuntime::TryReturnPlayAreaCardToHand(UCardInstance* Card)
{
	if (!IsValid(Card) || IsHandFull())
	{
		return false;
	}

	const int32 PlayIndex = FindCardIndex(PlayArea, Card);
	if (PlayIndex == INDEX_NONE)
	{
		return false;
	}

	PlayArea.RemoveAt(PlayIndex);
	Hand.Add(Card);

	UE_LOG(LogTemp, Warning, TEXT("[Deck] Rolled back %s from PlayArea to Hand."), *Card->GetDebugLabel());
	LogState(TEXT("AfterPlayRollback"));
	return true;
}

bool UDeckRuntime::TryMovePlayAreaCardToDestination(UCardInstance* Card, ECardDestination Destination)
{
	if (!IsValid(Card))
	{
		return false;
	}

	const int32 PlayIndex = FindCardIndex(PlayArea, Card);
	if (PlayIndex == INDEX_NONE)
	{
		return false;
	}

	PlayArea.RemoveAt(PlayIndex);

	const TCHAR* DestinationName = TEXT("Unknown");
	switch (Destination)
	{
	case ECardDestination::Discard:
		DiscardPile.Add(Card);
		DestinationName = TEXT("DiscardPile");
		break;

	case ECardDestination::Exhaust:
		ExhaustPile.Add(Card);
		DestinationName = TEXT("ExhaustPile");
		break;

	case ECardDestination::Removed:
		RemovedPile.Add(Card);
		DestinationName = TEXT("RemovedPile");
		break;

	default:
		PlayArea.Add(Card);
		UE_LOG(LogTemp, Warning, TEXT("[Deck] Unsupported destination for %s; card restored to PlayArea."), *Card->GetDebugLabel());
		return false;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Deck] Finished %s: PlayArea -> %s."),
		*Card->GetDebugLabel(),
		DestinationName
	);
	LogState(TEXT("AfterFinishPlay"));
	return true;
}

bool UDeckRuntime::ShuffleDiscardIntoDrawPile()
{
	if (DiscardPile.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Deck] Shuffle skipped: DiscardPile is empty."));
		return false;
	}

	if (DrawPile.Num() != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Deck] Shuffle skipped: DrawPile is not empty (Draw=%d)."), DrawPile.Num());
		return false;
	}

	DrawPile.Append(DiscardPile);
	DiscardPile.Reset();
	ShuffleDrawPileWithBattleRng();

	UE_LOG(LogTemp, Log, TEXT("[Deck] Shuffled DiscardPile into DrawPile using the battle RNG stream."));
	LogState(TEXT("AfterShuffle"));
	return true;
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
