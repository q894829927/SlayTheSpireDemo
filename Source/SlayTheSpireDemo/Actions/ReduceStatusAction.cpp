#include "ReduceStatusAction.h"

#include "../Battle/BattleManager.h"
#include "../Combat/Combatant.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusInstance.h"

void UReduceStatusAction::Initialize(
	ABattleManager* InBattle,
	ACombatant* InSource,
	ACombatant* InTarget,
	UStatusInstance* InExpectedInstance,
	int32 InAmountToRemove,
	EStatusChangeReason InReason
)
{
	Battle = InBattle;
	Source = InSource;
	Target = InTarget;
	ExpectedInstance = InExpectedInstance;
	AmountToRemove = InAmountToRemove;
	Reason = InReason;
}

void UReduceStatusAction::Initialize(
	UStatusContainer* InContainer,
	UStatusInstance* InExpectedInstance,
	int32 InAmountToRemove
)
{
	Battle = nullptr;
	Source = IsValid(InExpectedInstance) ? InExpectedInstance->GetOwner() : nullptr;
	Target = Source;
	ExpectedInstance = InExpectedInstance;
	AmountToRemove = InAmountToRemove;
	Reason = EStatusChangeReason::Reduced;

	if (IsValid(InContainer) && IsValid(Target.Get()) && Target->GetStatusContainer() != InContainer)
	{
		Target = nullptr;
	}
}

void UReduceStatusAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (Reason != EStatusChangeReason::Reduced && Reason != EStatusChangeReason::TurnEndDecay)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ReduceStatusAction skipped: invalid Reason=%d."), static_cast<int32>(Reason));
		Finish();
		return;
	}

	if (!IsValid(Target.Get()) || !IsValid(ExpectedInstance.Get()) || AmountToRemove <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ReduceStatusAction skipped: invalid Target, ExpectedInstance or AmountToRemove=%d."), AmountToRemove);
		Finish();
		return;
	}

	if (ExpectedInstance->GetOwner() != Target.Get())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ReduceStatusAction skipped: exact runtime instance does not belong to Target."));
		Finish();
		return;
	}

	UStatusContainer* Container = Target->GetStatusContainer();
	if (!IsValid(Container))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ReduceStatusAction skipped: Target has no StatusContainer."));
		Finish();
		return;
	}

	const FString ExpectedLabel = ExpectedInstance->GetDebugLabel();
	const FStatusMutationResult Result = Container->ReduceStatusCommit(ExpectedInstance.Get(), AmountToRemove);

	switch (Result.Outcome)
	{
	case EStatusMutationOutcome::Committed:
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Action] ReduceStatusAction committed for %s Amount %d -> %d Reason=%d."),
			*ExpectedLabel,
			Result.AmountBefore,
			Result.AmountAfter,
			static_cast<int32>(Reason)
		);
		break;

	case EStatusMutationOutcome::NoOp:
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Action] ReduceStatusAction no-op: exact runtime instance %s is no longer reducible in the expected container."),
			*ExpectedLabel
		);
		break;

	case EStatusMutationOutcome::Invalid:
	default:
		UE_LOG(LogTemp, Warning, TEXT("[Action] ReduceStatusAction failed for exact runtime instance %s."), *ExpectedLabel);
		break;
	}

	Finish();
}
