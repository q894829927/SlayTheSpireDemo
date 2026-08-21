#include "BattleManager.h"

#include "BattleReadSnapshot.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Enemy/EnemyIntent.h"
#include "../Status/StatusData.h"

namespace
{
	FText PresentationFailureReasonToText(EGameplayRequestFailureReason Reason)
	{
		switch (Reason)
		{
		case EGameplayRequestFailureReason::None:
			return FText::GetEmpty();
		case EGameplayRequestFailureReason::InvalidBattle:
			return FText::FromString(TEXT("Battle is not ready."));
		case EGameplayRequestFailureReason::BattleEnded:
			return FText::FromString(TEXT("The battle has already ended."));
		case EGameplayRequestFailureReason::ResolutionFaulted:
			return FText::FromString(TEXT("Battle resolution stopped safely."));
		case EGameplayRequestFailureReason::WrongTurn:
			return FText::FromString(TEXT("It is not the player's turn."));
		case EGameplayRequestFailureReason::ResolutionBusy:
			return FText::FromString(TEXT("Battle resolution is still in progress."));
		case EGameplayRequestFailureReason::InvalidCard:
			return FText::FromString(TEXT("That card is invalid."));
		case EGameplayRequestFailureReason::CardNoLongerInHand:
			return FText::FromString(TEXT("That card is no longer in hand."));
		case EGameplayRequestFailureReason::NotEnoughEnergy:
			return FText::FromString(TEXT("Not enough Energy."));
		case EGameplayRequestFailureReason::InvalidTarget:
			return FText::FromString(TEXT("Choose a legal target."));
		case EGameplayRequestFailureReason::QueueRejected:
			return FText::FromString(TEXT("The battle could not accept that action."));
		default:
			return FText::FromString(TEXT("Action rejected."));
		}
	}
}

bool ABattleManager::TryResolveCombatantPresentationId(
	const ACombatant* Combatant,
	FName& OutPresentationId
) const
{
	OutPresentationId = NAME_None;
	if (!IsValid(Combatant))
	{
		return false;
	}

	if (Combatant != Player.Get() && Combatant != Enemy.Get())
	{
		return false;
	}

	if (!Combatant->PresentationId.IsNone())
	{
		OutPresentationId = Combatant->PresentationId;
	}
	else if (Combatant == Player.Get())
	{
		OutPresentationId = TEXT("Player");
	}
	else if (Combatant == Enemy.Get())
	{
		OutPresentationId = TEXT("EnemyPrimary");
	}

	return !OutPresentationId.IsNone();
}

bool ABattleManager::ValidateResolvedPresentationIds(FString& OutReason) const
{
	OutReason.Reset();
	FName PlayerId = NAME_None;
	FName EnemyId = NAME_None;
	if (!TryResolveCombatantPresentationId(Player.Get(), PlayerId)
		|| !TryResolveCombatantPresentationId(Enemy.Get(), EnemyId))
	{
		OutReason = TEXT("A current battle participant does not resolve to a PresentationId.");
		return false;
	}

	if (PlayerId.IsNone() || EnemyId.IsNone())
	{
		OutReason = TEXT("A resolved battle PresentationId is empty.");
		return false;
	}

	if (PlayerId == EnemyId)
	{
		OutReason = FString::Printf(
			TEXT("Resolved battle PresentationIds are not unique: %s."),
			*PlayerId.ToString()
		);
		return false;
	}

	return true;
}

bool ABattleManager::TryFreezePresentationStateSnapshot(
	const FBattleReadSnapshot& ReadSnapshot,
	FPresentationStateSnapshot& OutSnapshot
) const
{
	OutSnapshot = FPresentationStateSnapshot{};

#if WITH_DEV_AUTOMATION_TESTS
	if (bForcePresentationFreezeFailureForTesting)
	{
		return false;
	}
#endif

	if (ReadSnapshot.BattleId == 0 || ReadSnapshot.StateRevision == 0
		|| ReadSnapshot.BattleId != BattleId
		|| ReadSnapshot.StateRevision != StateRevision)
	{
		return false;
	}

	FString PresentationIdFailure;
	if (!ValidateResolvedPresentationIds(PresentationIdFailure))
	{
		return false;
	}

	OutSnapshot.BattleId = static_cast<int64>(ReadSnapshot.BattleId);
	OutSnapshot.StateRevision = static_cast<int64>(ReadSnapshot.StateRevision);
	OutSnapshot.BattleState = ReadSnapshot.BattleState;
	OutSnapshot.Energy = ReadSnapshot.Energy;
	OutSnapshot.MaxEnergy = ReadSnapshot.MaxEnergy;
	OutSnapshot.DrawCount = ReadSnapshot.DrawCount;
	OutSnapshot.DiscardCount = ReadSnapshot.DiscardCount;
	OutSnapshot.ExhaustCount = ReadSnapshot.ExhaustCount;
	OutSnapshot.bCanEndTurn = ReadSnapshot.BattleState == EBattleState::PlayerTurn
		&& QueryEndPlayerTurn().bAllowed;

	switch (ReadSnapshot.BattleState)
	{
	case EBattleState::Victory:
		OutSnapshot.Outcome = EBattleHUDOutcome::Victory;
		break;
	case EBattleState::Defeat:
		OutSnapshot.Outcome = EBattleHUDOutcome::Defeat;
		break;
	case EBattleState::ResolutionFaulted:
		OutSnapshot.Outcome = EBattleHUDOutcome::ResolutionFaulted;
		break;
	default:
		OutSnapshot.Outcome = EBattleHUDOutcome::None;
		break;
	}

	const auto FreezeCombatant = [this](
		const FCombatantReadView& Source,
		bool bPlayer,
		FBattleHUDCombatantView& OutView
	) -> bool
	{
		OutView = FBattleHUDCombatantView{};
		ACombatant* Combatant = Source.Combatant.Get();
		if (!IsValid(Combatant)
			|| !TryResolveCombatantPresentationId(Combatant, OutView.PresentationId))
		{
			return false;
		}

		OutView.bPlayer = bPlayer;
		OutView.DisplayName = Source.DisplayName.IsEmpty()
			? FText::FromName(OutView.PresentationId)
			: Source.DisplayName;
		OutView.HP = Source.HP;
		OutView.MaxHP = Source.MaxHP;
		OutView.Block = Source.Block;
		OutView.bDead = Source.bDead;
		OutView.Statuses.Reserve(Source.Statuses.Num());

		for (const FStatusReadView& Status : Source.Statuses)
		{
			FBattleHUDStatusView FrozenStatus;
			FrozenStatus.StatusId = Status.StatusId;
			FrozenStatus.Amount = Status.Amount;
			FrozenStatus.Description = Status.CurrentDescription;

			if (const UStatusData* Definition = Status.Definition.Get())
			{
				FrozenStatus.DisplayName = Definition->DisplayName.IsEmpty()
					? FText::FromName(Status.StatusId)
					: Definition->DisplayName;
				FrozenStatus.bUseAtlasIcon = Definition->IconRegion.bUseAtlasIcon;
				FrozenStatus.UVOffset = Definition->IconRegion.UVOffset;
				FrozenStatus.UVScale = Definition->IconRegion.UVScale;
				FrozenStatus.TrimOffset = Definition->IconRegion.TrimOffset;
				FrozenStatus.TrimScale = Definition->IconRegion.TrimScale;
			}
			else
			{
				FrozenStatus.DisplayName = FText::FromName(Status.StatusId);
			}

			OutView.Statuses.Add(MoveTemp(FrozenStatus));
		}

		return true;
	};

	if (!FreezeCombatant(ReadSnapshot.Player, true, OutSnapshot.Player)
		|| !FreezeCombatant(ReadSnapshot.Enemy, false, OutSnapshot.Enemy))
	{
		return false;
	}

	OutSnapshot.HandCards.Reserve(ReadSnapshot.HandCards.Num());
	for (const FCardReadView& Source : ReadSnapshot.HandCards)
	{
		FBattleHUDCardView FrozenCard;
		FrozenCard.RuntimeId = Source.RuntimeId;
		FrozenCard.CardId = Source.CardId;
		FrozenCard.Cost = Source.CurrentCost;
		FrozenCard.TargetType = Source.TargetType;
		FrozenCard.Description = Source.CurrentDescription;

		UCardInstance* Card = Source.Card.Get();
		const UCardData* Definition = IsValid(Card) ? Card->GetDefinition() : nullptr;
		if (IsValid(Definition))
		{
			FrozenCard.DisplayName = Definition->DisplayName.IsEmpty()
				? FText::FromName(Source.CardId)
				: Definition->DisplayName;
			FrozenCard.CardType = Definition->CardType;
			FrozenCard.CardArt = Definition->CardArt;
		}
		else
		{
			FrozenCard.DisplayName = FText::FromName(Source.CardId);
		}

		const FGameplayValidationResult Validation = QueryCardPlayability(Card);
		FrozenCard.bGameplayPlayable = Validation.bAllowed;
		if (!Validation.bAllowed)
		{
			FrozenCard.UnplayableReason = PresentationFailureReasonToText(Validation.FailureReason);
		}

		OutSnapshot.HandCards.Add(MoveTemp(FrozenCard));
	}

	OutSnapshot.EnemyIntent = FBattleHUDIntentView{};
	OutSnapshot.EnemyIntent.BaseAmount = ReadSnapshot.EnemyIntent.BaseAmount;
	OutSnapshot.EnemyIntent.bHasCurrentResolvedDamageAmount =
		ReadSnapshot.EnemyIntentPlayerFacing.bHasCurrentResolvedDamageAmount;
	OutSnapshot.EnemyIntent.CurrentResolvedDamageAmount =
		ReadSnapshot.EnemyIntentPlayerFacing.CurrentResolvedDamageAmount;
	if (ReadSnapshot.EnemyIntent.Type == EEnemyIntentType::Attack)
	{
		OutSnapshot.EnemyIntent.Type = EBattleHUDIntentType::Attack;
		OutSnapshot.EnemyIntent.DisplayName = FText::FromString(TEXT("Attack"));
	}

	return true;
}

bool ABattleManager::TryGetLatestFrozenPresentationBaseline(
	FPresentationStateSnapshot& OutSnapshot
) const
{
	OutSnapshot = FPresentationStateSnapshot{};
	if (!bHasLatestFrozenPresentationBaseline)
	{
		return false;
	}

	OutSnapshot = LatestFrozenPresentationBaseline;
	return true;
}

bool ABattleManager::IsPresentationAvailable() const
{
	return bPresentationAvailable;
}

FText ABattleManager::GetPresentationUnavailableReason() const
{
	return PresentationUnavailableReason;
}

void ABattleManager::ResetPresentationForBattle()
{
	if (ReadStateReadyTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReadStateReadyTickerHandle);
		ReadStateReadyTickerHandle.Reset();
	}
	bReadStateReadyPublishScheduled = false;

	PendingPublicDeliveryQueue.Reset();
	LatestFrozenPresentationBaseline = FPresentationStateSnapshot{};
	bHasLatestFrozenPresentationBaseline = false;
	bPresentationAvailable = true;
	PresentationUnavailableReason = FText::GetEmpty();
	LastSealedPresentationResolutionId = 0;
	LastDeliveredPresentationResolutionId = 0;

	PresentationRecorder = NewObject<UBattlePresentationRecorder>(this);
	if (!IsValid(PresentationRecorder))
	{
		MarkPresentationUnavailable(TEXT("Could not create the battle-scoped Presentation Recorder."));
		return;
	}

	PresentationRecorder->ResetForBattle(BattleId);

	FString PresentationIdFailure;
	if (!ValidateResolvedPresentationIds(PresentationIdFailure))
	{
		MarkPresentationUnavailable(PresentationIdFailure);
	}
}

bool ABattleManager::BeginPresentationResolution(EPresentationResolutionOrigin Origin)
{
	if (!bEnableCommittedPresentationRecording || !bPresentationAvailable)
	{
		return false;
	}

	if (!IsValid(PresentationRecorder))
	{
		MarkPresentationUnavailable(TEXT("Presentation Recorder is unavailable at Resolution begin."));
		return false;
	}

	FPresentationRecordWriter Writer;
	if (!PresentationRecorder->BeginResolution(Origin, Writer))
	{
		MarkPresentationUnavailable(TEXT("Presentation Resolution begin failed or another builder is still active."));
		return false;
	}

	return true;
}

void ABattleManager::AbortPresentationResolution()
{
	if (IsValid(PresentationRecorder))
	{
		PresentationRecorder->AbortResolution();
	}
}

FPresentationRecordWriter ABattleManager::GetActivePresentationRecordWriter() const
{
	FPresentationRecordWriter Writer;
	if (IsValid(PresentationRecorder))
	{
		PresentationRecorder->TryGetActiveWriter(Writer);
	}
	return Writer;
}

void ABattleManager::AppendPresentationResolutionFault(
	const FString& Reason,
	int32 ExecutedCount,
	const UBattleAction* LastAction
)
{
	const FPresentationRecordWriter Writer = GetActivePresentationRecordWriter();
	if (!Writer.IsAvailable())
	{
		return;
	}

	FPresentationRecord FaultRecord;
	FaultRecord.Type = EBattlePresentationRecordType::ResolutionFault;
	FaultRecord.FaultReason = Reason;
	FaultRecord.FaultExecutedActionCount = ExecutedCount;
	FaultRecord.FaultLastActionName = IsValid(LastAction) ? LastAction->GetFName() : NAME_None;
	if (!Writer.Append(MoveTemp(FaultRecord)))
	{
		bPresentationAvailable = false;
		PresentationUnavailableReason = FText::FromString(
			TEXT("Presentation record append failed; historical playback was disabled for this battle.")
		);
	}
}

void ABattleManager::FinalizePresentationResolutionAtStableBoundary()
{
	FBattleReadSnapshot ReadSnapshot;
	if (!TryBuildPlayerFacingReadSnapshot(ReadSnapshot))
	{
		AbortPresentationResolution();
		MarkPresentationUnavailable(TEXT("Could not build the exact stable read state required to freeze Presentation."));
		return;
	}

	FPresentationStateSnapshot FrozenSnapshot;
	if (!TryFreezePresentationStateSnapshot(ReadSnapshot, FrozenSnapshot))
	{
		AbortPresentationResolution();
		MarkPresentationUnavailable(TEXT("Could not freeze the exact player-facing Presentation snapshot."));
		return;
	}

	LatestFrozenPresentationBaseline = FrozenSnapshot;
	bHasLatestFrozenPresentationBaseline = true;

	if (!IsValid(PresentationRecorder) || !PresentationRecorder->HasActiveResolution())
	{
		return;
	}

	if (!bPresentationAvailable || !PresentationRecorder->IsActiveResolutionValid())
	{
		PresentationRecorder->AbortResolution();
		if (bPresentationAvailable)
		{
			MarkPresentationUnavailable(TEXT("Presentation Record append failed; the whole unpublished Resolution history was discarded."));
		}
		return;
	}

	FPresentationResolutionEnvelope Envelope;
	if (!PresentationRecorder->SealResolution(FrozenSnapshot, Envelope))
	{
		MarkPresentationUnavailable(TEXT("Presentation Resolution seal failed; historical playback was disabled for this battle."));
		return;
	}

	if (Envelope.BattleId != static_cast<int64>(BattleId)
		|| Envelope.ResolutionId <= 0
		|| static_cast<uint64>(Envelope.ResolutionId) <= LastSealedPresentationResolutionId)
	{
		MarkPresentationUnavailable(TEXT("Presentation Resolution seal identity was invalid or duplicated."));
		return;
	}

	LastSealedPresentationResolutionId = static_cast<uint64>(Envelope.ResolutionId);
	EnqueuePendingPublicPresentation(MoveTemp(Envelope));
}

void ABattleManager::FreezeLatestPresentationBaselineWithoutResolution()
{
	FBattleReadSnapshot ReadSnapshot;
	if (!TryBuildPlayerFacingReadSnapshot(ReadSnapshot))
	{
		return;
	}

	FPresentationStateSnapshot FrozenSnapshot;
	if (!TryFreezePresentationStateSnapshot(ReadSnapshot, FrozenSnapshot))
	{
		MarkPresentationUnavailable(TEXT("Could not freeze the latest player-facing Presentation baseline."));
		return;
	}

	LatestFrozenPresentationBaseline = FrozenSnapshot;
	bHasLatestFrozenPresentationBaseline = true;
}

void ABattleManager::MarkPresentationUnavailable(const FString& Reason)
{
	bPresentationAvailable = false;
	PresentationUnavailableReason = FText::FromString(
		Reason.IsEmpty()
			? TEXT("Committed Presentation is unavailable for this battle.")
			: Reason
	);
	PendingPublicDeliveryQueue.Reset();

	UE_LOG(
		LogTemp,
		Error,
		TEXT("[Presentation] Unavailable for BattleId=%llu: %s"),
		BattleId,
		*PresentationUnavailableReason.ToString()
	);
}

void ABattleManager::EnqueuePendingPublicPresentation(FPresentationResolutionEnvelope&& Envelope)
{
	if (Envelope.BattleId != static_cast<int64>(BattleId))
	{
		return;
	}

	if (PendingPublicDeliveryQueue.Num() >= MaxPendingPublicPresentationEnvelopes)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Presentation] Pending public-delivery FIFO overflow. Collapsing to newest frozen Envelope. BattleId=%llu ResolutionId=%lld"),
			BattleId,
			Envelope.ResolutionId
		);
		PendingPublicDeliveryQueue.Reset();
	}

	PendingPublicDeliveryQueue.Add(MoveTemp(Envelope));
}

void ABattleManager::DrainPendingPublicPresentationDeliveries()
{
	if (PendingPublicDeliveryQueue.Num() == 0)
	{
		return;
	}

	if (!bPresentationAvailable || !bEnableCommittedPresentationRecording)
	{
		PendingPublicDeliveryQueue.Reset();
		return;
	}

	TArray<FPresentationResolutionEnvelope> Deliveries = MoveTemp(PendingPublicDeliveryQueue);
	PendingPublicDeliveryQueue.Reset();

	for (const FPresentationResolutionEnvelope& Envelope : Deliveries)
	{
		if (Envelope.BattleId != static_cast<int64>(BattleId)
			|| Envelope.ResolutionId <= 0
			|| static_cast<uint64>(Envelope.ResolutionId) <= LastDeliveredPresentationResolutionId)
		{
			continue;
		}

		LastDeliveredPresentationResolutionId = static_cast<uint64>(Envelope.ResolutionId);
		OnPresentationResolutionReady.Broadcast(Envelope);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
UBattlePresentationRecorder* ABattleManager::GetPresentationRecorderForTesting() const
{
	return PresentationRecorder.Get();
}

FPresentationRecordWriter ABattleManager::GetActivePresentationRecordWriterForTesting() const
{
	return GetActivePresentationRecordWriter();
}

bool ABattleManager::BeginSystemPresentationResolutionForTesting()
{
	return BeginPresentationResolution(EPresentationResolutionOrigin::System);
}

bool ABattleManager::SealActivePresentationResolutionForTesting()
{
	const uint64 Before = LastSealedPresentationResolutionId;
	FinalizePresentationResolutionAtStableBoundary();
	ScheduleReadStateReadyPublish();
	return LastSealedPresentationResolutionId > Before;
}

int32 ABattleManager::GetPendingPresentationDeliveryCountForTesting() const
{
	return PendingPublicDeliveryQueue.Num();
}

uint64 ABattleManager::GetLastSealedPresentationResolutionIdForTesting() const
{
	return LastSealedPresentationResolutionId;
}

void ABattleManager::SetForcePresentationFreezeFailureForTesting(bool bForce)
{
	bForcePresentationFreezeFailureForTesting = bForce;
}
#endif
