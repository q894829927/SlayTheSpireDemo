#pragma once

#include "CoreMinimal.h"
#include "EnergyMutationTypes.generated.h"

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FEnergyCommitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Energy")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Energy")
	bool bCommitted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Energy")
	int32 EnergyBefore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Energy")
	int32 EnergyAfter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle|Energy")
	int32 Delta = 0;
};
