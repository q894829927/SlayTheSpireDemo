#include "BattlePresentationController.h"

#include "PresentationCardView.h"
#include "PresentationDamageReducer.h"
#include "../Battle/BattleManager.h"
#include "../UI/BattleHUDViewModel.h"
#include "../UI/BattleHUDWidgetBase.h"

namespace
{
	// Process-lifetime mint authority. Controller replacement must never be able
	// to reproduce an old PresentationSessionToken numerically.
	int64 GNextPresentationControllerEpoch = 1;

	int64 AllocatePresentationControllerEpoch()
	{
		checkf(
			GNextPresentationControllerEpoch > 0
				&& GNextPresentationControllerEpoch < MAX_int64,
			TEXT("Presentation Controller epoch authority exhausted."));
		return GNextPresentationControllerEpoch++;
	}

	FBattleHUDCombatantView* FindCombatantView(
		FPresentationStateSnapshot& Snapshot,
		FName PresentationId
	)
	{
		if (PresentationId.IsNone())
		{
			return nullptr;
		}
		if (Snapshot.Player.PresentationId == PresentationId)
		{
			return &Snapshot.Player;
		}
		if (Snapshot.Enemy.PresentationId == PresentationId)
		{
			return &Snapshot.Enemy;
		}
		return nullptr;
	}

	bool IsOptionalParticipantPresentationId(
		const FPresentationStateSnapshot& Snapshot,
		FName PresentationId
	)
	{
		return PresentationId.IsNone()
			|| PresentationId == Snapshot.Player.PresentationId
			|| PresentationId == Snapshot.Enemy.PresentationId;
	}

	bool IsTerminalRecordType(EBattlePresentationRecordType Type)
	{
		return Type == EBattlePresentationRecordType::Victory
			|| Type == EBattlePresentationRecordType::Defeat
			|| Type == EBattlePresentationRecordType::ResolutionFault;
	}

	bool IsTerminalSnapshot(const FPresentationStateSnapshot& Snapshot)
	{
		return Snapshot.BattleState == EBattleState::Victory
			|| Snapshot.BattleState == EBattleState::Defeat
			|| Snapshot.BattleState == EBattleState::ResolutionFaulted
			|| Snapshot.Outcome != EBattleHUDOutcome::None;
	}

	bool ValidateTerminalEnvelopeShape(const FPresentationResolutionEnvelope& Envelope)
	{
		int32 TerminalIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Envelope.Records.Num(); ++Index)
		{
			if (!IsTerminalRecordType(Envelope.Records[Index].Type))
			{
				continue;
			}
			if (TerminalIndex != INDEX_NONE)
			{
				return false;
			}
			TerminalIndex = Index;
		}

		if (TerminalIndex != INDEX_NONE && TerminalIndex != Envelope.Records.Num() - 1)
		{
			return false;
		}

		const bool bFinalIsTerminal = IsTerminalSnapshot(Envelope.FinalSnapshot);
		if (!bFinalIsTerminal)
		{
			return TerminalIndex == INDEX_NONE;
		}
		if (TerminalIndex == INDEX_NONE)
		{
			return false;
		}

		const EBattlePresentationRecordType Type = Envelope.Records[TerminalIndex].Type;
		switch (Envelope.FinalSnapshot.BattleState)
		{
		case EBattleState::Victory:
			return Envelope.FinalSnapshot.Outcome == EBattleHUDOutcome::Victory
				&& Type == EBattlePresentationRecordType::Victory;
		case EBattleState::Defeat:
			return Envelope.FinalSnapshot.Outcome == EBattleHUDOutcome::Defeat
				&& Type == EBattlePresentationRecordType::Defeat;
		case EBattleState::ResolutionFaulted:
			return Envelope.FinalSnapshot.Outcome == EBattleHUDOutcome::ResolutionFaulted
				&& Type == EBattlePresentationRecordType::ResolutionFault;
		default:
			return false;
		}
	}

	bool ApplyTerminalRecord(
		FPresentationStateSnapshot& Snapshot,
		const FPresentationStateSnapshot& FinalSnapshot,
		const FPresentationRecord& Record
	)
	{
		if (!IsTerminalRecordType(Record.Type)
			|| Snapshot.BattleId <= 0
			|| Snapshot.BattleId != FinalSnapshot.BattleId
			|| IsTerminalSnapshot(Snapshot))
		{
			return false;
		}

		switch (Record.Type)
		{
		case EBattlePresentationRecordType::Victory:
			if (Record.Terminal.WinnerPresentationId.IsNone()
				|| Record.Terminal.DefeatedPresentationId.IsNone()
				|| Record.Terminal.WinnerPresentationId == Record.Terminal.DefeatedPresentationId
				|| Record.Terminal.WinnerPresentationId != Snapshot.Player.PresentationId
				|| Record.Terminal.WinnerPresentationId != FinalSnapshot.Player.PresentationId
				|| Record.Terminal.DefeatedPresentationId != Snapshot.Enemy.PresentationId
				|| Record.Terminal.DefeatedPresentationId != FinalSnapshot.Enemy.PresentationId
				|| !Snapshot.Enemy.bDead
				|| !FinalSnapshot.Enemy.bDead
				|| FinalSnapshot.BattleState != EBattleState::Victory
				|| FinalSnapshot.Outcome != EBattleHUDOutcome::Victory)
			{
				return false;
			}
			Snapshot.BattleState = EBattleState::Victory;
			Snapshot.Outcome = EBattleHUDOutcome::Victory;
			Snapshot.bCanEndTurn = false;
			return true;

		case EBattlePresentationRecordType::Defeat:
			if (Record.Terminal.WinnerPresentationId.IsNone()
				|| Record.Terminal.DefeatedPresentationId.IsNone()
				|| Record.Terminal.WinnerPresentationId == Record.Terminal.DefeatedPresentationId
				|| Record.Terminal.WinnerPresentationId != Snapshot.Enemy.PresentationId
				|| Record.Terminal.WinnerPresentationId != FinalSnapshot.Enemy.PresentationId
				|| Record.Terminal.DefeatedPresentationId != Snapshot.Player.PresentationId
				|| Record.Terminal.DefeatedPresentationId != FinalSnapshot.Player.PresentationId
				|| !Snapshot.Player.bDead
				|| !FinalSnapshot.Player.bDead
				|| FinalSnapshot.BattleState != EBattleState::Defeat
				|| FinalSnapshot.Outcome != EBattleHUDOutcome::Defeat)
			{
				return false;
			}
			Snapshot.BattleState = EBattleState::Defeat;
			Snapshot.Outcome = EBattleHUDOutcome::Defeat;
			Snapshot.bCanEndTurn = false;
			return true;

		case EBattlePresentationRecordType::ResolutionFault:
			if (Record.ResolutionFault.Reason.IsEmpty()
				|| Record.ResolutionFault.ExecutedActionCount < 0
				|| FinalSnapshot.BattleState != EBattleState::ResolutionFaulted
				|| FinalSnapshot.Outcome != EBattleHUDOutcome::ResolutionFaulted)
			{
				return false;
			}
			Snapshot.BattleState = EBattleState::ResolutionFaulted;
			Snapshot.Outcome = EBattleHUDOutcome::ResolutionFaulted;
			Snapshot.bCanEndTurn = false;
			return true;

		default:
			return false;
		}
	}

	int32 FindHandCardIndexByRuntimeId(
		const TArray<FBattleHUDCardView>& HandCards,
		int32 RuntimeId
	)
	{
		return HandCards.IndexOfByPredicate(
			[RuntimeId](const FBattleHUDCardView& Card)
			{
				return Card.RuntimeId == RuntimeId;
			}
		);
	}

	int32 FindStatusIndexByIdentity(
		const TArray<FBattleHUDStatusView>& Statuses,
		FName StatusId,
		int64 RuntimeSequence
	)
	{
		if (StatusId.IsNone() || RuntimeSequence <= 0)
		{
			return INDEX_NONE;
		}
		return Statuses.IndexOfByPredicate(
			[StatusId, RuntimeSequence](const FBattleHUDStatusView& Status)
			{
				return Status.StatusId == StatusId
					&& Status.RuntimeSequence == RuntimeSequence;
			}
		);
	}

	bool ValidateStatusViewArray(const TArray<FBattleHUDStatusView>& Statuses)
	{
		for (int32 Index = 0; Index < Statuses.Num(); ++Index)
		{
			const FBattleHUDStatusView& Status = Statuses[Index];
			if (Status.StatusId.IsNone() || Status.RuntimeSequence <= 0 || Status.Amount <= 0)
			{
				return false;
			}
			if (Index > 0 && Statuses[Index - 1].RuntimeSequence >= Status.RuntimeSequence)
			{
				return false;
			}
			for (int32 OtherIndex = Index + 1; OtherIndex < Statuses.Num(); ++OtherIndex)
			{
				if (Statuses[OtherIndex].StatusId == Status.StatusId)
				{
					return false;
				}
			}
		}
		return true;
	}

	bool ValidateStatusChangedPayload(const FStatusChangedPresentationPayload& Payload)
	{
		if (Payload.TargetPresentationId.IsNone()
			|| Payload.StatusId.IsNone()
			|| Payload.RuntimeSequence <= 0
			|| Payload.AmountBefore < 0
			|| Payload.AmountAfter < 0
			|| (Payload.bCreated && Payload.bRemoved)
			|| (Payload.bCreated && !Payload.DescriptionBefore.IsEmpty())
			|| (Payload.bRemoved && !Payload.DescriptionAfter.IsEmpty()))
		{
			return false;
		}

		switch (Payload.Reason)
		{
		case EStatusChangeReason::Applied:
			return Payload.bCreated
				&& !Payload.bRemoved
				&& Payload.AmountBefore == 0
				&& Payload.AmountAfter > 0;
		case EStatusChangeReason::Increased:
			return !Payload.bCreated
				&& !Payload.bRemoved
				&& Payload.AmountBefore > 0
				&& Payload.AmountAfter > Payload.AmountBefore;
		case EStatusChangeReason::Reduced:
		case EStatusChangeReason::TurnEndDecay:
			return !Payload.bCreated
				&& Payload.AmountBefore > Payload.AmountAfter
				&& Payload.AmountAfter >= 0
				&& Payload.bRemoved == (Payload.AmountAfter == 0);
		case EStatusChangeReason::Removed:
			return !Payload.bCreated
				&& Payload.bRemoved
				&& Payload.AmountBefore > 0
				&& Payload.AmountAfter == 0;
		default:
			return false;
		}
	}

	void ApplyStatusMetadata(
		const FStatusChangedPresentationPayload& Payload,
		FBattleHUDStatusView& View
	)
	{
		View.StatusId = Payload.StatusId;
		View.RuntimeSequence = Payload.RuntimeSequence;
		View.DisplayName = Payload.DisplayName.IsEmpty()
			? FText::FromName(Payload.StatusId)
			: Payload.DisplayName;
		View.Description = Payload.DescriptionAfter;
		View.Amount = Payload.AmountAfter;
		View.bUseAtlasIcon = Payload.bUseAtlasIcon;
		View.UVOffset = Payload.UVOffset;
		View.UVScale = Payload.UVScale;
		View.TrimOffset = Payload.TrimOffset;
		View.TrimScale = Payload.TrimScale;
	}

	bool ApplyStatusChangedRecord(
		FPresentationStateSnapshot& Snapshot,
		const FStatusChangedPresentationPayload& Payload
	)
	{
		if (!ValidateStatusChangedPayload(Payload)
			|| !IsOptionalParticipantPresentationId(Snapshot, Payload.SourcePresentationId))
		{
			return false;
		}

		FBattleHUDCombatantView* Target = FindCombatantView(Snapshot, Payload.TargetPresentationId);
		if (Target == nullptr || !ValidateStatusViewArray(Target->Statuses))
		{
			return false;
		}

		if (Payload.bCreated)
		{
			for (const FBattleHUDStatusView& Existing : Target->Statuses)
			{
				if (Existing.StatusId == Payload.StatusId
					|| Existing.RuntimeSequence == Payload.RuntimeSequence)
				{
					return false;
				}
			}

			FBattleHUDStatusView NewStatus;
			ApplyStatusMetadata(Payload, NewStatus);
			int32 InsertIndex = 0;
			while (InsertIndex < Target->Statuses.Num()
				&& Target->Statuses[InsertIndex].RuntimeSequence < Payload.RuntimeSequence)
			{
				++InsertIndex;
			}
			Target->Statuses.Insert(MoveTemp(NewStatus), InsertIndex);
			return ValidateStatusViewArray(Target->Statuses);
		}

		const int32 StatusIndex = FindStatusIndexByIdentity(
			Target->Statuses,
			Payload.StatusId,
			Payload.RuntimeSequence
		);
		if (StatusIndex == INDEX_NONE
			|| Target->Statuses[StatusIndex].Amount != Payload.AmountBefore)
		{
			return false;
		}

		if (Payload.bRemoved)
		{
			Target->Statuses.RemoveAt(StatusIndex);
			return ValidateStatusViewArray(Target->Statuses);
		}

		ApplyStatusMetadata(Payload, Target->Statuses[StatusIndex]);
		return ValidateStatusViewArray(Target->Statuses);
	}
}

bool UBattlePresentationController::Initialize(
	ABattleManager* InBattleManager,
	UBattleHUDViewModel* InViewModel,
	UBattleHUDWidgetBase* InWidget
)
{
	Shutdown();
	EnsureControllerEpoch();
	if (!IsValid(InBattleManager) || !IsValid(InViewModel))
	{
		return false;
	}

	BattleManager = InBattleManager;
	ViewModel = InViewModel;
	Widget = InWidget;
	InBattleManager->OnPresentationResolutionReady.AddUObject(this, &UBattlePresentationController::HandlePresentationResolutionReady);
	InBattleManager->OnReadStateReady.AddUObject(this, &UBattlePresentationController::HandleReadStateReady);

	if (InBattleManager->IsPresentationAvailable()
		&& InBattleManager->IsCommittedPresentationRecordingEnabledForBattle())
	{
		InViewModel->SetPresentationDisplayOwned(true);
	}

	FPresentationStateSnapshot Baseline;
	if (InBattleManager->TryGetLatestFrozenPresentationBaseline(Baseline))
	{
		CurrentBattleId = Baseline.BattleId;
		ApplyDisplayedSnapshot(Baseline, false);

		const int64 BaselineResolutionWatermark = static_cast<int64>(InBattleManager->GetLatestFrozenPresentationBaselineResolutionId());
		LastQueuedResolutionId = BaselineResolutionWatermark;
		LastCompletedResolutionId = BaselineResolutionWatermark;
		EstablishPresentationSessionForCurrentBinding();
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
	InvalidatePresentationSession(Widget);
	if (IsValid(Widget))
	{
		Widget->CancelAllDetachedDamageVisuals();
	}
	CancelActiveTimeout();
	CancelActivePlaybackUnit();
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
	DisplayedPresentationSnapshot = FPresentationStateSnapshot{};
	WorkingPresentationSnapshot = FPresentationStateSnapshot{};
	bHasActiveEnvelope = false;
	bHasDisplayedPresentationSnapshot = false;
	bHasWorkingPresentationSnapshot = false;
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
	UBattleHUDWidgetBase* PreviousWidget = Widget;

	// Binding replacement is an authority replacement. Retire both the G8
	// session and the old G0-G7 playback owner before installing the new Widget;
	// old playback tokens must never be delivered to the new owner during catch-up.
	InvalidatePresentationSession(PreviousWidget);
	RetireActivePlaybackForWidgetReplacement(PreviousWidget);
	Widget = InWidget;
	EstablishPresentationSessionForCurrentBinding();

	if (bHadInFlightPresentation)
	{
		SkipPresentation();
	}
}

bool UBattlePresentationController::TryGetPresentationSessionToken(
	FPresentationSessionToken& OutToken) const
{
	OutToken = FPresentationSessionToken{};
	if (!ActivePresentationSessionToken.IsValid())
	{
		return false;
	}
	OutToken = ActivePresentationSessionToken;
	return true;
}

bool UBattlePresentationController::IsCurrentPresentationSession(
	const FPresentationSessionToken& Token) const
{
	return Token.IsValid()
		&& ActivePresentationSessionToken.IsValid()
		&& Token == ActivePresentationSessionToken;
}

void UBattlePresentationController::NotifyPresentationFinished(const FPresentationPlaybackToken& Token)
{
	if (!bWaitingForCompletion
		|| Token != ActivePlaybackToken
		|| Token.LocalPlaybackGeneration != LocalPlaybackGeneration
		|| Token.BattleId != CurrentBattleId)
	{
		return;
	}

	CancelActiveTimeout();
	if (Token.UnitKind == EPresentationPlaybackUnitKind::Group)
	{
		// G3 does not enable production Group playback. A Group completion on the
		// common surface must recover the whole ActiveEnvelope, never leader-only.
		ReconcileActiveEnvelopeToFinalSnapshot();
		return;
	}
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

	// Ordinary catch-up changes chronology, not Presentation authority. G8 session
	// identity intentionally remains unchanged here.
	CancelActiveTimeout();
	CancelActivePlaybackUnit();
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

	if (Newest)
	{
		ApplyDisplayedSnapshot(Newest->FinalSnapshot, false);
		MarkEntireBacklogCompletedExact(nullptr);
		LastCompletedResolutionId = FMath::Max(LastCompletedResolutionId, Newest->ResolutionId);
	}

	PlaybackQueue.Reset();
	ActiveEnvelope = FPresentationResolutionEnvelope{};
	WorkingPresentationSnapshot = FPresentationStateSnapshot{};
	bHasActiveEnvelope = false;
	bHasWorkingPresentationSnapshot = false;
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

	InvalidatePresentationSession(LostWidget);
	RetireActivePlaybackForWidgetReplacement(LostWidget);
	Widget = nullptr;
	SkipPresentation();
}

void UBattlePresentationController::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UBattlePresentationController::HandlePresentationResolutionReady(const FPresentationResolutionEnvelope& Envelope)
{
	// Receipts precede playback even if the Controller's delegate was registered
	// before the ViewModel's listener. Zero-record envelopes also carry outcomes.
	if (IsValid(ViewModel)) ViewModel->AcceptSelectionPresentationOutcomes(Envelope);
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
		InvalidatePresentationSession(Widget);
		ResetPlaybackState(true);
		LastQueuedResolutionId = 0;
		LastCompletedResolutionId = 0;
		CurrentBattleId = LatestBaseline.BattleId;
		ApplyDisplayedSnapshot(LatestBaseline, false);
	}

	if (IsValid(ViewModel) && !ViewModel->IsPresentationDisplayOwned())
	{
		ViewModel->SetPresentationDisplayOwned(true);
	}
	if (!ActivePresentationSessionToken.IsValid())
	{
		EstablishPresentationSessionForCurrentBinding();
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
		CollapseEntireBacklogToEnvelope(Envelope);
		return;
	}

	PlaybackQueue.Add(Envelope);
	if (IsValid(ViewModel) && ViewModel->IsSelectionPresentationSubmitInProgress())
	{
		// Queue is UPROPERTY-owned; do not capture UObject-bearing envelopes in a
		// non-GC-tracked timer while Confirm is still a fallible transaction.
		const TWeakObjectPtr<UBattlePresentationController> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			if (UBattlePresentationController* Controller = WeakThis.Get()) Controller->StartNextEnvelope();
			return false;
		}));
		return;
	}
	StartNextEnvelope();
}

void UBattlePresentationController::HandleReadStateReady(uint64 InBattleId, uint64 InStateRevision)
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
	ActivePlaybackToken = FPresentationPlaybackToken{};

	if (!bHasDisplayedPresentationSnapshot
		|| DisplayedPresentationSnapshot.BattleId != ActiveEnvelope.BattleId
		|| !ValidateTerminalEnvelopeShape(ActiveEnvelope))
	{
		ReconcileActiveEnvelopeToFinalSnapshot();
		return;
	}

	WorkingPresentationSnapshot = DisplayedPresentationSnapshot;
	bHasWorkingPresentationSnapshot = true;

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
		ReconcileActiveEnvelopeToFinalSnapshot();
		return;
	}

	const bool bRecordSupportsVisiblePlayback =
		Record.Type == EBattlePresentationRecordType::ResolutionFault
		|| Record.Type == EBattlePresentationRecordType::Victory
		|| Record.Type == EBattlePresentationRecordType::Defeat
		|| Record.Type == EBattlePresentationRecordType::Damage
		|| Record.Type == EBattlePresentationRecordType::BlockChanged
		|| Record.Type == EBattlePresentationRecordType::CardPlayed
		|| Record.Type == EBattlePresentationRecordType::EnergyChanged
		|| Record.Type == EBattlePresentationRecordType::CardZoneChanged
		|| Record.Type == EBattlePresentationRecordType::DeckShuffled
		|| Record.Type == EBattlePresentationRecordType::StatusChanged;
	if (!bRecordSupportsVisiblePlayback)
	{
		CompleteActiveRecord();
		return;
	}

	if (Record.Type == EBattlePresentationRecordType::Damage)
	{
		if (!bHasWorkingPresentationSnapshot)
		{
			ReconcileActiveEnvelopeToFinalSnapshot();
			return;
		}
		FPresentationStateSnapshot PreflightSnapshot = WorkingPresentationSnapshot;
		if (!PresentationDamageReducer::TryApplyDamageRecord(
			PreflightSnapshot,
			Record.Damage))
		{
			ReconcileActiveEnvelopeToFinalSnapshot();
			return;
		}
	}
	else if (Record.Type == EBattlePresentationRecordType::StatusChanged)
	{
		if (!bHasWorkingPresentationSnapshot)
		{
			ReconcileActiveEnvelopeToFinalSnapshot();
			return;
		}

		FPresentationStateSnapshot PreflightSnapshot = WorkingPresentationSnapshot;
		if (!ApplyStatusChangedRecord(PreflightSnapshot, Record.StatusChanged))
		{
			ReconcileActiveEnvelopeToFinalSnapshot();
			return;
		}
	}
	else if (IsTerminalRecordType(Record.Type))
	{
		if (!bHasWorkingPresentationSnapshot
			|| ActiveRecordIndex != ActiveEnvelope.Records.Num() - 1)
		{
			ReconcileActiveEnvelopeToFinalSnapshot();
			return;
		}

		FPresentationStateSnapshot PreflightSnapshot = WorkingPresentationSnapshot;
		if (!ApplyTerminalRecord(PreflightSnapshot, ActiveEnvelope.FinalSnapshot, Record))
		{
			ReconcileActiveEnvelopeToFinalSnapshot();
			return;
		}
	}

	// Every offered playback unit receives a fresh generation. This keeps Record
	// and future Group callbacks disjoint even when they share leader sequence.
	AdvancePlaybackGeneration();
	ActivePlaybackToken = FPresentationPlaybackToken{};
	ActivePlaybackToken.BattleId = Record.BattleId;
	ActivePlaybackToken.ResolutionId = Record.ResolutionId;
	ActivePlaybackToken.PresentationSequence = Record.PresentationSequence;
	ActivePlaybackToken.LocalPlaybackGeneration = LocalPlaybackGeneration;
	ActivePlaybackToken.UnitKind = EPresentationPlaybackUnitKind::SingleRecord;
	ActivePlaybackToken.GroupId = 0;

	bWaitingForCompletion = true;
	const bool bBlueprintAcceptedPlayback = IsValid(Widget)
		&& Widget->PlayPresentationRecord(Record, ActivePlaybackToken, ActiveRecordIndex);
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
	if (!bHasActiveEnvelope || !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex))
	{
		return;
	}

	bWaitingForCompletion = false;
	ActivePlaybackToken = FPresentationPlaybackToken{};

	const FPresentationRecord& CompletedRecord = ActiveEnvelope.Records[ActiveRecordIndex];
	if (!ApplyRecordToWorkingSnapshot(CompletedRecord))
	{
		ReconcileActiveEnvelopeToFinalSnapshot();
		return;
	}

	if (IsValid(ViewModel) && bHasWorkingPresentationSnapshot)
	{
		ViewModel->ApplyPresentationSnapshot(WorkingPresentationSnapshot, true);
	}

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
	ApplyDisplayedSnapshot(ActiveEnvelope.FinalSnapshot, false);
	MarkPresentationResolutionCompletedExact(ActiveEnvelope);

	LastCompletedResolutionId = FMath::Max(LastCompletedResolutionId, CompletedResolutionId);
	ActiveEnvelope = FPresentationResolutionEnvelope{};
	WorkingPresentationSnapshot = FPresentationStateSnapshot{};
	bHasActiveEnvelope = false;
	bHasWorkingPresentationSnapshot = false;
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

void UBattlePresentationController::ReconcileActiveEnvelopeToFinalSnapshot()
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

	const FPresentationResolutionEnvelope RecoveredEnvelope = ActiveEnvelope;
	CancelActivePlaybackUnit();
	AdvancePlaybackGeneration();

	ApplyDisplayedSnapshot(RecoveredEnvelope.FinalSnapshot, false);
	MarkPresentationResolutionCompletedExact(RecoveredEnvelope);
	LastCompletedResolutionId = FMath::Max(
		LastCompletedResolutionId,
		RecoveredEnvelope.ResolutionId);

	ActiveEnvelope = FPresentationResolutionEnvelope{};
	WorkingPresentationSnapshot = FPresentationStateSnapshot{};
	bHasActiveEnvelope = false;
	bHasWorkingPresentationSnapshot = false;
	bWaitingForCompletion = false;
	ActiveRecordIndex = INDEX_NONE;
	ActivePlaybackToken = FPresentationPlaybackToken{};

	// Deliberately preserve PlaybackQueue. This is the G3 ActiveEnvelope scope.
	// G8 ordinary reconcile also deliberately preserves PresentationSessionToken.
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

void UBattlePresentationController::CollapseEntireBacklogToEnvelope(
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

	CancelActiveTimeout();
	CancelActivePlaybackUnit();
	AdvancePlaybackGeneration();

	ApplyDisplayedSnapshot(Envelope.FinalSnapshot, false);
	MarkEntireBacklogCompletedExact(&Envelope);
	LastCompletedResolutionId = FMath::Max(LastCompletedResolutionId, Envelope.ResolutionId);

	PlaybackQueue.Reset();
	ActiveEnvelope = FPresentationResolutionEnvelope{};
	WorkingPresentationSnapshot = FPresentationStateSnapshot{};
	bHasActiveEnvelope = false;
	bHasWorkingPresentationSnapshot = false;
	bWaitingForCompletion = false;
	ActiveRecordIndex = INDEX_NONE;
	ActivePlaybackToken = FPresentationPlaybackToken{};

	if (IsValid(ViewModel))
	{
		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
	}
}

void UBattlePresentationController::ResetPlaybackState(bool bAdvanceGeneration)
{
	CancelActiveTimeout();
	CancelActivePlaybackUnit();
	if (bAdvanceGeneration)
	{
		AdvancePlaybackGeneration();
	}
	PlaybackQueue.Reset();
	ActiveEnvelope = FPresentationResolutionEnvelope{};
	WorkingPresentationSnapshot = FPresentationStateSnapshot{};
	bHasActiveEnvelope = false;
	bHasWorkingPresentationSnapshot = false;
	bWaitingForCompletion = false;
	ActiveRecordIndex = INDEX_NONE;
	ActivePlaybackToken = FPresentationPlaybackToken{};
}

void UBattlePresentationController::CancelActivePlaybackUnit()
{
	if (!bWaitingForCompletion)
	{
		return;
	}
	if (IsValid(Widget))
	{
		Widget->CancelTrackedPresentationPlayback(ActivePlaybackToken);
	}
	bWaitingForCompletion = false;
}

void UBattlePresentationController::RetireActivePlaybackForWidgetReplacement(
	UBattleHUDWidgetBase* ExpectedOldWidget)
{
	CancelActiveTimeout();
	if (bWaitingForCompletion
		&& Widget == ExpectedOldWidget
		&& IsValid(ExpectedOldWidget))
	{
		ExpectedOldWidget->CancelTrackedPresentationPlayback(ActivePlaybackToken);
	}
	bWaitingForCompletion = false;
	ActivePlaybackToken = FPresentationPlaybackToken{};
	AdvancePlaybackGeneration();
}

void UBattlePresentationController::EnsureControllerEpoch()
{
	if (ControllerEpoch <= 0)
	{
		ControllerEpoch = AllocatePresentationControllerEpoch();
	}
}

bool UBattlePresentationController::IsPresentationOwnedMode() const
{
	const ABattleManager* Battle = BattleManager.Get();
	return IsValid(Battle)
		&& IsValid(ViewModel)
		&& ViewModel->IsPresentationDisplayOwned()
		&& Battle->IsPresentationAvailable()
		&& Battle->IsCommittedPresentationRecordingEnabledForBattle()
		&& CurrentBattleId > 0;
}

void UBattlePresentationController::InvalidatePresentationSession(
	UBattleHUDWidgetBase* CleanupWidget)
{
	const FPresentationSessionToken OldToken = ActivePresentationSessionToken;
	// Invalidate first so any synchronous cleanup callback already observes stale.
	ActivePresentationSessionToken = FPresentationSessionToken{};
	if (OldToken.IsValid())
	{
		UBattleHUDWidgetBase* Owner = IsValid(CleanupWidget) ? CleanupWidget : Widget.Get();
		if (IsValid(Owner))
		{
			Owner->CancelDetachedDamageVisualsForSession(OldToken);
		}
	}
}

void UBattlePresentationController::EstablishPresentationSessionForCurrentBinding()
{
	if (!IsPresentationOwnedMode() || !IsValid(Widget))
	{
		ActivePresentationSessionToken = FPresentationSessionToken{};
		return;
	}

	EnsureControllerEpoch();
	checkf(
		NextPresentationSessionGeneration > 0
			&& NextPresentationSessionGeneration < MAX_int64,
		TEXT("Presentation session generation exhausted."));

	FPresentationSessionToken NewToken;
	NewToken.BattleId = CurrentBattleId;
	NewToken.ControllerEpoch = ControllerEpoch;
	NewToken.PresentationSessionGeneration = NextPresentationSessionGeneration++;
	ActivePresentationSessionToken = NewToken;
}

void UBattlePresentationController::MarkPresentationResolutionCompletedExact(
	const FPresentationResolutionEnvelope& Envelope
)
{
	if (IsValid(ViewModel)
		&& Envelope.BattleId > 0
		&& Envelope.ResolutionId > 0)
	{
		ViewModel->MarkPresentationResolutionCompleted(
			Envelope.BattleId,
			Envelope.ResolutionId);
	}
}

void UBattlePresentationController::MarkEntireBacklogCompletedExact(
	const FPresentationResolutionEnvelope* AdditionalEnvelope
)
{
	if (bHasActiveEnvelope)
	{
		MarkPresentationResolutionCompletedExact(ActiveEnvelope);
	}
	for (const FPresentationResolutionEnvelope& Envelope : PlaybackQueue)
	{
		MarkPresentationResolutionCompletedExact(Envelope);
	}
	if (AdditionalEnvelope != nullptr)
	{
		MarkPresentationResolutionCompletedExact(*AdditionalEnvelope);
	}
}

void UBattlePresentationController::EnterPresentationUnavailableFailSafe()
{
	ABattleManager* Battle = BattleManager.Get();
	InvalidatePresentationSession(Widget);
	ResetPlaybackState(true);

	FPresentationStateSnapshot LatestBaseline;
	if (IsValid(Battle) && Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline))
	{
		CurrentBattleId = LatestBaseline.BattleId;
		const int64 BaselineResolutionWatermark = static_cast<int64>(Battle->GetLatestFrozenPresentationBaselineResolutionId());
		LastQueuedResolutionId = BaselineResolutionWatermark;
		LastCompletedResolutionId = BaselineResolutionWatermark;
		ApplyDisplayedSnapshot(LatestBaseline, false);
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
	InvalidatePresentationSession(Widget);
	CancelActiveTimeout();
	CancelActivePlaybackUnit();
	AdvancePlaybackGeneration();
	if (!IsValid(ViewModel))
	{
		ResetPlaybackState(false);
		return;
	}

	ViewModel->SetPresentationDisplayOwned(false);
	FPresentationStateSnapshot LatestBaseline;
	if (IsValid(Battle) && Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline))
	{
		CurrentBattleId = LatestBaseline.BattleId;
		const int64 BaselineResolutionWatermark = static_cast<int64>(Battle->GetLatestFrozenPresentationBaselineResolutionId());
		LastQueuedResolutionId = BaselineResolutionWatermark;
		LastCompletedResolutionId = BaselineResolutionWatermark;
		ApplyDisplayedSnapshot(LatestBaseline, false);
		MarkEntireBacklogCompletedExact(nullptr);
	}

	ResetPlaybackState(false);
	ViewModel->RefreshLiveInputBindingsIfCaughtUp();
}

bool UBattlePresentationController::ApplyRecordToWorkingSnapshot(const FPresentationRecord& Record)
{
	if (!bHasWorkingPresentationSnapshot)
	{
		return false;
	}

	switch (Record.Type)
	{
	case EBattlePresentationRecordType::Damage:
		return PresentationDamageReducer::TryApplyDamageRecord(
			WorkingPresentationSnapshot,
			Record.Damage);

	case EBattlePresentationRecordType::BlockChanged:
	{
		FBattleHUDCombatantView* Target = FindCombatantView(WorkingPresentationSnapshot, Record.BlockChanged.TargetPresentationId);
		if (Target == nullptr)
		{
			return false;
		}
		Target->Block = Record.BlockChanged.BlockAfter;
		return true;
	}

	case EBattlePresentationRecordType::CardPlayed:
	{
		const FPresentationCardSnapshot& Card = Record.CardPlayed.Card;
		if (Card.RuntimeId == INDEX_NONE || Card.CardId.IsNone()
			|| WorkingPresentationSnapshot.Energy != Record.CardPlayed.EnergyBefore)
		{
			return false;
		}
		const int32 HandIndex = FindHandCardIndexByRuntimeId(WorkingPresentationSnapshot.HandCards, Card.RuntimeId);
		if (HandIndex == INDEX_NONE
			|| HandIndex != Record.CardPlayed.HandIndexBefore
			|| WorkingPresentationSnapshot.HandCards[HandIndex].CardId != Card.CardId)
		{
			return false;
		}
		WorkingPresentationSnapshot.HandCards.RemoveAt(HandIndex);
		WorkingPresentationSnapshot.Energy = Record.CardPlayed.EnergyAfter;
		return true;
	}

	case EBattlePresentationRecordType::EnergyChanged:
		if (WorkingPresentationSnapshot.Energy != Record.EnergyChanged.EnergyBefore)
		{
			return false;
		}
		WorkingPresentationSnapshot.Energy = Record.EnergyChanged.EnergyAfter;
		return true;

	case EBattlePresentationRecordType::CardZoneChanged:
	{
		const FPresentationCardSnapshot& Card = Record.CardZoneChanged.Card;
		if (Card.RuntimeId == INDEX_NONE || Card.CardId.IsNone())
		{
			return false;
		}

		if (Record.CardZoneChanged.FromZone == ECardZone::DrawPile
			&& Record.CardZoneChanged.ToZone == ECardZone::Hand)
		{
			if (WorkingPresentationSnapshot.DrawCount <= 0
				|| FindHandCardIndexByRuntimeId(WorkingPresentationSnapshot.HandCards, Card.RuntimeId) != INDEX_NONE
				|| Record.CardZoneChanged.ToIndex < 0
				|| Record.CardZoneChanged.ToIndex > WorkingPresentationSnapshot.HandCards.Num())
			{
				return false;
			}
			--WorkingPresentationSnapshot.DrawCount;
			WorkingPresentationSnapshot.HandCards.Insert(
				PresentationCardView::MakePresentationOnlyCardView(Card),
				Record.CardZoneChanged.ToIndex);
			return true;
		}

		if (Record.CardZoneChanged.FromZone == ECardZone::Hand
			&& Record.CardZoneChanged.ToZone == ECardZone::DiscardPile)
		{
			const int32 HandIndex = FindHandCardIndexByRuntimeId(WorkingPresentationSnapshot.HandCards, Card.RuntimeId);
			if (HandIndex == INDEX_NONE
				|| HandIndex != Record.CardZoneChanged.FromIndex
				|| WorkingPresentationSnapshot.HandCards[HandIndex].CardId != Card.CardId)
			{
				return false;
			}
			WorkingPresentationSnapshot.HandCards.RemoveAt(HandIndex);
			++WorkingPresentationSnapshot.DiscardCount;
			return true;
		}

		if (Record.CardZoneChanged.FromZone == ECardZone::Hand
			&& Record.CardZoneChanged.ToZone == ECardZone::DrawPile)
		{
			const int32 HandIndex = FindHandCardIndexByRuntimeId(WorkingPresentationSnapshot.HandCards, Card.RuntimeId);
			if (HandIndex == INDEX_NONE
				|| HandIndex != Record.CardZoneChanged.FromIndex
				|| WorkingPresentationSnapshot.HandCards[HandIndex].CardId != Card.CardId
				|| Record.CardZoneChanged.ToIndex != WorkingPresentationSnapshot.DrawCount)
			{
				return false;
			}
			WorkingPresentationSnapshot.HandCards.RemoveAt(HandIndex);
			++WorkingPresentationSnapshot.DrawCount;
			return true;
		}

		if (Record.CardZoneChanged.FromZone == ECardZone::Hand
			&& Record.CardZoneChanged.ToZone == ECardZone::ExhaustPile)
		{
			const int32 HandIndex = FindHandCardIndexByRuntimeId(WorkingPresentationSnapshot.HandCards, Card.RuntimeId);
			if (HandIndex == INDEX_NONE
				|| HandIndex != Record.CardZoneChanged.FromIndex
				|| WorkingPresentationSnapshot.HandCards[HandIndex].CardId != Card.CardId
				|| Record.CardZoneChanged.ToIndex != WorkingPresentationSnapshot.ExhaustCount)
			{
				return false;
			}
			WorkingPresentationSnapshot.HandCards.RemoveAt(HandIndex);
			++WorkingPresentationSnapshot.ExhaustCount;
			return true;
		}

		if (Record.CardZoneChanged.FromZone == ECardZone::PlayArea)
		{
			switch (Record.CardZoneChanged.ToZone)
			{
			case ECardZone::DiscardPile:
				++WorkingPresentationSnapshot.DiscardCount;
				return true;
			case ECardZone::ExhaustPile:
				++WorkingPresentationSnapshot.ExhaustCount;
				return true;
			case ECardZone::RemovedPile:
				return true;
			default:
				return false;
			}
		}
		return false;
	}

	case EBattlePresentationRecordType::DeckShuffled:
		if (WorkingPresentationSnapshot.DrawCount != Record.DeckShuffled.DrawCountBefore
			|| WorkingPresentationSnapshot.DiscardCount != Record.DeckShuffled.DiscardCountBefore)
		{
			return false;
		}
		WorkingPresentationSnapshot.DrawCount = Record.DeckShuffled.DrawCountAfter;
		WorkingPresentationSnapshot.DiscardCount = Record.DeckShuffled.DiscardCountAfter;
		return true;

	case EBattlePresentationRecordType::StatusChanged:
		return ApplyStatusChangedRecord(WorkingPresentationSnapshot, Record.StatusChanged);

	case EBattlePresentationRecordType::ResolutionFault:
	case EBattlePresentationRecordType::Victory:
	case EBattlePresentationRecordType::Defeat:
		return ApplyTerminalRecord(WorkingPresentationSnapshot, ActiveEnvelope.FinalSnapshot, Record);

	case EBattlePresentationRecordType::None:
	default:
		return false;
	}
}

void UBattlePresentationController::ApplyDisplayedSnapshot(const FPresentationStateSnapshot& Snapshot, bool bRefreshBindings)
{
	DisplayedPresentationSnapshot = Snapshot;
	WorkingPresentationSnapshot = Snapshot;
	bHasDisplayedPresentationSnapshot = Snapshot.BattleId > 0;
	bHasWorkingPresentationSnapshot = bHasDisplayedPresentationSnapshot;
	if (IsValid(ViewModel))
	{
		ViewModel->ApplyPresentationSnapshot(Snapshot, true);
		if (bRefreshBindings)
		{
			ViewModel->RefreshLiveInputBindingsIfCaughtUp();
		}
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
		CancelActivePlaybackUnit();
		if (TimeoutToken.UnitKind == EPresentationPlaybackUnitKind::Group)
		{
			ReconcileActiveEnvelopeToFinalSnapshot();
		}
		else
		{
			CompleteActiveRecord();
		}
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

bool UBattlePresentationController::IsEnvelopeForCurrentBattle(const FPresentationResolutionEnvelope& Envelope) const
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

EPresentationPlaybackUnitKind UBattlePresentationController::GetActivePlaybackUnitKindForTesting() const
{
	return ActivePlaybackToken.UnitKind;
}

int64 UBattlePresentationController::GetLastCompletedResolutionIdForTesting() const
{
	return LastCompletedResolutionId;
}

void UBattlePresentationController::ExpireActivePlaybackForTesting()
{
	HandleActiveTimeout(0.0f);
}

void UBattlePresentationController::ReconcileActiveEnvelopeToFinalSnapshotForTesting()
{
	ReconcileActiveEnvelopeToFinalSnapshot();
}

bool UBattlePresentationController::RebindActivePlaybackAsGroupForTesting(
	const FPresentationGroupTag& Group,
	const TArray<int32>& RecordIndices
)
{
	if (!bHasActiveEnvelope
		|| !bWaitingForCompletion
		|| !IsValid(Widget)
		|| !Group.IsValid()
		|| Group.Kind != EPresentationGroupKind::SelectionDestination
		|| Group.ExpectedMemberCount <= 1
		|| RecordIndices.Num() != Group.ExpectedMemberCount)
	{
		return false;
	}

	TArray<FPresentationRecord> Records;
	Records.Reserve(RecordIndices.Num());
	int32 PreviousIndex = INDEX_NONE;
	for (const int32 RecordIndex : RecordIndices)
	{
		if (!ActiveEnvelope.Records.IsValidIndex(RecordIndex)
			|| (PreviousIndex != INDEX_NONE && RecordIndex <= PreviousIndex))
		{
			return false;
		}
		const FPresentationRecord& Record = ActiveEnvelope.Records[RecordIndex];
		if (Record.Group.Kind != Group.Kind
			|| Record.Group.GroupId != Group.GroupId
			|| Record.Group.ExpectedMemberCount != Group.ExpectedMemberCount)
		{
			return false;
		}
		Records.Add(Record);
		PreviousIndex = RecordIndex;
	}

	CancelActiveTimeout();
	CancelActivePlaybackUnit();
	AdvancePlaybackGeneration();

	const FPresentationRecord& Leader = Records[0];
	ActivePlaybackToken = FPresentationPlaybackToken{};
	ActivePlaybackToken.BattleId = Leader.BattleId;
	ActivePlaybackToken.ResolutionId = Leader.ResolutionId;
	ActivePlaybackToken.PresentationSequence = Leader.PresentationSequence;
	ActivePlaybackToken.LocalPlaybackGeneration = LocalPlaybackGeneration;
	ActivePlaybackToken.UnitKind = EPresentationPlaybackUnitKind::Group;
	ActivePlaybackToken.GroupId = Group.GroupId;
	bWaitingForCompletion = true;

	if (!Widget->PlayPresentationGroup(
		Records,
		Group,
		RecordIndices,
		ActivePlaybackToken))
	{
		bWaitingForCompletion = false;
		ActivePlaybackToken = FPresentationPlaybackToken{};
		return false;
	}
	ScheduleActiveTimeout();
	return true;
}

bool UBattlePresentationController::TryGetWorkingSnapshotForTesting(FPresentationStateSnapshot& OutSnapshot) const
{
	if (!bHasWorkingPresentationSnapshot)
	{
		return false;
	}
	OutSnapshot = WorkingPresentationSnapshot;
	return true;
}

bool UBattlePresentationController::ReduceEnvelopeForTesting(
	const FPresentationStateSnapshot& Baseline,
	const FPresentationResolutionEnvelope& Envelope,
	FPresentationStateSnapshot& OutReducedSnapshot
)
{
	const FPresentationStateSnapshot SavedWorking = WorkingPresentationSnapshot;
	const FPresentationResolutionEnvelope SavedEnvelope = ActiveEnvelope;
	const bool bSavedHasWorking = bHasWorkingPresentationSnapshot;

	WorkingPresentationSnapshot = Baseline;
	ActiveEnvelope = Envelope;
	bHasWorkingPresentationSnapshot = true;

	bool bSucceeded = true;
	for (const FPresentationRecord& Record : Envelope.Records)
	{
		if (!ApplyRecordToWorkingSnapshot(Record))
		{
			bSucceeded = false;
			break;
		}
	}
	if (bSucceeded)
	{
		OutReducedSnapshot = WorkingPresentationSnapshot;
	}

	WorkingPresentationSnapshot = SavedWorking;
	ActiveEnvelope = SavedEnvelope;
	bHasWorkingPresentationSnapshot = bSavedHasWorking;
	return bSucceeded;
}
#endif
