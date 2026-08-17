#include "GainBlockAction.h"

#include "../Combat/Combatant.h"
#include "../Modifiers/Block/BlockModifierPipeline.h"
#include "../Modifiers/Block/BlockSpec.h"

void UGainBlockAction::Initialize(ACombatant* InSource, ACombatant* InTarget, int32 InBaseAmount)
{
	Source = InSource;
	Target = InTarget;
	BaseAmount = InBaseAmount;
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

	if (Spec.ResolvedAmount > 0)
	{
		Target->GainBlock(Spec.ResolvedAmount);
	}

	Finish();
}
