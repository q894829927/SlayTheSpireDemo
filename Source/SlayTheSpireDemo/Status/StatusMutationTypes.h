#pragma once

#include "CoreMinimal.h"
#include "StatusMutationTypes.generated.h"

class UStatusData;
class UStatusInstance;

UENUM(BlueprintType)
enum class EStatusChangeReason : uint8
{
	Applied UMETA(DisplayName = "Applied"),
	Increased UMETA(DisplayName = "Increased"),
	Reduced UMETA(DisplayName = "Reduced"),
	TurnEndDecay UMETA(DisplayName = "Turn End Decay"),
	Removed UMETA(DisplayName = "Removed")
};

enum class EStatusMutationOutcome : uint8
{
	Invalid,
	NoOp,
	Committed
};

struct SLAYTHESPIREDEMO_API FStatusMutationResult
{
	EStatusMutationOutcome Outcome = EStatusMutationOutcome::Invalid;

	FName StatusId = NAME_None;
	uint64 RuntimeSequence = 0;

	int32 AmountBefore = 0;
	int32 AmountAfter = 0;

	bool bCreated = false;
	bool bRemoved = false;

	UStatusInstance* EffectiveInstance = nullptr;
	UStatusData* EffectiveDefinition = nullptr;

	bool IsCommitted() const
	{
		return Outcome == EStatusMutationOutcome::Committed;
	}

	bool IsNoOp() const
	{
		return Outcome == EStatusMutationOutcome::NoOp;
	}
};
