#include "DamageAction.h"

#include "../Combat/Combatant.h"

void UDamageAction::Initialize(ACombatant* InSource, ACombatant* InTarget, int32 InBaseAmount)
{
	Source = InSource;
	Target = InTarget;
	BaseAmount = InBaseAmount;
}

void UDamageAction::Execute()
{
	if (!IsValid(Target.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DamageAction skipped: invalid target."));
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

	// Phase 2 intentionally commits BaseAmount directly. Phase 5 will build an
	// FDamageSpec and resolve it through the Modifier Pipeline before commit.
	Target->TakeCombatDamage(BaseAmount);
	Finish();
}
