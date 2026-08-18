#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleEventDispatcher.generated.h"

class ACombatant;
class UBattleActionQueue;
struct FBattleEvent;

struct FTriggerEligibilityRecord
{
	FName StatusId = NAME_None;
	int32 Priority = 0;
	uint64 RuntimeSequence = 0;
	int32 LocalTriggerIndex = INDEX_NONE;
};

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
