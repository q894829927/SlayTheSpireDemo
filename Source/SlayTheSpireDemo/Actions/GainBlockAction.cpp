#include "GainBlockAction.h"

#include "../Combat/Combatant.h"

void UGainBlockAction::Initialize(ACombatant* InSource, ACombatant* InTarget, int32 InBaseAmount)
{
	Source = InSource;
	Target = InTarget;
	BaseAmount = InBaseAmount;
}

void UGainBlockAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Target.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] GainBlockAction skipped: invalid target."));
		Finish();
		return;
	}

	if (BaseAmount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] GainBlockAction skipped: BaseAmount=%d"), BaseAmount);
		Finish();
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Action] GainBlockAction: Source=%s Target=%s BaseAmount=%d"),
		*GetNameSafe(Source.Get()),
		*GetNameSafe(Target.Get()),
		BaseAmount
	);

	// Phase 3 still commits BaseAmount directly. Phase 5 will build an
	// FBlockSpec and resolve it through the Modifier Pipeline before commit.
	Target->GainBlock(BaseAmount);
	Finish();
}
