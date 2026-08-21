#include "DamageAction.h"

#include "../Combat/Combatant.h"
#include "../Modifiers/Damage/DamageModifierPipeline.h"
#include "../Modifiers/Damage/DamageSpec.h"

void UDamageAction::Initialize(
	ACombatant* InSource,
	ACombatant* InTarget,
	int32 InBaseAmount,
	EDamageKind InDamageKind
)
{
	Source = InSource;
	Target = InTarget;
	BaseAmount = InBaseAmount;
	DamageKind = InDamageKind;
	SourcePresentationId = NAME_None;
	TargetPresentationId = NAME_None;
}

void UDamageAction::SetPresentationParticipantIds(
	FName InSourcePresentationId,
	FName InTargetPresentationId
)
{
	SourcePresentationId = InSourcePresentationId;
	TargetPresentationId = InTargetPresentationId;
}

void UDamageAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Target.Get()) || Target->IsDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DamageAction skipped: target is invalid or dead."));
		Finish();
		return;
	}

	if (BaseAmount < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DamageAction skipped: invalid negative BaseAmount=%d"), BaseAmount);
		Finish();
		return;
	}

	FDamageSpec Spec;
	Spec.Source = Source.Get();
	Spec.Target = Target.Get();
	Spec.DamageKind = DamageKind;
	Spec.BaseAmount = BaseAmount;

	FDamageModifierPipeline::Resolve(Spec);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Action] DamageAction resolved: Source=%s Target=%s Kind=%s Base=%d Resolved=%d"),
		*GetNameSafe(Source.Get()),
		*GetNameSafe(Target.Get()),
		DamageKindToString(DamageKind),
		BaseAmount,
		Spec.ResolvedAmount
	);

	if (Spec.ResolvedAmount <= 0)
	{
		Finish();
		return;
	}

	const FDamageCommitResult CommitResult = Target->TakeCombatDamage(Spec.ResolvedAmount);
	if (!CommitResult.bCommitted)
	{
		Finish();
		return;
	}

	const FPresentationRecordWriter& Writer = GetPresentationRecordWriter();
	if (Writer.IsAvailable())
	{
		const bool bSourceContextValid = Source.Get() == nullptr
			|| (IsValid(Source.Get()) && !SourcePresentationId.IsNone());
		const bool bTargetContextValid = IsValid(Target.Get()) && !TargetPresentationId.IsNone();
		if (!bSourceContextValid || !bTargetContextValid)
		{
			Writer.InvalidateCurrentResolution();
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[Presentation] Damage commit could not build a trustworthy participant identity payload."));
		}
		else
		{
			FPresentationRecord Record;
			Record.Type = EBattlePresentationRecordType::Damage;
			Record.Damage.SourcePresentationId = SourcePresentationId;
			Record.Damage.TargetPresentationId = TargetPresentationId;
			Record.Damage.DamageKind = DamageKind;
			Record.Damage.IncomingDamage = CommitResult.IncomingDamage;
			Record.Damage.HPBefore = CommitResult.HPBefore;
			Record.Damage.HPAfter = CommitResult.HPAfter;
			Record.Damage.BlockBefore = CommitResult.BlockBefore;
			Record.Damage.BlockAfter = CommitResult.BlockAfter;
			Record.Damage.BlockedDamage = CommitResult.BlockedDamage;
			Record.Damage.HPDamage = CommitResult.HPDamage;
			if (!Writer.Append(MoveTemp(Record)))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[Presentation] Damage record append failed; Gameplay commit remains authoritative."));
			}
		}
	}

	Finish();
}
