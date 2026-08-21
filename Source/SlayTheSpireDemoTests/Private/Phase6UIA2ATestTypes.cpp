#include "Phase6UIA2ATestTypes.h"

#include "Actions/BattleActionQueue.h"
#include "Events/BattleEvent.h"
#include "Presentation/PresentationTypes.h"

void UPhase6UIA2AProbeState::RecordContextWriter(const FPresentationRecordWriter& Writer)
{
	bContextWriterAvailable = Writer.IsAvailable();
	ContextBattleId = Writer.GetBattleId();
	ContextResolutionId = Writer.GetResolutionId();
}

void UPhase6UIA2AProbeState::RecordActionWriter(const FPresentationRecordWriter& Writer)
{
	bActionWriterAvailable = Writer.IsAvailable();
	ActionBattleId = Writer.GetBattleId();
	ActionResolutionId = Writer.GetResolutionId();
	++ActionExecutionCount;
}

void UPhase6UIA2AProbeAction::Initialize(
	UPhase6UIA2AProbeState* InState,
	bool bInAppendRecord
)
{
	State = InState;
	bAppendRecord = bInAppendRecord;
}

void UPhase6UIA2AProbeAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (IsValid(State))
	{
		const FPresentationRecordWriter& Writer = GetPresentationRecordWriter();
		State->RecordActionWriter(Writer);
		if (bAppendRecord && Writer.IsAvailable())
		{
			FPresentationRecord Record;
			Record.Type = EBattlePresentationRecordType::None;
			Writer.Append(MoveTemp(Record));
		}
	}
	Finish();
}

void UPhase6UIA2AProbeTrigger::Initialize(UPhase6UIA2AProbeState* InState)
{
	State = InState;
}

bool UPhase6UIA2AProbeTrigger::CanReact(
	const FBattleEvent& Event,
	const FTriggerContext& /*Context*/
) const
{
	return Event.TryGet<FTurnEndedEvent>() != nullptr;
}

void UPhase6UIA2AProbeTrigger::BuildReactions(
	const FBattleEvent& /*Event*/,
	const FTriggerContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(State) || !IsValid(Context.GetActionOuter()))
	{
		return;
	}

	State->RecordContextWriter(Context.GetPresentationRecordWriter());
	UPhase6UIA2AProbeAction* Action = NewObject<UPhase6UIA2AProbeAction>(Context.GetActionOuter());
	Action->Initialize(State.Get(), false);
	OutActions.Add(Action);
}

bool UPhase6UIA2APlaybackWidget::PlayPresentationRecord_Implementation(
	const FPresentationRecord& /*Record*/,
	const FPresentationPlaybackToken& Token
)
{
	++PlayCallCount;
	LastToken = Token;
	return bAcceptAsyncPlayback;
}

void APhase6UIA2ATestPresenter::InvokeBeginPlayForTesting()
{
	BeginPlay();
}
