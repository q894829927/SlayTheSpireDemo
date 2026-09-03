#include "GainEnergyAction.h"

#include "../Battle/BattleManager.h"
#include "../Battle/EnergyMutation.h"
#include "../Presentation/EnergyPresentationRecord.h"

void UGainEnergyAction::Initialize(ABattleManager* InBattle, int32 InAmount)
{
	Battle = InBattle;
	Amount = InAmount;
}

void UGainEnergyAction::Execute(UBattleActionQueue* /*Queue*/)
{
	const FEnergyCommitResult CommitResult = BattleEnergyMutation::TryGain(Battle.Get(), Amount);
	if (!CommitResult.bSucceeded || !CommitResult.bCommitted)
	{
		Finish();
		return;
	}

	EnergyPresentationRecord::AppendCommittedEnergyChanged(
		CommitResult,
		GetPresentationRecordWriter());
	Finish();
}
