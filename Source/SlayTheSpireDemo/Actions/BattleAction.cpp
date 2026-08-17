#include "BattleAction.h"

void UBattleAction::Execute(UBattleActionQueue* /*Queue*/)
{
	UE_LOG(LogTemp, Error, TEXT("[Action] Base BattleAction executed directly: %s"), *GetNameSafe(this));
	Finish();
}

void UBattleAction::Finish()
{
	if (bIsFinished)
	{
		return;
	}

	bIsFinished = true;
	OnFinished.Broadcast(this);
}
