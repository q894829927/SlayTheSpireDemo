#include "Phase7BTriggerSourceTestTypes.h"

#include "Events/BattleEvent.h"
#include "Relics/RelicInstance.h"
#include "Status/StatusInstance.h"

void UPhase7BTestExecutionRecorder::Record(int32 Value)
{
	Values.Add(Value);
}

const TArray<int32>& UPhase7BTestExecutionRecorder::GetValues() const
{
	return Values;
}

void UPhase7BTestRecordAction::Initialize(UPhase7BTestExecutionRecorder* InRecorder, int32 InValue)
{
	Recorder = InRecorder;
	Value = InValue;
}

void UPhase7BTestRecordAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (IsValid(Recorder.Get()))
	{
		Recorder->Record(Value);
	}
	Finish();
}

void UPhase7BTestSourceTrigger::Initialize(
	UPhase7BTestExecutionRecorder* InRecorder,
	int32 InValue,
	ETriggerRuntimeSourceKind InExpectedSourceKind,
	FName InExpectedSourceId
)
{
	Recorder = InRecorder;
	Value = InValue;
	ExpectedSourceKind = InExpectedSourceKind;
	ExpectedSourceId = InExpectedSourceId;
}

bool UPhase7BTestSourceTrigger::MatchesExpectedSource(const FTriggerContext& Context) const
{
	if (!IsValid(Context.GetRuntimeSourceObject())
		|| Context.GetSourceKind() != ExpectedSourceKind
		|| Context.GetSourceId() != ExpectedSourceId
		|| Context.GetRuntimeSequence() == 0)
	{
		return false;
	}

	if (ExpectedSourceKind == ETriggerRuntimeSourceKind::Status)
	{
		return IsValid(Context.GetRuntimeSource())
			&& Context.GetRelicSource() == nullptr
			&& IsValid(Context.GetOwner());
	}

	URelicInstance* Relic = Context.GetRelicSource();
	return IsValid(Relic)
		&& Context.GetRuntimeSource() == nullptr
		&& Context.GetOwner() == nullptr
		&& IsValid(Context.GetBattle())
		&& Context.GetBattle() == Relic->GetBattle();
}

bool UPhase7BTestSourceTrigger::CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const
{
	return IsValid(Recorder.Get())
		&& Event.TryGet<FTurnEndedEvent>() != nullptr
		&& MatchesExpectedSource(Context);
}

void UPhase7BTestSourceTrigger::BuildReactions(
	const FBattleEvent& /*Event*/,
	const FTriggerContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Recorder.Get())
		|| !IsValid(Context.GetActionOuter())
		|| !MatchesExpectedSource(Context))
	{
		return;
	}

	UPhase7BTestRecordAction* Action = NewObject<UPhase7BTestRecordAction>(Context.GetActionOuter());
	Action->Initialize(Recorder.Get(), Value);
	OutActions.Add(Action);
}
