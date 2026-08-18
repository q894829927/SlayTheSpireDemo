#include "Phase6ATestTypes.h"

#include "../Actions/BattleActionQueue.h"
#include "../Combat/Combatant.h"
#include "../Events/BattleEvent.h"
#include "../Events/BattleEventDispatcher.h"

namespace
{
	bool IsOwnerTurnEnded(const FBattleEvent& Event, const FTriggerContext& Context)
	{
		const FTurnEndedEvent* TurnEnded = Event.TryGet<FTurnEndedEvent>();
		return TurnEnded != nullptr
			&& IsValid(TurnEnded->TurnOwner)
			&& TurnEnded->TurnOwner == Context.GetOwner();
	}
}

void UPhase6ATestExecutionRecorder::Record(int32 Value)
{
	Values.Add(Value);
}

const TArray<int32>& UPhase6ATestExecutionRecorder::GetValues() const
{
	return Values;
}

void UPhase6ATestRecordAction::Initialize(UPhase6ATestExecutionRecorder* InRecorder, int32 InValue)
{
	Recorder = InRecorder;
	Value = InValue;
}

void UPhase6ATestRecordAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (IsValid(Recorder.Get()))
	{
		Recorder->Record(Value);
	}
	Finish();
}

void UPhase6ATestRecordTrigger::Initialize(UPhase6ATestExecutionRecorder* InRecorder, int32 InValue)
{
	Recorder = InRecorder;
	Value = InValue;
}

bool UPhase6ATestRecordTrigger::CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const
{
	return IsValid(Recorder.Get()) && IsOwnerTurnEnded(Event, Context);
}

void UPhase6ATestRecordTrigger::BuildReactions(
	const FBattleEvent& /*Event*/,
	const FTriggerContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Recorder.Get()) || !IsValid(Context.GetActionOuter()))
	{
		return;
	}

	UPhase6ATestRecordAction* Action = NewObject<UPhase6ATestRecordAction>(Context.GetActionOuter());
	Action->Initialize(Recorder.Get(), Value);
	OutActions.Add(Action);
}

void UPhase6ATestEmitTurnEndedAction::Initialize(
	UPhase6ATestExecutionRecorder* InRecorder,
	int32 InCommitValue,
	UBattleEventDispatcher* InDispatcher,
	ACombatant* InNestedTurnOwner,
	const TArray<ACombatant*>& InCombatants
)
{
	Recorder = InRecorder;
	CommitValue = InCommitValue;
	Dispatcher = InDispatcher;
	NestedTurnOwner = InNestedTurnOwner;
	Combatants.Reset();
	for (ACombatant* Combatant : InCombatants)
	{
		Combatants.Add(Combatant);
	}
}

void UPhase6ATestEmitTurnEndedAction::Execute(UBattleActionQueue* Queue)
{
	if (!IsValid(Recorder.Get()) || !IsValid(Dispatcher.Get()) || !IsValid(NestedTurnOwner.Get()) || !IsValid(Queue))
	{
		Finish();
		return;
	}

	// This Record is the test Action's observable commit. The nested event is
	// deliberately dispatched after commit and before Finish(), matching the
	// production event-emission contract required by Phase 6.
	Recorder->Record(CommitValue);

	TArray<ACombatant*> RawCombatants;
	RawCombatants.Reserve(Combatants.Num());
	for (const TObjectPtr<ACombatant>& Combatant : Combatants)
	{
		RawCombatants.Add(Combatant.Get());
	}

	Dispatcher->Dispatch(
		FBattleEvent::MakeTurnEnded(NestedTurnOwner.Get()),
		Queue,
		RawCombatants
	);

	Finish();
}

void UPhase6ATestNestedTrigger::Initialize(
	UPhase6ATestExecutionRecorder* InRecorder,
	UBattleEventDispatcher* InDispatcher,
	ACombatant* InNestedTurnOwner,
	const TArray<ACombatant*>& InCombatants,
	int32 InEmitValue,
	int32 InSiblingValue
)
{
	Recorder = InRecorder;
	Dispatcher = InDispatcher;
	NestedTurnOwner = InNestedTurnOwner;
	EmitValue = InEmitValue;
	SiblingValue = InSiblingValue;
	Combatants.Reset();
	for (ACombatant* Combatant : InCombatants)
	{
		Combatants.Add(Combatant);
	}
}

bool UPhase6ATestNestedTrigger::CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const
{
	return IsValid(Recorder.Get())
		&& IsValid(Dispatcher.Get())
		&& IsValid(NestedTurnOwner.Get())
		&& IsOwnerTurnEnded(Event, Context);
}

void UPhase6ATestNestedTrigger::BuildReactions(
	const FBattleEvent& /*Event*/,
	const FTriggerContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.GetActionOuter()))
	{
		return;
	}

	TArray<ACombatant*> RawCombatants;
	RawCombatants.Reserve(Combatants.Num());
	for (const TObjectPtr<ACombatant>& Combatant : Combatants)
	{
		RawCombatants.Add(Combatant.Get());
	}

	UPhase6ATestEmitTurnEndedAction* EmitAction = NewObject<UPhase6ATestEmitTurnEndedAction>(Context.GetActionOuter());
	EmitAction->Initialize(
		Recorder.Get(),
		EmitValue,
		Dispatcher.Get(),
		NestedTurnOwner.Get(),
		RawCombatants
	);
	OutActions.Add(EmitAction);

	UPhase6ATestRecordAction* SiblingAction = NewObject<UPhase6ATestRecordAction>(Context.GetActionOuter());
	SiblingAction->Initialize(Recorder.Get(), SiblingValue);
	OutActions.Add(SiblingAction);
}
