#include "BattleHUDViewModel.h"

#include "../Battle/BattleManager.h"
#include "../Battle/BattleReadSnapshot.h"
#include "../Battle/BattleSelectionRequest.h"
#include "../Presentation/BattlePresentationController.h"

FBattleHUDInteractionReadinessShadow UBattleHUDViewModel::EvaluateInteractionReadinessShadow(
	const UBattlePresentationController* Controller) const
{
	FBattleHUDInteractionReadinessShadow Result;
	Result.BattleId = BattleId;
	Result.StateRevision = StateRevision;
	Result.SelectedCardRuntimeId = SelectedCardRuntimeId;

	ABattleManager* Battle = BattleManager.Get();
	FPresentationStateSnapshot LatestBaseline;
	FBattleReadSnapshot CurrentRead;
	const bool bExactFrozenAndReadSurface = IsValid(Battle)
		&& Battle->IsPresentationAvailable()
		&& Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline)
		&& LatestBaseline.BattleId == BattleId
		&& LatestBaseline.StateRevision == StateRevision
		&& Battle->TryBuildPlayerFacingReadSnapshot(CurrentRead)
		&& static_cast<int64>(CurrentRead.BattleId) == BattleId
		&& static_cast<int64>(CurrentRead.StateRevision) == StateRevision;

	if (bPresentationDisplayOwned)
	{
		Result.AuthoritySource = EBattleHUDReadinessAuthoritySource::PresentationOwned;
		FPresentationSessionToken SessionToken;
		if (IsValid(Controller)
			&& Controller->TryGetPresentationSessionToken(SessionToken)
			&& Controller->IsCurrentPresentationSession(SessionToken)
			&& SessionToken.BattleId == BattleId)
		{
			Result.SessionToken = SessionToken;
			Result.bExactReadSurface = bExactFrozenAndReadSurface;
		}
	}
	else
	{
		Result.AuthoritySource = EBattleHUDReadinessAuthoritySource::DirectBaseline;
		// Direct/no-history authority is intentionally sessionless.
		Result.bExactReadSurface = bExactFrozenAndReadSurface;
	}

	FPendingCardSelectionReadView AuthoritativePendingView;
	Result.bAuthoritativePendingSelection = IsValid(Battle)
		&& BattleSelectionRequest::TryBuildPendingCardSelectionReadView(
			Battle,
			AuthoritativePendingView)
		&& AuthoritativePendingView.RequestIdentity.IsValid();

	FPendingCardSelectionReadView VisiblePendingView;
	Result.bVisiblePendingSelection = TryGetPendingCardSelectionReadView(VisiblePendingView);
	if (Result.bVisiblePendingSelection)
	{
		Result.PendingSelectionIdentity = VisiblePendingView.RequestIdentity;
	}

	if (Outcome != EBattleHUDOutcome::None
		|| InteractionState == EBattleHUDInteractionState::Terminal
		|| InteractionState == EBattleHUDInteractionState::PresentationUnavailable)
	{
		Result.Mode = EBattleHUDReadinessMode::TerminalOrUnavailable;
		Result.bReady = false;
		Result.bBaselineReady = false;
		Result.bMatchesBaseline = true;
		return Result;
	}

	// An authoritative pending Gameplay request has priority over the ordinary
	// battle interaction enum. It can exist while the formal VM still reads
	// Resolving during boundary catch-up.
	if (Result.bAuthoritativePendingSelection)
	{
		Result.Mode = EBattleHUDReadinessMode::PendingCardSelection;
		const bool bPresentationSelectionScopeExact =
			ActiveCardPresentationSelectionGeneration <= 0
			|| (ActiveCardPresentationSelectionBattleId == BattleId
				&& ActiveCardPresentationSelectionBoundaryRevision ==
					VisiblePendingView.RequestIdentity.SelectionBoundaryRevision);
		Result.bReady = Result.bExactReadSurface
			&& Result.bVisiblePendingSelection
			&& VisiblePendingView.RequestIdentity == AuthoritativePendingView.RequestIdentity
			&& bPresentationSelectionScopeExact;
		Result.bBaselineReady = Result.bVisiblePendingSelection;
		Result.bMatchesBaseline = Result.bReady == Result.bBaselineReady;
		return Result;
	}

	if (InteractionState == EBattleHUDInteractionState::Resolving || bInputLocked)
	{
		Result.Mode = EBattleHUDReadinessMode::Resolving;
		Result.bReady = false;
		Result.bBaselineReady = false;
		Result.bMatchesBaseline = true;
		return Result;
	}

	const bool bSelectedCardOnDisplayedHand = SelectedCardRuntimeId != INDEX_NONE
		&& HandCards.ContainsByPredicate(
			[this](const FBattleHUDCardView& Card)
			{
				return Card.RuntimeId == SelectedCardRuntimeId;
			});
	const bool bExactNormalBindings = Result.bExactReadSurface && IsLiveBindingCurrent();

	if (InteractionState == EBattleHUDInteractionState::ChoosingTarget)
	{
		Result.Mode = EBattleHUDReadinessMode::CardTargetChoice;
		Result.bReady = bExactNormalBindings
			&& bSelectedCardOnDisplayedHand
			&& LegalTargets.Num() > 0;
		Result.bBaselineReady = !bInputLocked
			&& bSelectedCardOnDisplayedHand
			&& LegalTargets.Num() > 0;
		// 8.8 makes EndTurn=false part of the post-fix baseline, not a shadow
		// divergence that later phases are allowed to normalize silently.
		Result.bMatchesBaseline = Result.bReady == Result.bBaselineReady
			&& !bCanEndTurn;
		return Result;
	}

	if (InteractionState == EBattleHUDInteractionState::ReadyToConfirm)
	{
		Result.Mode = EBattleHUDReadinessMode::CardReadyToConfirm;
		Result.bReady = bExactNormalBindings && bSelectedCardOnDisplayedHand;
		Result.bBaselineReady = !bInputLocked && bSelectedCardOnDisplayedHand;
		Result.bMatchesBaseline = Result.bReady == Result.bBaselineReady;
		return Result;
	}

	if (InteractionState == EBattleHUDInteractionState::Idle)
	{
		Result.Mode = EBattleHUDReadinessMode::NormalPlayerTurn;
		Result.bReady = bExactNormalBindings;
		Result.bBaselineReady = !bInputLocked;
		Result.bMatchesBaseline = Result.bReady == Result.bBaselineReady;
		return Result;
	}

	Result.Mode = EBattleHUDReadinessMode::Resolving;
	Result.bReady = false;
	Result.bBaselineReady = false;
	Result.bMatchesBaseline = true;
	return Result;
}
