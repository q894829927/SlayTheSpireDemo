#pragma once

#include "CoreMinimal.h"
#include "EnergyMutationTypes.h"

class ABattleManager;

namespace BattleEnergyMutation
{
	SLAYTHESPIREDEMO_API FEnergyCommitResult TrySpend(ABattleManager* Battle, int32 Amount);
	SLAYTHESPIREDEMO_API FEnergyCommitResult SetValue(ABattleManager* Battle, int32 NewValue);
}
