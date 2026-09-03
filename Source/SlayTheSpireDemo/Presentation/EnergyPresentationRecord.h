#pragma once

struct FEnergyCommitResult;
struct FPresentationRecordWriter;

// Narrow shared projection for committed Energy mutation facts only.
// Gameplay mutation and validation remain owned by BattleEnergyMutation.
namespace EnergyPresentationRecord
{
	SLAYTHESPIREDEMO_API void AppendCommittedEnergyChanged(
		const FEnergyCommitResult& CommitResult,
		const FPresentationRecordWriter& Writer);
}
