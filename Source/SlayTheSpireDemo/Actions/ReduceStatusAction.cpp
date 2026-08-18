#include "ReduceStatusAction.h"

#include "../Status/StatusContainer.h"
#include "../Status/StatusInstance.h"

void UReduceStatusAction::Initialize(
	UStatusContainer* InContainer,
	UStatusInstance* InExpectedInstance,
	int32 InAmountToRemove
)
{
	Container = InContainer;
	ExpectedInstance = InExpectedInstance;
	AmountToRemove = InAmountToRemove;
}

void UReduceStatusAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Container.Get()) || !IsValid(ExpectedInstance.Get()) || AmountToRemove <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ReduceStatusAction skipped: invalid Container, ExpectedInstance or AmountToRemove=%d."), AmountToRemove);
		Finish();
		return;
	}

	const FString ExpectedLabel = ExpectedInstance->GetDebugLabel();
	if (!Container->ReduceStatus(ExpectedInstance.Get(), AmountToRemove))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] ReduceStatusAction no-op: exact runtime instance %s is no longer reducible in the expected container."),
			*ExpectedLabel
		);
		Finish();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Action] ReduceStatusAction committed for %s AmountToRemove=%d."), *ExpectedLabel, AmountToRemove);
	Finish();
}
