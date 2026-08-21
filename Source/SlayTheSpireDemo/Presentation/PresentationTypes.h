#pragma once

#include "CoreMinimal.h"
#include "../Battle/BattleState.h"
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
	ResolutionFault UMETA(DisplayName = "Resolution Fault")
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

	// A2A owns only fault transport. Later slices add typed committed payloads at
	// their owning commit boundaries rather than widening this into a generic
	// Variant/GameplayTag payload container.
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
