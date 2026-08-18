#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleEventDispatcher.generated.h"

class ACombatant;
class UBattleActionQueue;
struct FBattleEvent;

// Records the deterministic order of triggers that passed CanReact. This is an
// eligibility/candidate trace only; it does not claim that reaction Actions were
// successfully built, inserted or executed in this same order.
struct FTriggerEligibilityRecord
{
	FName StatusId = NAME_None;
	int32 Priority = 0;
	uint64 RuntimeSequence = 0;
	int32 LocalTriggerIndex = INDEX_NONE;
};

// Temporary source-compatibility alias for the original Phase 6A tests. New
// code should use FTriggerEligibilityRecord so the trace semantics stay clear.
using FTriggerDispatchRecord = FTriggerEligibilityRecord;

UCLASS()
class SLAYTHESPIREDEMO_API UBattleEventDispatcher : public UObject
{
	GENERATED_BODY()

public:
	bool Dispatch(
		const FBattleEvent& Event,
		UBattleActionQueue* Queue,
		const TArray<ACombatant*>& Combatants,
		TArray<FTriggerEligibilityRecord>* OutEligibilityTrace = nullptr
	) const;
};
