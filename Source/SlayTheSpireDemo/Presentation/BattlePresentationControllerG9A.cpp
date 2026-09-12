#include "BattlePresentationController.h"

#include "../Battle/BattleManager.h"
#include "../UI/BattleBufferedPlayerIntent.h"
#include "../UI/BattleHUDViewModel.h"

namespace
{
	const FBattleHUDCardView* FindCardViewByRuntimeId(
		const TArray<FBattleHUDCardView>& Cards,
		int32 RuntimeId)
	{
		if (RuntimeId == INDEX_NONE)
		{
			return nullptr;
		}

		for (const FBattleHUDCardView& Card : Cards)
		{
			if (Card.RuntimeId == RuntimeId)
			{
				return &Card;
			}
		}
		return nullptr;
	}

	bool IsApprovedG9CardBufferWindow(const FPresentationRecord& Record)
	{
		if (Record.Type == EBattlePresentationRecordType::CardPlayed)
		{
			return true;
		}

		if (Record.Type != EBattlePresentationRecordType::CardZoneChanged
			|| Record.CardZoneChanged.FromZone != ECardZone::PlayArea)
		{
			return false;
		}

		switch (Record.CardZoneChanged.ToZone)
		{
		case ECardZone::DiscardPile:
		case ECardZone::ExhaustPile:
		case ECardZone::RemovedPile:
			return true;
		default:
			return false;
		}
	}

	bool IsExactNormalPlayerCardTarget(
		const FPresentationStateSnapshot& Snapshot,
		int32 RequestedRuntimeId,
		const FBattleHUDCardView*& OutTargetCard)
	{
		OutTargetCard = nullptr;
		if (Snapshot.BattleId <= 0
			|| Snapshot.StateRevision <= 0
			|| Snapshot.BattleState != EBattleState::PlayerTurn
			|| Snapshot.Outcome != EBattleHUDOutcome::None)
		{
			return false;
		}

		OutTargetCard = FindCardViewByRuntimeId(Snapshot.HandCards, RequestedRuntimeId);
		return OutTargetCard != nullptr && OutTargetCard->bGameplayPlayable;
	}
}

bool UBattlePresentationController::TryCaptureBufferedCardTarget(
	int32 RequestedRuntimeId,
	FBufferedCardIntent& OutIntent) const
{
	OutIntent = FBufferedCardIntent{};

	if (RequestedRuntimeId == INDEX_NONE
		|| !IsPresentationOwnedMode()
		|| !ActivePresentationSessionToken.IsValid()
		|| !bHasActiveEnvelope
		|| !bWaitingForCompletion
		|| !ActivePlaybackToken.IsValid()
		|| ActivePlaybackToken.UnitKind != EPresentationPlaybackUnitKind::SingleRecord
		|| !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex))
	{
		return false;
	}

	const FPresentationRecord& ActiveRecord = ActiveEnvelope.Records[ActiveRecordIndex];
	if (!IsApprovedG9CardBufferWindow(ActiveRecord)
		|| ActiveRecord.BattleId != CurrentBattleId
		|| ActiveRecord.BattleId != ActivePlaybackToken.BattleId
		|| ActiveRecord.ResolutionId != ActivePlaybackToken.ResolutionId
		|| ActiveRecord.PresentationSequence != ActivePlaybackToken.PresentationSequence
		|| ActivePlaybackToken.LocalPlaybackGeneration <= 0)
	{
		return false;
	}

	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle)
		|| !IsValid(ViewModel)
		|| !Battle->IsPresentationAvailable()
		|| !Battle->IsCommittedPresentationRecordingEnabledForBattle()
		|| ViewModel->HasAuthoritativePendingCardSelection())
	{
		return false;
	}

	FPresentationStateSnapshot LatestTarget;
	const FBattleHUDCardView* TargetCard = nullptr;
	if (!Battle->TryGetLatestFrozenPresentationBaseline(LatestTarget)
		|| LatestTarget.BattleId != CurrentBattleId
		|| LatestTarget.BattleId != ActivePresentationSessionToken.BattleId
		|| !IsExactNormalPlayerCardTarget(LatestTarget, RequestedRuntimeId, TargetCard))
	{
		return false;
	}

	// G9 card buffering is Presentation-lag only. The requested card must already
	// be a visible survivor in the displayed Hand, while the exact target surface
	// is a different sealed revision that Presentation has not displayed yet.
	const FBattleHUDCardView* DisplayedCard =
		FindCardViewByRuntimeId(ViewModel->HandCards, RequestedRuntimeId);
	if (ViewModel->BattleId != LatestTarget.BattleId
		|| ViewModel->StateRevision == LatestTarget.StateRevision
		|| DisplayedCard == nullptr
		|| DisplayedCard->CardId.IsNone()
		|| DisplayedCard->CardId != TargetCard->CardId)
	{
		return false;
	}

	OutIntent.SessionToken = ActivePresentationSessionToken;
	OutIntent.BattleId = LatestTarget.BattleId;
	OutIntent.ExpectedReadyRevision = LatestTarget.StateRevision;
	OutIntent.CaptureWindow.SourceResolutionId = ActiveRecord.ResolutionId;
	OutIntent.CaptureWindow.SourcePresentationSequence = ActiveRecord.PresentationSequence;
	OutIntent.CaptureWindow.LocalWindowGeneration =
		static_cast<uint64>(ActivePlaybackToken.LocalPlaybackGeneration);
	OutIntent.RuntimeId = RequestedRuntimeId;
	return OutIntent.IsValid();
}

EBufferedIntentShadowEvaluation UBattlePresentationController::EvaluateBufferedCardTarget(
	const FBufferedCardIntent& Intent) const
{
	if (!Intent.IsValid()
		|| !IsCurrentPresentationSession(Intent.SessionToken)
		|| Intent.BattleId != CurrentBattleId)
	{
		return EBufferedIntentShadowEvaluation::Stale;
	}

	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle)
		|| !IsValid(ViewModel)
		|| !Battle->IsPresentationAvailable()
		|| !Battle->IsCommittedPresentationRecordingEnabledForBattle()
		|| ViewModel->HasAuthoritativePendingCardSelection())
	{
		return EBufferedIntentShadowEvaluation::Stale;
	}

	FPresentationStateSnapshot LatestTarget;
	const FBattleHUDCardView* TargetCard = nullptr;
	if (!Battle->TryGetLatestFrozenPresentationBaseline(LatestTarget)
		|| LatestTarget.BattleId != Intent.BattleId
		|| LatestTarget.StateRevision != Intent.ExpectedReadyRevision
		|| !IsExactNormalPlayerCardTarget(LatestTarget, Intent.RuntimeId, TargetCard))
	{
		return EBufferedIntentShadowEvaluation::Stale;
	}

	if (ViewModel->BattleId != Intent.BattleId)
	{
		return EBufferedIntentShadowEvaluation::Stale;
	}

	// Revision identity is exact. A mismatch may wait only while authoritative
	// Presentation chronology is still in flight toward this already-sealed target;
	// numeric revision ordering never grants broader authority.
	if (ViewModel->StateRevision != Intent.ExpectedReadyRevision)
	{
		return HasSkippablePresentationDelay()
			? EBufferedIntentShadowEvaluation::Waiting
			: EBufferedIntentShadowEvaluation::Stale;
	}

	const FBattleHUDCardView* DisplayedCard =
		FindCardViewByRuntimeId(ViewModel->HandCards, Intent.RuntimeId);
	if (DisplayedCard == nullptr
		|| DisplayedCard->CardId.IsNone()
		|| DisplayedCard->CardId != TargetCard->CardId
		|| !DisplayedCard->bGameplayPlayable
		|| ViewModel->Outcome != EBattleHUDOutcome::None)
	{
		return EBufferedIntentShadowEvaluation::Stale;
	}

	if (!ViewModel->bInputLocked
		&& ViewModel->InteractionState == EBattleHUDInteractionState::Idle)
	{
		return EBufferedIntentShadowEvaluation::Ready;
	}

	return HasSkippablePresentationDelay()
		? EBufferedIntentShadowEvaluation::Waiting
		: EBufferedIntentShadowEvaluation::Stale;
}
