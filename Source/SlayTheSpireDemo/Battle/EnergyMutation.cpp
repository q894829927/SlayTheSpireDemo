#include "EnergyMutation.h"

#include "BattleManager.h"

FEnergyCommitResult BattleEnergyMutation::TrySpend(ABattleManager* Battle, int32 Amount)
{
	FEnergyCommitResult Result;
	if (!IsValid(Battle) || !Battle->CanSpendEnergy(Amount))
	{
		return Result;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (Battle->ConsumeForceNextEnergySpendFailureForTesting())
	{
		return Result;
	}
#endif

	Result.bSucceeded = true;
	Result.EnergyBefore = Battle->Energy;
	Battle->Energy -= Amount;
	Result.EnergyAfter = Battle->Energy;
	Result.Delta = Result.EnergyAfter - Result.EnergyBefore;
	Result.bCommitted = Result.Delta != 0;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Energy spent: Amount=%d Energy=%d/%d"),
		Amount,
		Battle->Energy,
		Battle->MaxEnergy
	);
	return Result;
}

FEnergyCommitResult BattleEnergyMutation::TryGain(ABattleManager* Battle, int32 Amount)
{
	FEnergyCommitResult Result;
	if (!IsValid(Battle) || Amount <= 0)
	{
		return Result;
	}

	const int64 Candidate = static_cast<int64>(Battle->Energy) + static_cast<int64>(Amount);
	if (Candidate > static_cast<int64>(MAX_int32))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] Energy gain rejected: int32 overflow. Energy=%d Amount=%d"), Battle->Energy, Amount);
		return Result;
	}

	Result.bSucceeded = true;
	Result.bCommitted = true;
	Result.EnergyBefore = Battle->Energy;
	Battle->Energy = static_cast<int32>(Candidate);
	Result.EnergyAfter = Battle->Energy;
	Result.Delta = Amount;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Energy gained: Amount=%d Energy=%d/%d"),
		Amount,
		Battle->Energy,
		Battle->MaxEnergy
	);
	return Result;
}

FEnergyCommitResult BattleEnergyMutation::SetValue(ABattleManager* Battle, int32 NewValue)
{
	FEnergyCommitResult Result;
	if (!IsValid(Battle) || NewValue < 0)
	{
		return Result;
	}

	Result.bSucceeded = true;
	Result.EnergyBefore = Battle->Energy;
	Battle->Energy = NewValue;
	Result.EnergyAfter = Battle->Energy;
	Result.Delta = Result.EnergyAfter - Result.EnergyBefore;
	Result.bCommitted = Result.Delta != 0;
	return Result;
}
