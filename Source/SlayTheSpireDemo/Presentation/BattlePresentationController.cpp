#include "BattlePresentationController.h"

#include "../Battle/BattleManager.h"
#include "../UI/BattleHUDViewModel.h"
#include "../UI/BattleHUDWidgetBase.h"

namespace
{
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

	FBattleHUDCardView MakeHUDCardView(const FPresentationCardSnapshot& Card)
	{
		FBattleHUDCardView View;
		View.RuntimeId = Card.RuntimeId;
		View.CardId = Card.CardId;
		View.DisplayName = Card.DisplayName;
		View.Cost = Card.Cost;
		View.CardType = Card.CardType;
		View.TargetType = Card.TargetType;
		View.Description = Card.Description;
		View.CardArt = Card.CardArt;
		View.bGameplayPlayable = false;
		View.UnplayableReason = FText::GetEmpty();
		return View;
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

	// Controller ownership is explicit at bootstrap. This prevents a stale or
	// independently initialized ViewModel from accepting live read-state updates
	// between baseline catch-up and the first committed Envelope.
	if (InBattleManager->IsPresentationAvailable()
		&& InBattleManager->IsCommittedPresentationRecordingEnabledForBattle())
	{
		InViewModel->SetPresentationDisplayOwned(true);
	}

	FPresentationStateSnapshot Baseline;
	if (InBattleManager->TryGetLatestFrozenPresentationBaseline(Baseline))
	{
		CurrentBattleId = Baseline.BattleId;
		// Apply the authoritative baseline before advancing watermarks. This is
		// intentionally idempotent and repairs a stale/rebuilt ViewModel.
		ApplyDisplayedSnapshot(Baseline, false);

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

	if (Newest)
	{
		ApplyDisplayedSnapshot(Newest->FinalSnapshot, false);
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
		ApplyDisplayedSnapshot(LatestBaseline, false);
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

	if (!bHasDisplayedPresentationSnapshot
		|| DisplayedPresentationSnapshot.BattleId != ActiveEnvelope.BattleId)
	{
		const FPresentationResolutionEnvelope FallbackEnvelope = ActiveEnvelope;
		CollapseToEnvelope(FallbackEnvelope);
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
		const FPresentationResolutionEnvelope FallbackEnvelope = ActiveEnvelope;
		CollapseToEnvelope(FallbackEnvelope);
		return;
	}

	const bool bRecordSupportsVisiblePlayback =
		Record.Type == EBattlePresentationRecordType::ResolutionFault
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

	// Status history must be trustworthy before Blueprint sees it. Preflight on
	// a copy so valid records can animate against the old displayed state; the
	// real WorkingPresentationSnapshot is still committed only after completion.
	if (Record.Type == EBattlePresentationRecordType::StatusChanged)
	{
		if (!bHasWorkingPresentationSnapshot)
		{
			const FPresentationResolutionEnvelope FallbackEnvelope = ActiveEnvelope;
			CollapseToEnvelope(FallbackEnvelope);
			return;
		}

		FPresentationStateSnapshot PreflightSnapshot = WorkingPresentationSnapshot;
		if (!ApplyStatusChangedRecord(PreflightSnapshot, Record.StatusChanged))
		{
			const FPresentationResolutionEnvelope FallbackEnvelope = ActiveEnvelope;
			CollapseToEnvelope(FallbackEnvelope);
			return;
		}
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
	if (!bHasActiveEnvelope || !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex))
	{
		return;
	}

	bWaitingForCompletion = false;
	ActivePlaybackToken = FPresentationPlaybackToken{};

	const FPresentationRecord& CompletedRecord = ActiveEnvelope.Records[ActiveRecordIndex];
	if (!ApplyRecordToWorkingSnapshot(CompletedRecord))
	{
		const FPresentationResolutionEnvelope FallbackEnvelope = ActiveEnvelope;
		CollapseToEnvelope(FallbackEnvelope);
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

	const FPresentationStateSnapshot FinalSnapshot = Envelope.FinalSnapshot;
	const int64 ResolutionId = Envelope.ResolutionId;
	ResetPlaybackState(true);
	ApplyDisplayedSnapshot(FinalSnapshot, true);
	LastCompletedResolutionId = FMath::Max(LastCompletedResolutionId, ResolutionId);
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
	WorkingPresentationSnapshot = FPresentationStateSnapshot{};
	bHasActiveEnvelope = false;
	bHasWorkingPresentationSnapshot = false;
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
		ApplyDisplayedSnapshot(LatestBaseline, true);
	}
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
	{
		FBattleHUDCombatantView* Target = FindCombatantView(
			WorkingPresentationSnapshot,
			Record.Damage.TargetPresentationId
		);
		if (Target == nullptr)
		{
			return false;
		}
		Target->HP = Record.Damage.HPAfter;
		Target->Block = Record.Damage.BlockAfter;
		Target->bDead = Target->HP <= 0;
		return true;
	}

	case EBattlePresentationRecordType::BlockChanged:
	{
		FBattleHUDCombatantView* Target = FindCombatantView(
			WorkingPresentationSnapshot,
			Record.BlockChanged.TargetPresentationId
		);
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
		const int32 HandIndex = FindHandCardIndexByRuntimeId(
			WorkingPresentationSnapshot.HandCards,
			Card.RuntimeId
		);
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
				MakeHUDCardView(Card),
				Record.CardZoneChanged.ToIndex
			);
			return true;
		}

		if (Record.CardZoneChanged.FromZone == ECardZone::Hand
			&& Record.CardZoneChanged.ToZone == ECardZone::DiscardPile)
		{
			const int32 HandIndex = FindHandCardIndexByRuntimeId(
				WorkingPresentationSnapshot.HandCards,
				Card.RuntimeId
			);
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

	case EBattlePresentationRecordType::None:
		return false;

	case EBattlePresentationRecordType::ResolutionFault:
	case EBattlePresentationRecordType::Victory:
	case EBattlePresentationRecordType::Defeat:
	default:
		return true;
	}
}

void UBattlePresentationController::ApplyDisplayedSnapshot(
	const FPresentationStateSnapshot& Snapshot,
	bool bRefreshBindings
)
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
