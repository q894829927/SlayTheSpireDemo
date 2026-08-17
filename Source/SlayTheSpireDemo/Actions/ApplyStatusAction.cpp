#include "ApplyStatusAction.h"

#include "../Battle/BattleManager.h"
#include "../Combat/Combatant.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusData.h"
#include "../Status/StatusInstance.h"

void UApplyStatusAction::Initialize(
	ABattleManager* InBattle,
	ACombatant* InSource,
	ACombatant* InTarget,
	UStatusData* InStatusDefinition,
	int32 InAmountToAdd
)
{
	Battle = InBattle;
	Source = InSource;
	Target = InTarget;
	StatusDefinition = InStatusDefinition;
	AmountToAdd = InAmountToAdd;
}

void UApplyStatusAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Battle.Get()) || !IsValid(Target.Get()) || !IsValid(StatusDefinition.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ApplyStatusAction skipped: invalid Battle, Target or StatusDefinition."));
		Finish();
		return;
	}

	if (Target->IsDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ApplyStatusAction skipped: target %s is dead."), *GetNameSafe(Target.Get()));
		Finish();
		return;
	}

	if (AmountToAdd <= 0 || StatusDefinition->StatusId.IsNone())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Action] ApplyStatusAction skipped: StatusId=%s AmountToAdd=%d."),
			*StatusDefinition->StatusId.ToString(),
			AmountToAdd
		);
		Finish();
		return;
	}

	UStatusContainer* Container = Target->GetStatusContainer();
	if (!IsValid(Container))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ApplyStatusAction skipped: target %s has no StatusContainer."), *GetNameSafe(Target.Get()));
		Finish();
		return;
	}

	const uint64 CandidateSequence = Battle->AllocateRuntimeSequence();
	if (CandidateSequence == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[Action] ApplyStatusAction skipped: failed to allocate RuntimeSequence."));
		Finish();
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Action] ApplyStatusAction: Source=%s Target=%s Status=%s AmountToAdd=%d CandidateSequence=%llu"),
		*GetNameSafe(Source.Get()),
		*GetNameSafe(Target.Get()),
		*StatusDefinition->StatusId.ToString(),
		AmountToAdd,
		static_cast<unsigned long long>(CandidateSequence)
	);

	bool bCreated = false;
	UStatusInstance* AppliedInstance = Container->ApplyStatus(
		StatusDefinition.Get(),
		AmountToAdd,
		CandidateSequence,
		bCreated
	);

	if (!IsValid(AppliedInstance))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] ApplyStatusAction failed to apply status."));
		Finish();
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Action] ApplyStatusAction committed: %s Amount=%d Created=%s"),
		*AppliedInstance->GetDebugLabel(),
		AppliedInstance->GetAmount(),
		bCreated ? TEXT("true") : TEXT("false")
	);

	Finish();
}
