#include "RemoveStatusAction.h"

#include "../Battle/BattleManager.h"
#include "../Combat/Combatant.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusInstance.h"

void URemoveStatusAction::Initialize(
	ABattleManager* InBattle,
	ACombatant* InSource,
	ACombatant* InTarget,
	UStatusInstance* InExpectedInstance
)
{
	Battle = InBattle;
	Source = InSource;
	Target = InTarget;
	ExpectedInstance = InExpectedInstance;
}

void URemoveStatusAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Target.Get()) || !IsValid(ExpectedInstance.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] RemoveStatusAction skipped: invalid Target or ExpectedInstance."));
		Finish();
		return;
	}

	if (ExpectedInstance->GetOwner() != Target.Get())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] RemoveStatusAction skipped: exact runtime instance does not belong to Target."));
		Finish();
		return;
	}

	UStatusContainer* Container = Target->GetStatusContainer();
	if (!IsValid(Container))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] RemoveStatusAction skipped: Target has no StatusContainer."));
		Finish();
		return;
	}

	const FString ExpectedLabel = ExpectedInstance->GetDebugLabel();
	const FStatusMutationResult Result = Container->RemoveStatusCommit(ExpectedInstance.Get());

	switch (Result.Outcome)
	{
	case EStatusMutationOutcome::Committed:
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Action] RemoveStatusAction committed for %s Amount %d -> 0."),
			*ExpectedLabel,
			Result.AmountBefore
		);
		break;

	case EStatusMutationOutcome::NoOp:
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Action] RemoveStatusAction no-op: exact runtime instance %s is no longer present."),
			*ExpectedLabel
		);
		break;

	case EStatusMutationOutcome::Invalid:
	default:
		UE_LOG(LogTemp, Warning, TEXT("[Action] RemoveStatusAction failed for exact runtime instance %s."), *ExpectedLabel);
		break;
	}

	Finish();
}
