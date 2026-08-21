#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleEventDispatcher.generated.h"

class ACombatant;
class UBattleActionQueue;
struct FBattleEvent;
struct FPresentationRecordWriter;

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

#if WITH_DEV_AUTOMATION_TESTS
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBattleEventDispatchedForTesting, const FBattleEvent&);
#endif

UCLASS()
class SLAYTHESPIREDEMO_API UBattleEventDispatcher : public UObject
{
	GENERATED_BODY()

public:
	bool Dispatch(
		const FBattleEvent& Event,
		UBattleActionQueue* Queue,
		const TArray<ACombatant*>& Combatants,
		TArray<FTriggerEligibilityRecord>* OutEligibilityTrace = nullptr,
		const FPresentationRecordWriter* PresentationRecordWriter = nullptr
	) const;

#if WITH_DEV_AUTOMATION_TESTS
	// Test-only observation hook for proving whether a real Dispatcher path emitted
	// a gameplay event. It does not participate in reaction ordering or mutation.
	static FOnBattleEventDispatchedForTesting OnEventDispatchedForTesting;
#endif
};
