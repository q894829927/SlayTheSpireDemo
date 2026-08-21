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
	InBattleManager->OnReadStateReady.AddUObject(
		this,
		&UBattlePresentationController::HandleReadStateReady
	);

	FPresentationStateSnapshot Baseline;
	if (InBattleManager->TryGetLatestFrozenPresentationBaseline(Baseline))
	{
		CurrentBattleId = Baseline.BattleId;

		ViewModel->ApplyPresentationSnapshot(Baseline, true);

		const int64 BaselineResolutionWatermark = static_cast<int64>(
			InBattleManager->GetLatestFrozenPresentationBaselineResolutionId()
		);
		LastQueuedResolutionId = BaselineResolutionWatermark;
		LastCompletedResolutionId = BaselineResolutionWatermark;

		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
	}

	if (!InBattleManager->IsPresentationAvailable())
	{
		EnterPresentationUnavailableFailSafe();
	}
	else if (!InBattleManager->IsCommittedPresentationRecordingEnabledForBattle())
	{
		EnterDirectBaselineMode();
	}
	return true;
}

void UBattlePresentationController::Shutdown()
{
	CancelActiveTimeout();
	if (ABattleManager* Battle = BattleManager.Get())
	{
		Battle->OnPresentationResolutionReady.RemoveAll(this);
		Battle->OnReadStateReady.RemoveAll(this);
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

	const bool bHadInFlightPresentation = bHasActiveEnvelope || PlaybackQueue.Num() > 0;
	Widget = InWidget;

	if (bHadInFlightPresentation)
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
	ABattleManager* Battle = BattleManager.Get();
	if (IsValid(Battle) && !Battle->IsPresentationAvailable())
	{
		EnterPresentationUnavailableFailSafe();
		return;
	}
	if (IsValid(Battle) && !Battle->IsCommittedPresentationRecordingEnabledForBattle())
	{
		EnterDirectBaselineMode();
		return;
	}

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
	if (Widget != LostWidget)
	{
		return;
	}

	Widget = nullptr;
	SkipPresentation();
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

	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle))
	{
		return;
	}
	if (!Battle->IsPresentationAvailable())
	{
		EnterPresentationUnavailableFailSafe();
		return;
	}
	if (!Battle->IsCommittedPresentationRecordingEnabledForBattle())
	{
		EnterDirectBaselineMode();
		return;
	}

	FPresentationStateSnapshot LatestBaseline;
	if (!Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline)
		|| Envelope.BattleId != LatestBaseline.BattleId
		|| Envelope.FinalStateRevision > LatestBaseline.StateRevision)
	{
		return;
	}

	if (CurrentBattleId != LatestBaseline.BattleId)
	{
		ResetPlaybackState(true);
		LastQueuedResolutionId = 0;
		LastCompletedResolutionId = 0;
		CurrentBattleId = LatestBaseline.BattleId;
	}

	if (IsValid(ViewModel) && !ViewModel->IsPresentationDisplayOwned())
	{
		ViewModel->SetPresentationDisplayOwned(true);
	}

	if (!IsEnvelopeForCurrentBattle(Envelope)
		|| Envelope.ResolutionId <= LastCompletedResolutionId
		|| Envelope.ResolutionId <= LastQueuedResolutionId)
	{
		return;
	}

	LastQueuedResolutionId = Envelope.ResolutionId;
	const int32 CurrentBacklogCount = PlaybackQueue.Num() + (bHasActiveEnvelope ? 1 : 0);
	if (CurrentBacklogCount >= MaxPlaybackEnvelopes)
	{
		CollapseToEnvelope(Envelope);
		return;
	}

	PlaybackQueue.Add(Envelope);
	StartNextEnvelope();
}

void UBattlePresentationController::HandleReadStateReady(
	uint64 InBattleId,
	uint64 InStateRevision
)
{
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle))
	{
		return;
	}

	if (!Battle->IsPresentationAvailable())
	{
		EnterPresentationUnavailableFailSafe();
		return;
	}

	if (!Battle->IsCommittedPresentationRecordingEnabledForBattle())
	{
		EnterDirectBaselineMode();
		return;
	}

	(void)InBattleId;
	(void)InStateRevision;
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
		CompleteActiveEnvelope();
		return;
	}

	const bool bRecordSupportsVisiblePlayback =
		Record.Type == EBattlePresentationRecordType::ResolutionFault
		|| Record.Type == EBattlePresentationRecordType::Damage
		|| Record.Type == EBattlePresentationRecordType::BlockChanged;
	if (!bRecordSupportsVisiblePlayback)
	{
		// Victory/Defeat are ordered historical facts in A2B but retain immediate
		// fallback until their formal visible treatment is added in UI-A2D.
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

	ABattleManager* Battle = BattleManager.Get();
	if (IsValid(Battle) && !Battle->IsPresentationAvailable())
	{
		EnterPresentationUnavailableFailSafe();
		return;
	}
	if (IsValid(Battle) && !Battle->IsCommittedPresentationRecordingEnabledForBattle())
	{
		EnterDirectBaselineMode();
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
	ABattleManager* Battle = BattleManager.Get();
	if (IsValid(Battle) && !Battle->IsPresentationAvailable())
	{
		EnterPresentationUnavailableFailSafe();
		return;
	}
	if (IsValid(Battle) && !Battle->IsCommittedPresentationRecordingEnabledForBattle())
	{
		EnterDirectBaselineMode();
		return;
	}

	ResetPlaybackState(true);

	if (IsValid(ViewModel))
	{
		ViewModel->ApplyPresentationSnapshot(Envelope.FinalSnapshot, true);
		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
	}
	LastCompletedResolutionId = FMath::Max(LastCompletedResolutionId, Envelope.ResolutionId);
}

void UBattlePresentationController::ResetPlaybackState(bool bAdvanceGeneration)
{
	CancelActiveTimeout();
	if (bAdvanceGeneration)
	{
		AdvancePlaybackGeneration();
	}
	PlaybackQueue.Reset();
	ActiveEnvelope = FPresentationResolutionEnvelope{};
	bHasActiveEnvelope = false;
	bWaitingForCompletion = false;
	ActiveRecordIndex = INDEX_NONE;
	ActivePlaybackToken = FPresentationPlaybackToken{};
}

void UBattlePresentationController::EnterPresentationUnavailableFailSafe()
{
	ABattleManager* Battle = BattleManager.Get();
	ResetPlaybackState(true);

	FPresentationStateSnapshot LatestBaseline;
	if (IsValid(Battle) && Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline))
	{
		CurrentBattleId = LatestBaseline.BattleId;
		const int64 BaselineResolutionWatermark = static_cast<int64>(
			Battle->GetLatestFrozenPresentationBaselineResolutionId()
		);
		LastQueuedResolutionId = BaselineResolutionWatermark;
		LastCompletedResolutionId = BaselineResolutionWatermark;
		if (IsValid(ViewModel))
		{
			ViewModel->ApplyPresentationSnapshot(LatestBaseline, true);
		}
	}

	if (IsValid(ViewModel))
	{
		ViewModel->EnterPresentationUnavailable(
			IsValid(Battle)
				? Battle->GetPresentationUnavailableReason()
				: FText::FromString(TEXT("Committed Presentation is unavailable for this battle."))
		);
	}
}

void UBattlePresentationController::EnterDirectBaselineMode()
{
	ABattleManager* Battle = BattleManager.Get();
	ResetPlaybackState(true);

	if (!IsValid(ViewModel))
	{
		return;
	}

	ViewModel->SetPresentationDisplayOwned(false);

	FPresentationStateSnapshot LatestBaseline;
	if (IsValid(Battle) && Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline))
	{
		CurrentBattleId = LatestBaseline.BattleId;
		const int64 BaselineResolutionWatermark = static_cast<int64>(
			Battle->GetLatestFrozenPresentationBaselineResolutionId()
		);
		LastQueuedResolutionId = BaselineResolutionWatermark;
		LastCompletedResolutionId = BaselineResolutionWatermark;
		ViewModel->ApplyPresentationSnapshot(LatestBaseline, true);
		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
	}
}

void UBattlePresentationController::CancelActiveTimeout()
{
	if (PlaybackTimeoutTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PlaybackTimeoutTickerHandle);
		PlaybackTimeoutTickerHandle.Reset();
	}
	ScheduledTimeoutToken = FPresentationPlaybackToken{};
}

void UBattlePresentationController::ScheduleActiveTimeout()
{
	CancelActiveTimeout();
	ScheduledTimeoutToken = ActivePlaybackToken;
	PlaybackTimeoutTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UBattlePresentationController::HandleActiveTimeout),
		FMath::Max(0.05f, PlaybackTimeoutSeconds)
	);
}

bool UBattlePresentationController::HandleActiveTimeout(float /*DeltaTime*/)
{
	PlaybackTimeoutTickerHandle.Reset();
	const FPresentationPlaybackToken TimeoutToken = ScheduledTimeoutToken;
	ScheduledTimeoutToken = FPresentationPlaybackToken{};
	if (bWaitingForCompletion
		&& TimeoutToken == ActivePlaybackToken
		&& TimeoutToken.LocalPlaybackGeneration == LocalPlaybackGeneration
		&& TimeoutToken.BattleId == CurrentBattleId)
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
