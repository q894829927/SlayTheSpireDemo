#include "EnergyPresentationRecord.h"

#include "PresentationTypes.h"
#include "../Battle/EnergyMutation.h"

void EnergyPresentationRecord::AppendCommittedEnergyChanged(
	const FEnergyCommitResult& CommitResult,
	const FPresentationRecordWriter& Writer)
{
	if (!CommitResult.bSucceeded || !CommitResult.bCommitted || !Writer.IsAvailable())
	{
		return;
	}

	if (CommitResult.Delta != CommitResult.EnergyAfter - CommitResult.EnergyBefore)
	{
		Writer.InvalidateCurrentResolution();
		UE_LOG(LogTemp, Warning, TEXT("[Presentation] Energy commit violated its Delta invariant."));
		return;
	}

	FPresentationRecord Record;
	Record.Type = EBattlePresentationRecordType::EnergyChanged;
	Record.EnergyChanged.EnergyBefore = CommitResult.EnergyBefore;
	Record.EnergyChanged.EnergyAfter = CommitResult.EnergyAfter;
	Record.EnergyChanged.Delta = CommitResult.Delta;
	if (!Writer.Append(MoveTemp(Record)))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Presentation] EnergyChanged append failed; Gameplay energy remains authoritative."));
	}
}
