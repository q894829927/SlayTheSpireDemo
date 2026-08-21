#pragma once

#include "CoreMinimal.h"
#include "DeckMutationTypes.generated.h"

UENUM(BlueprintType)
enum class ECardZone : uint8
{
	DrawPile UMETA(DisplayName = "Draw Pile"),
	Hand UMETA(DisplayName = "Hand"),
	PlayArea UMETA(DisplayName = "Play Area"),
	DiscardPile UMETA(DisplayName = "Discard Pile"),
	ExhaustPile UMETA(DisplayName = "Exhaust Pile"),
	RemovedPile UMETA(DisplayName = "Removed Pile")
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FCardZoneMutationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	bool bCommitted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	int32 CardRuntimeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	ECardZone FromZone = ECardZone::DrawPile;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	ECardZone ToZone = ECardZone::Hand;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	int32 FromIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	int32 ToIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FDeckShuffleCommitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	bool bCommitted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	int32 MovedCardCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	int32 DrawCountBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	int32 DrawCountAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	int32 DiscardCountBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Deck Mutation")
	int32 DiscardCountAfter = 0;
};
