#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DeckTypes.h"
#include "DeckRuntime.generated.h"

UCLASS()
class SLAYTHESPIREDEMO_API UDeckRuntime : public UObject
{
	GENERATED_BODY()

public:
	void InitializeDebugDeck(int32 Seed);

	bool HasCardsInDrawPile() const;
	bool HasCardsInDiscardPile() const;
	bool IsHandFull() const;

	bool TryDrawTopCard(FDeckCardToken& OutCard);
	bool GetFirstHandCard(FDeckCardToken& OutCard) const;
	bool TryDiscardCardByRuntimeId(int32 RuntimeId, FDeckCardToken& OutCard);
	bool ShuffleDiscardIntoDrawPile();

	int32 GetDrawCount() const;
	int32 GetHandCount() const;
	int32 GetDiscardCount() const;
	int32 GetExhaustCount() const;

	FString DescribeState() const;
	void LogState(const TCHAR* Context) const;

private:
	UPROPERTY(Transient)
	TArray<FDeckCardToken> DrawPile;

	UPROPERTY(Transient)
	TArray<FDeckCardToken> Hand;

	UPROPERTY(Transient)
	TArray<FDeckCardToken> DiscardPile;

	UPROPERTY(Transient)
	TArray<FDeckCardToken> ExhaustPile;

	FRandomStream RandomStream;
	int32 InitialSeed = 1337;
	int32 MaxHandSize = 10;
};
