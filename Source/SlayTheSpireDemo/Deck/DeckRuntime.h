#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../Cards/CardTypes.h"
#include "DeckRuntime.generated.h"

class UCardData;
class UCardInstance;

UCLASS()
class SLAYTHESPIREDEMO_API UDeckRuntime : public UObject
{
	GENERATED_BODY()

public:
	void InitializeFromDefinitions(const TArray<TObjectPtr<UCardData>>& Definitions, int32 Seed);

	bool HasCardsInDrawPile() const;
	bool HasCardsInDiscardPile() const;
	bool IsHandFull() const;
	bool IsCardInHand(const UCardInstance* Card) const;
	bool IsCardInPlayArea(const UCardInstance* Card) const;

	bool TryDrawTopCard(UCardInstance*& OutCard);
	UCardInstance* GetFirstHandCard() const;
	bool TryDiscardCard(UCardInstance* Card);
	bool TryMoveHandCardToPlayArea(UCardInstance* Card);
	bool TryReturnPlayAreaCardToHand(UCardInstance* Card);
	bool TryMovePlayAreaCardToDestination(UCardInstance* Card, ECardDestination Destination);
	bool ShuffleDiscardIntoDrawPile();

	int32 GetDrawCount() const;
	int32 GetHandCount() const;
	int32 GetDiscardCount() const;
	int32 GetExhaustCount() const;
	int32 GetPlayAreaCount() const;
	int32 GetRemovedCount() const;

	FString DescribeState() const;
	void LogState(const TCHAR* Context) const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCardInstance>> DrawPile;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCardInstance>> Hand;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCardInstance>> DiscardPile;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCardInstance>> ExhaustPile;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCardInstance>> PlayArea;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCardInstance>> RemovedPile;

	FRandomStream RandomStream;
	int32 InitialSeed = 1337;
	int32 NextRuntimeId = 1;
	int32 MaxHandSize = 10;
};
