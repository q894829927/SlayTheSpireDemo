#include "GainBlockAction.h"

#include "../Combat/Combatant.h"
#include "../Modifiers/Block/BlockModifierPipeline.h"
#include "../Modifiers/Block/BlockSpec.h"

void UGainBlockAction::Initialize(ACombatant* InSource, ACombatant* InTarget, int32 InBaseAmount)
{
	Source = InSource;
	Target = InTarget;
	BaseAmount = InBaseAmount;
	SourcePresentationId = NAME_None;
	TargetPresentationId = NAME_None;
}

void UGainBlockAction::SetPresentationParticipantIds(
	FName InSourcePresentationId,
	FName InTargetPresentationId
)
{
	SourcePresentationId = InSourcePresentationId;
	TargetPresentationId = InTargetPresentationId;
}

void UGainBlockAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Target.Get()) || Target->IsDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] GainBlockAction skipped: target is invalid or dead."));
		Finish();
		return;
	}

	if (BaseAmount < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] GainBlockAction skipped: invalid negative BaseAmount=%d"), BaseAmount);
		Finish();
		return;
	}

	FBlockSpec Spec;
	Spec.Source = Source.Get();
	Spec.Target = Target.Get();
	Spec.BaseAmount = BaseAmount;

	FBlockModifierPipeline::Resolve(Spec);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Action] GainBlockAction resolved: Source=%s Target=%s Base=%d Resolved=%d"),
		*GetNameSafe(Source.Get()),
		*GetNameSafe(Target.Get()),
		BaseAmount,
		Spec.ResolvedAmount
	);

	if (Spec.ResolvedAmount <= 0)
	{
		Finish();
		return;
	}

	const FBlockCommitResult CommitResult = Target->GainBlock(Spec.ResolvedAmount);
	if (!CommitResult.bCommitted)
	{
		Finish();
		return;
	}

	const FPresentationRecordWriter& Writer = GetPresentationRecordWriter();
	if (Writer.IsAvailable())
	{
		const bool bSourceContextValid = Source.Get() == nullptr
			? SourcePresentationId.IsNone()
			: IsValid(Source.Get()) && !SourcePresentationId.IsNone();
		const bool bTargetContextValid = IsValid(Target.Get()) && !TargetPresentationId.IsNone();
		if (!bSourceContextValid || !bTargetContextValid)
		{
			Writer.InvalidateCurrentResolution();
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[Presentation] Block commit could not build a trustworthy participant identity payload."));
		}
		else
		{
			FPresentationRecord Record;
			Record.Type = EBattlePresentationRecordType::BlockChanged;
			Record.BlockChanged.SourcePresentationId = SourcePresentationId;
			Record.BlockChanged.TargetPresentationId = TargetPresentationId;
			Record.BlockChanged.Reason = EBlockPresentationReason::Gain;
			Record.BlockChanged.BlockBefore = CommitResult.BlockBefore;
			Record.BlockChanged.BlockAfter = CommitResult.BlockAfter;
			Record.BlockChanged.BlockDelta = CommitResult.BlockDelta;
			if (!Writer.Append(MoveTemp(Record)))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[Presentation] Block record append failed; Gameplay commit remains authoritative."));
			}
		}
	}

	Finish();
}
