#pragma once

#include "CoreMinimal.h"
#include "CombatantMutationTypes.generated.h"

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FDamageCommitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	bool bCommitted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	int32 IncomingDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	int32 HPBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	int32 HPAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	int32 BlockBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	int32 BlockAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	int32 BlockedDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	int32 HPDamage = 0;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FBlockCommitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	bool bCommitted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	int32 BlockBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	int32 BlockAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Mutation")
	int32 BlockDelta = 0;
};
