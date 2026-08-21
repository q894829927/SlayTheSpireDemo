#include "BattlePresentationController.h"

#include "../Battle/BattleManager.h"
#include "../UI/BattleHUDViewModel.h"
#include "../UI/BattleHUDWidgetBase.h"

bool UBattlePresentationController::Initialize(
	ABattleManager* InBattleManager,
	UBattleHUDViewModel* InViewModel,
	UBattleHUDWidgetBase* InWidget
)
{
	Shutdown();
	if (!IsValid(InBattleManager) || !IsValid(InViewModel))
	{
		return false;
	}

	BattleManager = InBattleManager;
	ViewModel = InViewModel;
	Widget = InWidget;
	InBattleManager->OnPresentationResolutionReady.AddUObject(
		this,
		&UBattlePresentationController::HandlePresentationResolutionReady
	);

	FPresentationStateSnapshot Baseline;
	if (InBattleManager->TryGetLatestFrozenPresentationBaseline(Baseline))
	{
		CurrentBattleId = Baseline.BattleId;
		// Late subscribers do not replay history. They start from the newest frozen
		// baseline already applied by the ViewModel and may bind input only if that
		// exact revision is still current.
		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
	}
	return true;
}

void UBattlePresentationController::Shutdown()
{
	CancelActiveTimeout();
	if (ABattleManager* Battle = BattleManager.Get())
	{
		Battle->OnPresentationResolutionReady.RemoveAll(this);
	}

	AdvancePlaybackGeneration();
	BattleManager.Reset();
	Widget = nullptr;
	ViewModel = nullptr;
	PlaybackQueue.Reset();
	ActiveEnvelope = FPresentationResolutionEnvelope{};
	bHasActiveEnvelope = false;
	bWaitingForCompletion = false;
	ActiveRecordIndex = INDEX_NONE;
	CurrentBattleId = 0;
	LastQueuedResolutionId = 0;
	LastCompletedResolutionId = 0;
	ActivePlaybackToken = FPresentationPlaybackToken{};
}

void UBattlePresentationController::SetWidget(UBattleHUDWidgetBase* InWidget)
{
	if (Widget == InWidget)
	{
		return;
	}

	Widget = InWidget;
	if (!IsValid(Widget) && (bHasActiveEnvelope || PlaybackQueue.Num() > 0))
	{
		SkipPresentation();
	}
}

void UBattlePresentationController::NotifyPresentationFinished(
	const FPresentationPlaybackToken& Token
)
{
	if (!bWaitingForCompletion
		|| Token != ActivePlaybackToken
		|| Token.LocalPlaybackGeneration != LocalPlaybackGeneration
		|| Token.BattleId != CurrentBattleId)
	{
		return;
	}

	CancelActiveTimeout();
	CompleteActiveRecord();
}

void UBattlePresentationController::SkipPresentation()
{
	CancelActiveTimeout();
	AdvancePlaybackGeneration();

	const FPresentationResolutionEnvelope* Newest = nullptr;
	if (bHasActiveEnvelope)
	{
		Newest = &ActiveEnvelope;
	}
	if (PlaybackQueue.Num() > 0)
	{
		Newest = &PlaybackQueue.Last();
	}

	if (Newest && IsValid(ViewModel))
	{
		ViewModel->ApplyPresentationSnapshot(Newest->FinalSnapshot, true);
		LastCompletedResolutionId = FMath::Max(LastCompletedResolutionId, Newest->ResolutionId);
	}

	PlaybackQueue.Reset();
	ActiveEnvelope = FPresentationResolutionEnvelope{};
	bHasActiveEnvelope = false;
	bWaitingForCompletion = false;
	ActiveRecordIndex = INDEX_NONE;
	ActivePlaybackToken = FPresentationPlaybackToken{};

	if (IsValid(ViewModel))
	{
		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
	}
}

void UBattlePresentationController::NotifyWidgetLost(UBattleHUDWidgetBase* LostWidget)
{
	if (Widget == LostWidget)
	{
		Widget = nullptr;
		SkipPresentation();
	}
}

void UBattlePresentationController::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UBattlePresentationController::HandlePresentationResolutionReady(
	const FPresentationResolutionEnvelope& Envelope
)
{
	if (Envelope.BattleId <= 0 || Envelope.ResolutionId <= 0)
	{
		return;
	}

	if (CurrentBattleId != 0 && Envelope.BattleId != CurrentBattleId)
	{
		// A new battle invalidates every old callback/backlog deterministically.
		CancelActiveTimeout();
		AdvancePlaybackGeneration();
		PlaybackQueue.Reset();
		ActiveEnvelope = FPresentationResolutionEnvelope{};
		bHasActiveEnvelope = false;
		bWaitingForCompletion = false;
		ActiveRecordIndex = INDEX_NONE;
		LastQueuedResolutionId = 0;
		LastCompletedResolutionId = 0;
	}
	CurrentBattleId = Envelope.BattleId;

	if (!IsEnvelopeForCurrentBattle(Envelope)
		|| Envelope.ResolutionId <= LastCompletedResolutionId
		|| Envelope.ResolutionId <= LastQueuedResolutionId)
	{
		return;
	}

	LastQueuedResolutionId = Envelope.ResolutionId;
	if (PlaybackQueue.Num() >= MaxPlaybackEnvelopes)
	{
		CollapseToEnvelope(Envelope);
		return;
	}

	PlaybackQueue.Add(Envelope);
	StartNextEnvelope();
}

void UBattlePresentationController::StartNextEnvelope()
{
	if (bHasActiveEnvelope || PlaybackQueue.Num() == 0)
	{
		return;
	}

	ActiveEnvelope = MoveTemp(PlaybackQueue[0]);
	PlaybackQueue.RemoveAt(0);
	bHasActiveEnvelope = true;
	bWaitingForCompletion = false;
	ActiveRecordIndex = INDEX_NONE;

	if (ActiveEnvelope.Records.Num() == 0)
	{
		CompleteActiveEnvelope();
		return;
	}

	ActiveRecordIndex = 0;
	StartNextRecord();
}

void UBattlePresentationController::StartNextRecord()
{
	if (!bHasActiveEnvelope || !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex))
	{
		CompleteActiveEnvelope();
		return;
	}

	const FPresentationRecord& Record = ActiveEnvelope.Records[ActiveRecordIndex];
	if (Record.BattleId != ActiveEnvelope.BattleId
		|| Record.ResolutionId != ActiveEnvelope.ResolutionId
		|| Record.PresentationSequence <= 0)
	{
		// Invalid presentation data degrades to the envelope's frozen final state;
		// Gameplay is never involved.
		CompleteActiveEnvelope();
		return;
	}

	// A2A only transports ResolutionFault as a meaningful record. Unknown/None
	// records use immediate fallback until their owning later slice implements
	// visible playback.
	if (Record.Type != EBattlePresentationRecordType::ResolutionFault)
	{
		CompleteActiveRecord();
		return;
	}

	ActivePlaybackToken.BattleId = Record.BattleId;
	ActivePlaybackToken.ResolutionId = Record.ResolutionId;
	ActivePlaybackToken.PresentationSequence = Record.PresentationSequence;
	ActivePlaybackToken.LocalPlaybackGeneration = LocalPlaybackGeneration;

	bWaitingForCompletion = true;
	const bool bBlueprintAcceptedPlayback = IsValid(Widget)
		&& Widget->PlayPresentationRecord(Record, ActivePlaybackToken);
	if (!bBlueprintAcceptedPlayback)
	{
		bWaitingForCompletion = false;
		CompleteActiveRecord();
		return;
	}

	ScheduleActiveTimeout();
}

void UBattlePresentationController::CompleteActiveRecord()
{
	CancelActiveTimeout();
	if (!bHasActiveEnvelope)
	{
		return;
	}

	bWaitingForCompletion = false;
	ActivePlaybackToken = FPresentationPlaybackToken{};
	++ActiveRecordIndex;
	if (ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex))
	{
		StartNextRecord();
		return;
	}

	CompleteActiveEnvelope();
}

void UBattlePresentationController::CompleteActiveEnvelope()
{
	CancelActiveTimeout();
	if (!bHasActiveEnvelope)
	{
		return;
	}

	const int64 CompletedResolutionId = ActiveEnvelope.ResolutionId;
	if (IsValid(ViewModel))
	{
		ViewModel->ApplyPresentationSnapshot(ActiveEnvelope.FinalSnapshot, true);
	}

	LastCompletedResolutionId = FMath::Max(LastCompletedResolutionId, CompletedResolutionId);
	ActiveEnvelope = FPresentationResolutionEnvelope{};
	bHasActiveEnvelope = false;
	bWaitingForCompletion = false;
	ActiveRecordIndex = INDEX_NONE;
	ActivePlaybackToken = FPresentationPlaybackToken{};

	if (PlaybackQueue.Num() > 0)
	{
		StartNextEnvelope();
		return;
	}

	if (IsValid(ViewModel))
	{
		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
	}
}

void UBattlePresentationController::CollapseToEnvelope(
	const FPresentationResolutionEnvelope& Envelope
)
{
	CancelActiveTimeout();
	AdvancePlaybackGeneration();
	PlaybackQueue.Reset();
	ActiveEnvelope = FPresentationResolutionEnvelope{};
	bHasActiveEnvelope = false;
	bWaitingForCompletion = false;
	ActiveRecordIndex = INDEX_NONE;
	ActivePlaybackToken = FPresentationPlaybackToken{};

	if (IsValid(ViewModel))
	{
		ViewModel->ApplyPresentationSnapshot(Envelope.FinalSnapshot, true);
		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
	}
	LastCompletedResolutionId = FMath::Max(LastCompletedResolutionId, Envelope.ResolutionId);
}

void UBattlePresentationController::CancelActiveTimeout()
{
	if (PlaybackTimeoutTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PlaybackTimeoutTickerHandle);
		PlaybackTimeoutTickerHandle.Reset();
	}
}

void UBattlePresentationController::ScheduleActiveTimeout()
{
	CancelActiveTimeout();
	PlaybackTimeoutTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UBattlePresentationController::HandleActiveTimeout),
		FMath::Max(0.05f, PlaybackTimeoutSeconds)
	);
}

bool UBattlePresentationController::HandleActiveTimeout(float /*DeltaTime*/)
{
	PlaybackTimeoutTickerHandle.Reset();
	if (bWaitingForCompletion)
	{
		CompleteActiveRecord();
	}
	return false;
}

void UBattlePresentationController::AdvancePlaybackGeneration()
{
	++LocalPlaybackGeneration;
	if (LocalPlaybackGeneration <= 0)
	{
		LocalPlaybackGeneration = 1;
	}
}

bool UBattlePresentationController::IsEnvelopeForCurrentBattle(
	const FPresentationResolutionEnvelope& Envelope
) const
{
	return CurrentBattleId > 0
		&& Envelope.BattleId == CurrentBattleId
		&& Envelope.FinalSnapshot.BattleId == Envelope.BattleId
		&& Envelope.FinalStateRevision == Envelope.FinalSnapshot.StateRevision;
}

#if WITH_DEV_AUTOMATION_TESTS
int32 UBattlePresentationController::GetBacklogCountForTesting() const
{
	return PlaybackQueue.Num() + (bHasActiveEnvelope ? 1 : 0);
}

bool UBattlePresentationController::IsWaitingForCompletionForTesting() const
{
	return bWaitingForCompletion;
}

FPresentationPlaybackToken UBattlePresentationController::GetActivePlaybackTokenForTesting() const
{
	return ActivePlaybackToken;
}

int64 UBattlePresentationController::GetLastCompletedResolutionIdForTesting() const
{
	return LastCompletedResolutionId;
}
#endif
