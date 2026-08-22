#include "Phase6UIA2ATestTypes.h"

#include "Actions/BattleActionQueue.h"
#include "Cards/CardPlayContext.h"
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

void UPhase6UIA2AProbeCardEffect::Initialize(
	UPhase6UIA2AProbeState* InState,
	bool bInAppendRecord
)
{
	State = InState;
	bAppendRecord = bInAppendRecord;
}

void UPhase6UIA2AProbeCardEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(State) || !IsValid(Context.ActionOuter))
	{
		return;
	}

	State->RecordContextWriter(Context.PresentationRecordWriter);
	UPhase6UIA2AProbeAction* Action = NewObject<UPhase6UIA2AProbeAction>(Context.ActionOuter);
	Action->Initialize(State.Get(), bAppendRecord);
	OutActions.Add(Action);
}

void UPhase6UIA2AProbeCardEffect::GetPreviewArgumentNames(TArray<FName>& OutNames) const
{
	OutNames.Reset();
}

void UPhase6UIA2AProbeCardEffect::BuildPreviewArguments(
	const FCardEffectPreviewContext& /*Context*/,
	FPreviewTextArgumentBuilder& /*OutArguments*/
) const
{
}

void UPhase6UIA2AProbeCardEffect::ValidatePreviewConfiguration(TArray<FText>& OutErrors) const
{
	OutErrors.Reset();
}

void UPhase6UIA2AProbeTrigger::Initialize(UPhase6UIA2AProbeState* InState)
{
	State = InState;
}

bool UPhase6UIA2AProbeTrigger::CanReact(
	const FBattleEvent& Event,
	const FTriggerContext& Context
) const
{
	const FTurnEndedEvent* TurnEnded = Event.TryGet<FTurnEndedEvent>();
	return TurnEnded != nullptr && TurnEnded->TurnOwner == Context.GetOwner();
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
	if (bNotifySynchronouslyFromPlay)
	{
		NotifyPresentationFinished(Token);
	}
	return bAcceptAsyncPlayback;
}

void UPhase6UIA2APlaybackWidget::CancelPresentationRecordPlayback_Implementation()
{
	++CancelCallCount;
}

bool APhase6UIA2ATestPresenter::InvokeInitializeHUDForTesting(APlayerController* PlayerController)
{
	return InitializeHUD(PlayerController);
}

void APhase6UIA2ATestPresenter::InvokeShutdownHUDForTesting()
{
	ShutdownHUD();
}
