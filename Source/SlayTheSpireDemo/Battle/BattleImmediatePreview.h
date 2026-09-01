#pragma once

#include "CoreMinimal.h"
#include "BattleRequestTypes.h"
#include "BattleImmediatePreview.generated.h"

UENUM(BlueprintType)
enum class EImmediatePreviewOperationType : uint8
{
	Damage,
	Block
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FImmediatePreviewOperation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview")
	int32 EffectIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview")
	FName SemanticArgumentName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview")
	EImmediatePreviewOperationType Type = EImmediatePreviewOperationType::Damage;

	// Damage: resolved incoming damage per hit before Block absorption.
	// Block: resolved current self-Block amount.
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview")
	int32 ResolvedAmount = 0;

	// Damage preserves the immutable authored fixed hit count. Block uses 1.
	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview")
	int32 HitCount = 1;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FImmediateCardPreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview|Identity")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview|Identity")
	int64 StateRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview|Identity")
	int32 CardRuntimeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview|Identity")
	FName SourcePresentationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview|Identity")
	FName TargetPresentationId = NAME_None;

	// Reuses the authoritative Gameplay validation vocabulary. A3-3 owns
	// populating this field through QueryCardPlayability / QueryPlayCard.
	FGameplayValidationResult Validation;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview|Energy")
	int32 EnergyBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview|Energy")
	int32 EffectiveCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview|Energy")
	bool bHasEnergyAfter = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview|Energy")
	int32 EnergyAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Preview")
	TArray<FImmediatePreviewOperation> Operations;
};
