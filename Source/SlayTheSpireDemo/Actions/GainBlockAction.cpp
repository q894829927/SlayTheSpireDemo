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
	if (!IsValid(Target.Get()) || Target->IsDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] GainBlockAction skipped: target is invalid or dead."));
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

	// BaseAmount is stable intent captured when the action is built. Phase 5
	// will resolve the final value here at execution time through FBlockSpec
	// and the block Modifier Pipeline before committing to the target.
	Target->GainBlock(BaseAmount);
	Finish();
}
