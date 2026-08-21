#pragma once

#include "CoreMinimal.h"
#include "../Battle/BattleState.h"
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
	Defeat UMETA(DisplayName = "Defeat")
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
