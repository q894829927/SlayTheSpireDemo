#include "GainEnergyAction.h"

#include "../Battle/BattleManager.h"
#include "../Battle/EnergyMutation.h"
#include "../Presentation/PresentationTypes.h"

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

	const FPresentationRecordWriter& Writer = GetPresentationRecordWriter();
	if (Writer.IsAvailable())
	{
		if (CommitResult.Delta != CommitResult.EnergyAfter - CommitResult.EnergyBefore)
		{
			Writer.InvalidateCurrentResolution();
			UE_LOG(LogTemp, Warning, TEXT("[Presentation] GainEnergyAction commit violated its Delta invariant."));
		}
		else
		{
			FPresentationRecord Record;
			Record.Type = EBattlePresentationRecordType::EnergyChanged;
			Record.EnergyChanged.EnergyBefore = CommitResult.EnergyBefore;
			Record.EnergyChanged.EnergyAfter = CommitResult.EnergyAfter;
			Record.EnergyChanged.Delta = CommitResult.Delta;
			if (!Writer.Append(MoveTemp(Record)))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Presentation] GainEnergyAction EnergyChanged append failed; Gameplay remains authoritative."));
			}
		}
	}

	Finish();
}
