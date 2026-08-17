#include "DeckRuntime.h"

namespace
{
	FString DescribeCards(const TArray<FDeckCardToken>& Cards)
	{
		TArray<FString> Parts;
		Parts.Reserve(Cards.Num());

		for (const FDeckCardToken& Card : Cards)
		{
			Parts.Add(FString::Printf(TEXT("%s#%d"), *Card.DebugName.ToString(), Card.RuntimeId));
		}

		return FString::Join(Parts, TEXT(", "));
	}
}

void UDeckRuntime::InitializeDebugDeck(int32 Seed)
{
	DrawPile.Reset();
	Hand.Reset();
	DiscardPile.Reset();
	ExhaustPile.Reset();

	InitialSeed = Seed;
	RandomStream.Initialize(InitialSeed);

	FDeckCardToken CardA;
	CardA.RuntimeId = 1;
	CardA.DebugName = TEXT("Card_A");
	DrawPile.Add(CardA);

	FDeckCardToken CardB;
	CardB.RuntimeId = 2;
	CardB.DebugName = TEXT("Card_B");
	DrawPile.Add(CardB);

	FDeckCardToken CardC;
	CardC.RuntimeId = 3;
	CardC.DebugName = TEXT("Card_C");
	DrawPile.Add(CardC);

	UE_LOG(LogTemp, Log, TEXT("[Deck] Debug deck initialized. Seed=%d DrawPile top is the array end."), InitialSeed);
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

bool UDeckRuntime::TryDrawTopCard(FDeckCardToken& OutCard)
{
	if (!HasCardsInDrawPile() || IsHandFull())
	{
		return false;
	}

	OutCard = DrawPile.Pop();
	Hand.Add(OutCard);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Deck] Drew %s#%d from DrawPile to Hand."),
		*OutCard.DebugName.ToString(),
		OutCard.RuntimeId
	);
	LogState(TEXT("AfterDraw"));
	return true;
}

bool UDeckRuntime::GetFirstHandCard(FDeckCardToken& OutCard) const
{
	if (Hand.Num() == 0)
	{
		return false;
	}

	OutCard = Hand[0];
	return true;
}

bool UDeckRuntime::TryDiscardCardByRuntimeId(int32 RuntimeId, FDeckCardToken& OutCard)
{
	const int32 HandIndex = Hand.IndexOfByPredicate(
		[RuntimeId](const FDeckCardToken& Card)
		{
			return Card.RuntimeId == RuntimeId;
		}
	);

	if (HandIndex == INDEX_NONE)
	{
		return false;
	}

	OutCard = Hand[HandIndex];
	Hand.RemoveAt(HandIndex);
	DiscardPile.Add(OutCard);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Deck] Discarded %s#%d from Hand to DiscardPile."),
		*OutCard.DebugName.ToString(),
		OutCard.RuntimeId
	);
	LogState(TEXT("AfterDiscard"));
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
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Deck] Shuffle skipped: DrawPile is not empty (Draw=%d)."),
			DrawPile.Num()
		);
		return false;
	}

	DrawPile.Append(DiscardPile);
	DiscardPile.Reset();

	for (int32 Index = DrawPile.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = RandomStream.RandRange(0, Index);
		if (SwapIndex != Index)
		{
			DrawPile.Swap(Index, SwapIndex);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Deck] Shuffled DiscardPile into DrawPile using the battle RNG stream."));
	LogState(TEXT("AfterShuffle"));
	return true;
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

FString UDeckRuntime::DescribeState() const
{
	return FString::Printf(
		TEXT("Draw=%d [%s] Hand=%d [%s] Discard=%d [%s] Exhaust=%d [%s]"),
		DrawPile.Num(),
		*DescribeCards(DrawPile),
		Hand.Num(),
		*DescribeCards(Hand),
		DiscardPile.Num(),
		*DescribeCards(DiscardPile),
		ExhaustPile.Num(),
		*DescribeCards(ExhaustPile)
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
