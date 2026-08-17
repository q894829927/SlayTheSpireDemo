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

	if (Spec.ResolvedAmount > 0)
	{
		Target->TakeCombatDamage(Spec.ResolvedAmount);
	}

	Finish();
}
