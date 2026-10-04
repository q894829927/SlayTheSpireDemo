#include "BattlePresentationController.h"

#include "../Battle/BattleManager.h"
#include "../Battle/BattleReadSnapshot.h"
#include "../Cards/CardInstance.h"
#include "../UI/BattleHUDViewModel.h"

void UBattlePresentationController::InvalidateBufferedCardChronology()
{
	if (++BufferedCardChronologyGeneration == 0)
	{
		++BufferedCardChronologyGeneration;
	}
}

bool UBattlePresentationController::TryCaptureBufferedCardTarget(
	int32 RuntimeId, FBufferedCardIntent& OutIntent) const
{
	OutIntent = FBufferedCardIntent{};
	if (!IsPresentationOwnedMode() || !ActivePresentationSessionToken.IsValid()
		|| !bHasActiveEnvelope || !bWaitingForCompletion
		|| !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex)
		|| ActivePlaybackToken.UnitKind != EPresentationPlaybackUnitKind::SingleRecord
		|| ViewModel->InteractionState != EBattleHUDInteractionState::Resolving
		|| ViewModel->GetCardPresentationOwner(RuntimeId) != ECardPresentationOwner::Hand
		|| !ViewModel->HandCards.ContainsByPredicate([RuntimeId](const FBattleHUDCardView& Card)
			{ return Card.RuntimeId == RuntimeId && Card.bGameplayPlayable; }))
	{
		return false;
	}

	const FPresentationRecord& Record = ActiveEnvelope.Records[ActiveRecordIndex];
	const bool bPlayedWindow = Record.Type == EBattlePresentationRecordType::CardPlayed;
	const bool bDestinationWindow = Record.Type == EBattlePresentationRecordType::CardZoneChanged
		&& Record.CardZoneChanged.FromZone == ECardZone::PlayArea
		&& (Record.CardZoneChanged.ToZone == ECardZone::DiscardPile
			|| Record.CardZoneChanged.ToZone == ECardZone::ExhaustPile
			|| Record.CardZoneChanged.ToZone == ECardZone::RemovedPile);
	if ((!bPlayedWindow && !bDestinationWindow)
		|| ActivePlaybackToken.BattleId != Record.BattleId
		|| ActivePlaybackToken.ResolutionId != Record.ResolutionId
		|| ActivePlaybackToken.PresentationSequence != Record.PresentationSequence)
	{
		return false;
	}

	ABattleManager* Battle = BattleManager.Get();
	FPresentationStateSnapshot Target;
	FBattleReadSnapshot Read;
	if (!Battle->TryGetLatestFrozenPresentationBaseline(Target)
		|| Target.BattleId != CurrentBattleId || Target.StateRevision <= 0
		|| Target.BattleState != EBattleState::PlayerTurn || Target.Outcome != EBattleHUDOutcome::None
		|| ViewModel->Outcome != EBattleHUDOutcome::None
		|| ViewModel->HasAuthoritativePendingCardSelection()
		|| !Battle->TryBuildPlayerFacingReadSnapshot(Read)
		|| Read.BattleId != static_cast<uint64>(Target.BattleId)
		|| Read.StateRevision != static_cast<uint64>(Target.StateRevision))
	{
		return false;
	}
	const FCardReadView* Card = Read.HandCards.FindByPredicate([RuntimeId](const FCardReadView& View)
		{ return View.RuntimeId == RuntimeId; });
	if (!Card || !Card->Card.IsValid() || !Battle->QueryCardPlayability(Card->Card.Get()).bAllowed
		|| !Target.HandCards.ContainsByPredicate([RuntimeId](const FBattleHUDCardView& View)
			{ return View.RuntimeId == RuntimeId && View.bGameplayPlayable; }))
	{
		return false;
	}

	OutIntent.SessionToken = ActivePresentationSessionToken;
	OutIntent.BattleId = Target.BattleId;
	OutIntent.ExpectedReadyRevision = Target.StateRevision;
	OutIntent.RuntimeId = RuntimeId;
	OutIntent.Card = Card->Card;
	OutIntent.CaptureWindow.SourceResolutionId = Record.ResolutionId;
	OutIntent.CaptureWindow.SourcePresentationSequence = Record.PresentationSequence;
	OutIntent.CaptureWindow.LocalWindowGeneration = ActivePlaybackToken.LocalPlaybackGeneration;
	OutIntent.CaptureWindow.ChronologyGeneration = BufferedCardChronologyGeneration;
	return true;
}

bool UBattlePresentationController::IsBufferedCardTargetCurrent(const FBufferedCardIntent& Intent) const
{
	if (!IsPresentationOwnedMode() || !IsCurrentPresentationSession(Intent.SessionToken)
		|| Intent.BattleId != CurrentBattleId || Intent.ExpectedReadyRevision <= 0
		|| !Intent.CaptureWindow.IsValid()
		|| Intent.CaptureWindow.ChronologyGeneration != BufferedCardChronologyGeneration
		|| !Intent.Card.IsValid() || ViewModel->Outcome != EBattleHUDOutcome::None
		|| ViewModel->HasAuthoritativePendingCardSelection())
	{
		return false;
	}
	ABattleManager* Battle = BattleManager.Get();
	FPresentationStateSnapshot Target;
	FBattleReadSnapshot Read;
	if (!Battle->TryGetLatestFrozenPresentationBaseline(Target)
		|| Target.BattleId != Intent.BattleId || Target.StateRevision != Intent.ExpectedReadyRevision
		|| Target.BattleState != EBattleState::PlayerTurn || Target.Outcome != EBattleHUDOutcome::None
		|| !Battle->TryBuildPlayerFacingReadSnapshot(Read)
		|| Read.BattleId != static_cast<uint64>(Intent.BattleId)
		|| Read.StateRevision != static_cast<uint64>(Intent.ExpectedReadyRevision))
	{
		return false;
	}
	const FCardReadView* Card = Read.HandCards.FindByPredicate([&Intent](const FCardReadView& View)
		{ return View.RuntimeId == Intent.RuntimeId && View.Card == Intent.Card; });
	return Card && Battle->QueryCardPlayability(Card->Card.Get()).bAllowed;
}
