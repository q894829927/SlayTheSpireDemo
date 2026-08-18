#include "TurnEndStatusDecayTrigger.h"

#include "BattleEvent.h"
#include "../Actions/ReduceStatusAction.h"
#include "../Combat/Combatant.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusInstance.h"

bool UTurnEndStatusDecayTrigger::CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const
{
	const FTurnEndedEvent* TurnEnded = Event.TryGet<FTurnEndedEvent>();
	return TurnEnded != nullptr
		&& IsValid(TurnEnded->TurnOwner)
		&& TurnEnded->TurnOwner == Context.GetOwner()
		&& IsValid(Context.GetRuntimeSource())
		&& Context.GetRuntimeSource()->GetAmount() > 0
		&& AmountToRemove > 0;
}

void UTurnEndStatusDecayTrigger::BuildReactions(
	const FBattleEvent& Event,
	const FTriggerContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!CanReact(Event, Context))
	{
		return;
	}

	ACombatant* Owner = Context.GetOwner();
	UStatusInstance* RuntimeSource = Context.GetRuntimeSource();
	UObject* ActionOuter = Context.GetActionOuter();
	if (!IsValid(Owner) || !IsValid(RuntimeSource) || !IsValid(ActionOuter))
	{
		return;
	}

	UStatusContainer* Container = Owner->GetStatusContainer();
	if (!IsValid(Container))
	{
		return;
	}

	UReduceStatusAction* Action = NewObject<UReduceStatusAction>(ActionOuter);
	if (!IsValid(Action))
	{
		return;
	}

	Action->Initialize(Container, RuntimeSource, AmountToRemove);
	OutActions.Add(Action);
}
