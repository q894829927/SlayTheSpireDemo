#include "DamageAction.h"

#include "../Combat/Combatant.h"

void UDamageAction::Initialize(ACombatant* InSource, ACombatant* InTarget, int32 InBaseAmount)
{
	Source = InSource;
	Target = InTarget;
	BaseAmount = InBaseAmount;
}

void UDamageAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Target.Get()) || Target->IsDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DamageAction skipped: target is invalid or dead."));
		Finish();
		return;
	}

	if (BaseAmount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DamageAction skipped: BaseAmount=%d"), BaseAmount);
		Finish();
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Action] DamageAction: Source=%s Target=%s BaseAmount=%d"),
		*GetNameSafe(Source.Get()),
		*GetNameSafe(Target.Get()),
		BaseAmount
	);

	// BaseAmount is stable intent captured when the action is built. Phase 5
	// will resolve the final value here at execution time through FDamageSpec
	// and the damage Modifier Pipeline before committing to the target.
	Target->TakeCombatDamage(BaseAmount);
	Finish();
}
