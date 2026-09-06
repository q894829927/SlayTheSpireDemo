#include "CardExpansionWave1CTestTypes.h"

#include "Actions/BattleActionQueue.h"

void UWave1CTestMarkerAction::Initialize(int32* InExecutionCounter, FName InTag)
{
	ExecutionCounter = InExecutionCounter;
	Tag = InTag;
}

void UWave1CTestMarkerAction::Execute(UBattleActionQueue* Queue)
{
	(void)Queue;
	if (ExecutionCounter != nullptr)
	{
		++(*ExecutionCounter);
	}
	Finish();
}

void UWave1CTestContinuation::Configure(int32* InExecutionCounter, FName InTag, int32 InMarkersPerResult)
{
	ExecutionCounter = InExecutionCounter;
	Tag = InTag;
	MarkersPerResult = InMarkersPerResult;
}

bool UWave1CTestContinuation::BuildNextActions(
	const FSelectionResult& Result,
	UBattleActionQueue* Queue,
	TArray<UBattleAction*>& OutActions
) const
{
	OutActions.Reset();
	if (Result.Status != ESelectionStatus::Resolved || Result.SelectedObjects.Num() == 0)
	{
		return false;
	}
	for (int32 Index = 0; Index < MarkersPerResult; ++Index)
	{
		UWave1CTestMarkerAction* Marker = NewObject<UWave1CTestMarkerAction>(Queue);
		Marker->Initialize(ExecutionCounter, Tag);
		OutActions.Add(Marker);
	}
	return true;
}
