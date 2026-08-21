#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "../Battle/BattleState.h"
#include "../Deck/DeckMutationTypes.h"
#include "../Modifiers/ModifierTypes.h"
#include "../UI/BattleHUDTypes.h"
#include "PresentationTypes.generated.h"

UENUM(BlueprintType)
enum class EPresentationResolutionOrigin : uint8
{
	BattleStart UMETA(DisplayName = "Battle Start"),
	PlayCard UMETA(DisplayName = "Play Card"),
	EndTurn UMETA(DisplayName = "End Turn"),
	System UMETA(DisplayName = "System")
};

UENUM(BlueprintType)
enum class EBattlePresentationRecordType : uint8
{
	None UMETA(DisplayName = "None"),
	ResolutionFault UMETA(DisplayName = "Resolution Fault"),
	Damage UMETA(DisplayName = "Damage"),
	BlockChanged UMETA(DisplayName = "Block Changed"),
	Victory UMETA(DisplayName = "Victory"),
	Defeat UMETA(DisplayName = "Defeat"),
	CardPlayed UMETA(DisplayName = "Card Played"),
	EnergyChanged UMETA(DisplayName = "Energy Changed"),
	CardZoneChanged UMETA(DisplayName = "Card Zone Changed"),
	DeckShuffled UMETA(DisplayName = "Deck Shuffled")
};

UENUM(BlueprintType)
enum class EBlockPresentationReason : uint8
{
	Gain UMETA(DisplayName = "Gain"),
	TurnStartClear UMETA(DisplayName = "Turn Start Clear")
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FDamagePresentationPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	FName SourcePresentationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	FName TargetPresentationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	EDamageKind DamageKind = EDamageKind::Attack;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	int32 IncomingDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	int32 HPBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	int32 HPAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	int32 BlockBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	int32 BlockAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	int32 BlockedDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	int32 HPDamage = 0;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FBlockChangedPresentationPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Block")
	FName SourcePresentationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Block")
	FName TargetPresentationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Block")
	EBlockPresentationReason Reason = EBlockPresentationReason::Gain;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Block")
	int32 BlockBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Block")
	int32 BlockAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Block")
	int32 BlockDelta = 0;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FPresentationCardSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	int32 RuntimeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	int32 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	ECardType CardType = ECardType::Attack;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	ECardTargetType TargetType = ECardTargetType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	TObjectPtr<UTexture2D> CardArt = nullptr;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FCardPlayedPresentationPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	FPresentationCardSnapshot Card;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	FName SourcePresentationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	FName TargetPresentationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	int32 HandIndexBefore = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	int32 PlayAreaIndexAfter = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Energy")
	int32 EnergyBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Energy")
	int32 EnergyAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Energy")
	int32 CostPaid = 0;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FEnergyChangedPresentationPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Energy")
	int32 EnergyBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Energy")
	int32 EnergyAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Energy")
	int32 Delta = 0;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FCardZoneChangedPresentationPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	FPresentationCardSnapshot Card;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	ECardZone FromZone = ECardZone::DrawPile;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	ECardZone ToZone = ECardZone::Hand;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	int32 FromIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	int32 ToIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FDeckShuffledPresentationPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Deck")
	int32 MovedCardCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Deck")
	int32 DrawCountBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Deck")
	int32 DrawCountAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Deck")
	int32 DiscardCountBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Deck")
	int32 DiscardCountAfter = 0;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FPresentationPlaybackToken
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	int64 ResolutionId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	int64 PresentationSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	int64 LocalPlaybackGeneration = 0;

	bool operator==(const FPresentationPlaybackToken& Other) const
	{
		return BattleId == Other.BattleId
			&& ResolutionId == Other.ResolutionId
			&& PresentationSequence == Other.PresentationSequence
			&& LocalPlaybackGeneration == Other.LocalPlaybackGeneration;
	}

	bool operator!=(const FPresentationPlaybackToken& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FPresentationRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	int64 ResolutionId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	int64 PresentationSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	EBattlePresentationRecordType Type = EBattlePresentationRecordType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Damage")
	FDamagePresentationPayload Damage;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Block")
	FBlockChangedPresentationPayload BlockChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	FCardPlayedPresentationPayload CardPlayed;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Energy")
	FEnergyChangedPresentationPayload EnergyChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Card")
	FCardZoneChangedPresentationPayload CardZoneChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Deck")
	FDeckShuffledPresentationPayload DeckShuffled;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Fault")
	FString FaultReason;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Fault")
	int32 FaultExecutedActionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Fault")
	FName FaultLastActionName = NAME_None;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FPresentationStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Revision")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Revision")
	int64 StateRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|State")
	EBattleState BattleState = EBattleState::BattleStart;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|State")
	EBattleHUDOutcome Outcome = EBattleHUDOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Combat")
	int32 Energy = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Combat")
	int32 MaxEnergy = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|State")
	bool bCanEndTurn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Combat")
	FBattleHUDCombatantView Player;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Combat")
	FBattleHUDCombatantView Enemy;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Cards")
	TArray<FBattleHUDCardView> HandCards;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Cards")
	int32 DrawCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Cards")
	int32 DiscardCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Cards")
	int32 ExhaustCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Intent")
	FBattleHUDIntentView EnemyIntent;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FPresentationResolutionEnvelope
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	int64 ResolutionId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	EPresentationResolutionOrigin Origin = EPresentationResolutionOrigin::System;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	int64 FinalStateRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	TArray<FPresentationRecord> Records;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation")
	FPresentationStateSnapshot FinalSnapshot;
};
