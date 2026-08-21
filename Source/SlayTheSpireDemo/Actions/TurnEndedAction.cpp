#include "TurnEndedAction.h"

#include "BattleActionQueue.h"
#include "../Battle/BattleManager.h"
#include "../Combat/Combatant.h"

void UTurnEndedAction::Initialize(ABattleManager* InBattle, ACombatant* InTurnOwner)
{
	Battle = InBattle;
	TurnOwner = InTurnOwner;
}

void UTurnEndedAction::Execute(UBattleActionQueue* Queue)
{
	if (!IsValid(Battle.Get()) || !IsValid(TurnOwner.Get()) || !IsValid(Queue))
	{
		UE_LOG(LogTemp, Error, TEXT("[Action] TurnEndedAction failed: invalid Battle, TurnOwner or Queue."));
		if (IsValid(Queue))
		{
			Queue->RequestResolutionFault(TEXT("TurnEndedAction executed with an invalid runtime dependency."));
		}
		Finish();
		return;
	}

	Battle->HandleTurnEndedActionExecution(
		TurnOwner.Get(),
		Queue,
		GetPresentationRecordWriter()
	);
	Finish();
}
