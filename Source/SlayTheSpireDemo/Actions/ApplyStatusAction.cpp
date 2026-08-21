#include "ApplyStatusAction.h"

#include "../Battle/BattleManager.h"
#include "../Combat/Combatant.h"
#include "../Presentation/StatusPresentationRecordBuilder.h"
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

	const FPresentationRecordWriter& Writer = GetPresentationRecordWriter();
	const UStatusInstance* ExistingBefore = Writer.IsAvailable()
		? Container->FindMutableStatusById(StatusDefinition->StatusId)
		: nullptr;
	const FText DescriptionBefore = StatusPresentation::FreezeDescription(ExistingBefore);

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

	const FStatusMutationResult Result = Container->ApplyStatusCommit(
		StatusDefinition.Get(),
		AmountToAdd,
		CandidateSequence
	);

	switch (Result.Outcome)
	{
	case EStatusMutationOutcome::Committed:
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Action] ApplyStatusAction committed: %s Amount=%d Created=%s"),
			IsValid(Result.EffectiveInstance) ? *Result.EffectiveInstance->GetDebugLabel() : TEXT("InvalidStatus"),
			Result.AmountAfter,
			Result.bCreated ? TEXT("true") : TEXT("false")
		);

		if (Writer.IsAvailable())
		{
			const FText DescriptionAfter = StatusPresentation::FreezeDescription(Result.EffectiveInstance);
			const EStatusChangeReason Reason = Result.bCreated
				? EStatusChangeReason::Applied
				: EStatusChangeReason::Increased;
			StatusPresentation::AppendCommittedChange(
				Writer,
				Battle.Get(),
				Source.Get(),
				Target.Get(),
				Result,
				Reason,
				DescriptionBefore,
				DescriptionAfter
			);
		}
		break;
	}

	case EStatusMutationOutcome::NoOp:
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Action] ApplyStatusAction no-op: Status=%s RuntimeSequence=%llu Amount=%d."),
			*Result.StatusId.ToString(),
			static_cast<unsigned long long>(Result.RuntimeSequence),
			Result.AmountAfter
		);
		break;

	case EStatusMutationOutcome::Invalid:
	default:
		UE_LOG(LogTemp, Warning, TEXT("[Action] ApplyStatusAction failed to apply status."));
		break;
	}

	Finish();
}
