#include "BattleHUDViewModel.h"

#include "../Battle/BattleManager.h"
#include "../Battle/BattleReadSnapshot.h"
#include "../Battle/BattleRequestTypes.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Presentation/PresentationTypes.h"

namespace
{
	FText FailureReasonToText(EGameplayRequestFailureReason Reason)
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

	bool AreStatusViewsEqual(const FBattleHUDStatusView& Left, const FBattleHUDStatusView& Right)
	{
		return Left.StatusId == Right.StatusId
			&& Left.RuntimeSequence == Right.RuntimeSequence
			&& Left.DisplayName.EqualTo(Right.DisplayName)
			&& Left.Description.EqualTo(Right.Description)
			&& Left.Amount == Right.Amount
			&& Left.bUseAtlasIcon == Right.bUseAtlasIcon
			&& Left.UVOffset == Right.UVOffset
			&& Left.UVScale == Right.UVScale
			&& Left.TrimOffset == Right.TrimOffset
			&& Left.TrimScale == Right.TrimScale;
	}

	bool AreStatusArraysEqual(
		const TArray<FBattleHUDStatusView>& Left,
		const TArray<FBattleHUDStatusView>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!AreStatusViewsEqual(Left[Index], Right[Index]))
			{
				return false;
			}
		}
		return true;
	}

	bool AreRelicViewsEqual(const FBattleHUDRelicView& Left, const FBattleHUDRelicView& Right)
	{
		return Left.RelicId == Right.RelicId
			&& Left.RuntimeSequence == Right.RuntimeSequence
			&& Left.DisplayName.EqualTo(Right.DisplayName)
			&& Left.Description.EqualTo(Right.Description)
			&& Left.bShowCounter == Right.bShowCounter
			&& Left.Counter == Right.Counter
			&& Left.CounterMax == Right.CounterMax
			&& Left.Icon.Get() == Right.Icon.Get();
	}

	bool AreRelicArraysEqual(
		const TArray<FBattleHUDRelicView>& Left,
		const TArray<FBattleHUDRelicView>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!AreRelicViewsEqual(Left[Index], Right[Index]))
			{
				return false;
			}
		}
		return true;
	}

	bool AreCombatantCoreViewsEqual(
		const FBattleHUDCombatantView& Left,
		const FBattleHUDCombatantView& Right)
	{
		return Left.PresentationId == Right.PresentationId
			&& Left.bPlayer == Right.bPlayer
			&& Left.DisplayName.EqualTo(Right.DisplayName)
			&& Left.HP == Right.HP
			&& Left.MaxHP == Right.MaxHP
			&& Left.Block == Right.Block
			&& Left.bDead == Right.bDead
			&& AreRelicArraysEqual(Left.Relics, Right.Relics);
	}

	bool AreCardViewsEqual(const FBattleHUDCardView& Left, const FBattleHUDCardView& Right)
	{
		return Left.RuntimeId == Right.RuntimeId
			&& Left.CardId == Right.CardId
			&& Left.DisplayName.EqualTo(Right.DisplayName)
			&& Left.bUpgraded == Right.bUpgraded
			&& Left.Cost == Right.Cost
			&& Left.CardType == Right.CardType
			&& Left.Rarity == Right.Rarity
			&& Left.CardColor == Right.CardColor
			&& Left.TargetType == Right.TargetType
			&& Left.Description.EqualTo(Right.Description)
			&& Left.RichDescription.EqualTo(Right.RichDescription)
			&& Left.CardArt.Get() == Right.CardArt.Get()
			&& Left.bGameplayPlayable == Right.bGameplayPlayable
			&& Left.UnplayableReason.EqualTo(Right.UnplayableReason);
	}

	bool AreCardArraysEqual(
		const TArray<FBattleHUDCardView>& Left,
		const TArray<FBattleHUDCardView>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!AreCardViewsEqual(Left[Index], Right[Index]))
			{
				return false;
			}
		}
		return true;
	}

	bool AreIntentViewsEqual(const FBattleHUDIntentView& Left, const FBattleHUDIntentView& Right)
	{
		return Left.Type == Right.Type
			&& Left.DisplayName.EqualTo(Right.DisplayName)
			&& Left.BaseAmount == Right.BaseAmount
			&& Left.bHasCurrentResolvedDamageAmount == Right.bHasCurrentResolvedDamageAmount
			&& Left.CurrentResolvedDamageAmount == Right.CurrentResolvedDamageAmount;
	}
}

bool UBattleHUDViewModel::Initialize(
	ABattleManager* InBattleManager,
	bool bInPresentationDisplayOwned
)
{
	Shutdown();
	if (!IsValid(InBattleManager))
	{
		LastFeedback = FText::FromString(TEXT("Battle HUD has no BattleManager."));
		BroadcastChanged(EBattleHUDDirtyFlags::All);
		return false;
	}

	BattleManager = InBattleManager;
	bPresentationDisplayOwned = bInPresentationDisplayOwned;
	InBattleManager->OnReadStateReady.AddUObject(this, &UBattleHUDViewModel::HandleReadStateReady);

	if (!InBattleManager->IsPresentationAvailable())
	{
		FPresentationStateSnapshot Baseline;
		if (InBattleManager->TryGetLatestFrozenPresentationBaseline(Baseline))
		{
			ApplyPresentationSnapshot(Baseline, true);
		}
		EnterPresentationUnavailable(InBattleManager->GetPresentationUnavailableReason());
		return true;
	}

	FPresentationStateSnapshot Baseline;
	if (InBattleManager->TryGetLatestFrozenPresentationBaseline(Baseline))
	{
		ApplyPresentationSnapshot(Baseline, true);
		if (!bPresentationDisplayOwned)
		{
			RefreshLiveInputBindingsIfCaughtUp();
		}
		return true;
	}

	InteractionState = EBattleHUDInteractionState::Resolving;
	bInputLocked = true;
	bCanEndTurn = false;
	BroadcastChanged(EBattleHUDDirtyFlags::All);
	return true;
}

void UBattleHUDViewModel::Shutdown()
{
	if (ABattleManager* Battle = BattleManager.Get())
	{
		Battle->OnReadStateReady.RemoveAll(this);
	}

	ResetCardPresentationOwnershipState(true);
	NextCardPresentationSelectionGeneration = 1;
	BattleManager.Reset();
	ClearLiveInputBindings();
	HandCards.Reset();
	LegalTargets.Reset();
	SelectedCardRuntimeId = INDEX_NONE;
	BattleId = 0;
	StateRevision = 0;
	DisplayedBattleState = EBattleState::BattleStart;
	bDisplayedSnapshotCanEndTurn = false;
	bPresentationDisplayOwned = false;
	InteractionState = EBattleHUDInteractionState::Resolving;
	Outcome = EBattleHUDOutcome::None;
	bInputLocked = true;
	bCanEndTurn = false;
	LastFeedback = FText::GetEmpty();
	Player = FBattleHUDCombatantView{};
	Enemy = FBattleHUDCombatantView{};
	Energy = 0;
	MaxEnergy = 0;
	DrawCount = 0;
	DiscardCount = 0;
	ExhaustCount = 0;
	EnemyIntent = FBattleHUDIntentView{};
	LastChangeFlags = EBattleHUDDirtyFlags::All;
}

bool UBattleHUDViewModel::SelectCardByRuntimeId(int32 RuntimeId)
{
	if (!CanAcceptSelectionInput() || !IsLiveBindingCurrent())
	{
		SetFeedback(EGameplayRequestFailureReason::ResolutionBusy);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
		return false;
	}

	if (SelectedCardRuntimeId == RuntimeId)
	{
		CancelSelection();
		return true;
	}

	const FBattleHUDCardView* DisplayedCard = FindDisplayedCardByRuntimeId(RuntimeId);
	UCardInstance* Card = FindHandCardByRuntimeId(RuntimeId);
	ABattleManager* Battle = BattleManager.Get();
	if (DisplayedCard == nullptr || !IsValid(Card) || !IsValid(Battle))
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidCard);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
		return false;
	}

	const FGameplayValidationResult Validation = Battle->QueryCardPlayability(Card);
	if (!Validation.bAllowed)
	{
		SetFeedback(Validation.FailureReason);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
		return false;
	}

	ClearFeedback();
	ClearImmediatePreviewInternal();
	SelectedCardRuntimeId = RuntimeId;
	RebuildLegalTargets(Card);

	switch (DisplayedCard->TargetType)
	{
	case ECardTargetType::None:
		InteractionState = EBattleHUDInteractionState::ReadyToConfirm;
		break;
	case ECardTargetType::Self:
	case ECardTargetType::Enemy:
		if (LegalTargetObjects.Num() > 0)
		{
			InteractionState = EBattleHUDInteractionState::ChoosingTarget;
			break;
		}
		ClearSelectionInternal();
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Feedback);
		return false;
	default:
		ClearSelectionInternal();
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Feedback);
		return false;
	}

	bInputLocked = false;
	bCanEndTurn = bDisplayedSnapshotCanEndTurn && Battle->QueryEndPlayerTurn().bAllowed;
	BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Feedback);
	return true;
}

void UBattleHUDViewModel::CancelSelection()
{
	if (InteractionState == EBattleHUDInteractionState::Resolving ||
		InteractionState == EBattleHUDInteractionState::Terminal ||
		InteractionState == EBattleHUDInteractionState::PresentationUnavailable)
	{
		return;
	}

	ClearSelectionInternal();
	ClearFeedback();
	InteractionState = EBattleHUDInteractionState::Idle;
	bInputLocked = !IsLiveBindingCurrent();
	bCanEndTurn = false;
	if (!bInputLocked)
	{
		if (ABattleManager* Battle = BattleManager.Get())
		{
			bCanEndTurn = bDisplayedSnapshotCanEndTurn && Battle->QueryEndPlayerTurn().bAllowed;
		}
	}
	BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Feedback);
}

bool UBattleHUDViewModel::SelectTargetById(int32 TargetId)
{
	if (InteractionState != EBattleHUDInteractionState::ChoosingTarget || bInputLocked || !IsLiveBindingCurrent())
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
		return false;
	}
	ACombatant* Target = FindLegalTargetById(TargetId);
	if (!IsValid(Target))
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
		return false;
	}
	return SubmitSelectedCard(Target);
}

bool UBattleHUDViewModel::ConfirmSelectedCard()
{
	if (InteractionState != EBattleHUDInteractionState::ReadyToConfirm || bInputLocked || !IsLiveBindingCurrent())
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
		return false;
	}
	const FBattleHUDCardView* DisplayedCard = FindDisplayedCardByRuntimeId(SelectedCardRuntimeId);
	UCardInstance* Card = FindHandCardByRuntimeId(SelectedCardRuntimeId);
	if (DisplayedCard == nullptr || !IsValid(Card))
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidCard);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
		return false;
	}
	if (DisplayedCard->TargetType == ECardTargetType::None)
	{
		return SubmitSelectedCard(nullptr);
	}
	SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
	BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
	return false;
}

bool UBattleHUDViewModel::RequestEndTurn()
{
	if (!CanAcceptSelectionInput() || !IsLiveBindingCurrent())
	{
		SetFeedback(EGameplayRequestFailureReason::ResolutionBusy);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
		return false;
	}
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle))
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidBattle);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
		return false;
	}
	const FGameplayRequestResult Result = Battle->RequestEndPlayerTurn();
	if (!Result.IsAcceptedForResolution())
	{
		SetFeedback(Result.FailureReason);
		EBattleHUDDirtyFlags DirtyFlags = EBattleHUDDirtyFlags::Feedback;
		if (!IsLiveBindingCurrent())
		{
			ClearLiveInputBindings();
			bInputLocked = true;
			bCanEndTurn = false;
			DirtyFlags |= EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants;
		}
		BroadcastChanged(DirtyFlags);
		return false;
	}
	ClearSelectionInternal();
	ClearFeedback();
	ClearLiveInputBindings();
	SetResolving();
	BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Feedback);
	return true;
}

bool UBattleHUDViewModel::TryGetLegalTargetByPresentationId(FName PresentationId, FBattleHUDTargetView& OutTarget) const
{
	OutTarget = FBattleHUDTargetView{};
	if (PresentationId.IsNone()) return false;
	for (const FBattleHUDTargetView& Target : LegalTargets)
	{
		if (Target.PresentationId == PresentationId)
		{
			OutTarget = Target;
			return true;
		}
	}
	return false;
}

void UBattleHUDViewModel::ApplyPresentationSnapshot(const FPresentationStateSnapshot& Snapshot, bool bResetInteraction)
{
	const bool bBattleChanged = BattleId != Snapshot.BattleId;
	const bool bRevisionChanged = bBattleChanged || StateRevision != Snapshot.StateRevision;
	EBattleHUDDirtyFlags DirtyFlags = bBattleChanged ? EBattleHUDDirtyFlags::All : EBattleHUDDirtyFlags::None;

	if (!bBattleChanged)
	{
		if (!AreCardArraysEqual(HandCards, Snapshot.HandCards)) DirtyFlags |= EBattleHUDDirtyFlags::Hand;
		if (!AreCombatantCoreViewsEqual(Player, Snapshot.Player) || !AreCombatantCoreViewsEqual(Enemy, Snapshot.Enemy)) DirtyFlags |= EBattleHUDDirtyFlags::Combatants;
		if (!AreStatusArraysEqual(Player.Statuses, Snapshot.Player.Statuses) || !AreStatusArraysEqual(Enemy.Statuses, Snapshot.Enemy.Statuses)) DirtyFlags |= EBattleHUDDirtyFlags::Statuses;
		if (Energy != Snapshot.Energy || MaxEnergy != Snapshot.MaxEnergy) DirtyFlags |= EBattleHUDDirtyFlags::Energy;
		if (DrawCount != Snapshot.DrawCount || DiscardCount != Snapshot.DiscardCount || ExhaustCount != Snapshot.ExhaustCount) DirtyFlags |= EBattleHUDDirtyFlags::PileCounts;
		if (!AreIntentViewsEqual(EnemyIntent, Snapshot.EnemyIntent)) DirtyFlags |= EBattleHUDDirtyFlags::Intent;
		if (Outcome != Snapshot.Outcome) DirtyFlags |= EBattleHUDDirtyFlags::Terminal;
	}

	const int32 PreviousSelectedCardRuntimeId = SelectedCardRuntimeId;
	const int32 PreviousLegalTargetCount = LegalTargets.Num();
	const EBattleHUDInteractionState PreviousInteractionState = InteractionState;
	const bool bPreviousInputLocked = bInputLocked;
	const bool bPreviousCanEndTurn = bCanEndTurn;

	BattleId = Snapshot.BattleId;
	StateRevision = Snapshot.StateRevision;
	DisplayedBattleState = Snapshot.BattleState;
	Outcome = Snapshot.Outcome;
	Energy = Snapshot.Energy;
	MaxEnergy = Snapshot.MaxEnergy;
	Player = Snapshot.Player;
	Enemy = Snapshot.Enemy;
	HandCards = Snapshot.HandCards;
	DrawCount = Snapshot.DrawCount;
	DiscardCount = Snapshot.DiscardCount;
	ExhaustCount = Snapshot.ExhaustCount;
	EnemyIntent = Snapshot.EnemyIntent;
	bDisplayedSnapshotCanEndTurn = Snapshot.bCanEndTurn;

	if (bBattleChanged)
	{
		CompletedPresentationResolutionBattleId = BattleId;
		CompletedPresentationResolutionIds.Reset();
	}

	ClearLiveInputBindings();
	if (bResetInteraction || bRevisionChanged) ClearSelectionInternal();
	if (bRevisionChanged) ClearPendingCardSelectionInputState();

	if (Outcome != EBattleHUDOutcome::None)
	{
		InteractionState = EBattleHUDInteractionState::Terminal;
		bInputLocked = true;
		bCanEndTurn = false;
	}
	else
	{
		InteractionState = EBattleHUDInteractionState::Resolving;
		bInputLocked = true;
		bCanEndTurn = false;
	}

	if (bRevisionChanged || bResetInteraction || PreviousSelectedCardRuntimeId != SelectedCardRuntimeId || PreviousLegalTargetCount != LegalTargets.Num() || PreviousInteractionState != InteractionState || bPreviousInputLocked != bInputLocked || bPreviousCanEndTurn != bCanEndTurn)
	{
		DirtyFlags |= EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants;
	}
	if (bBattleChanged || PreviousInteractionState == EBattleHUDInteractionState::PresentationUnavailable)
	{
		DirtyFlags |= EBattleHUDDirtyFlags::PresentationAvailability;
	}

	// State is made coherent first. Ownership reconciliation is silent until the
	// historical Native/Blueprint change has been published, so no ownership
	// listener reacts against stale formal Hand children.
	const TArray<int32> OwnershipChanges = ReconcileCardPresentationOwnershipInternal();
	BroadcastChanged(DirtyFlags);
	PublishCardPresentationOwnershipChanged(OwnershipChanges);
}

bool UBattleHUDViewModel::RefreshLiveInputBindingsIfCaughtUp()
{
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle) || !Battle->IsPresentationAvailable())
	{
		ClearLiveInputBindings();
		return false;
	}
	FPresentationStateSnapshot LatestBaseline;
	if (!Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline) || LatestBaseline.BattleId != BattleId || LatestBaseline.StateRevision != StateRevision)
	{
		ClearLiveInputBindings();
		return false;
	}
	FBattleReadSnapshot CurrentRead;
	if (!Battle->TryBuildPlayerFacingReadSnapshot(CurrentRead) || static_cast<int64>(CurrentRead.BattleId) != BattleId || static_cast<int64>(CurrentRead.StateRevision) != StateRevision)
	{
		ClearLiveInputBindings();
		return false;
	}
	ClearLiveInputBindings();
	for (const FCardReadView& CardView : CurrentRead.HandCards)
	{
		UCardInstance* Card = CardView.Card.Get();
		if (!IsValid(Card) || FindDisplayedCardByRuntimeId(CardView.RuntimeId) == nullptr)
		{
			ClearLiveInputBindings();
			return false;
		}
		LiveCardBindings.Add(CardView.RuntimeId, Card);
	}
	FName PlayerId = NAME_None;
	FName EnemyId = NAME_None;
	if (!Battle->TryResolveCombatantPresentationId(Battle->Player.Get(), PlayerId) || !Battle->TryResolveCombatantPresentationId(Battle->Enemy.Get(), EnemyId) || PlayerId != Player.PresentationId || EnemyId != Enemy.PresentationId)
	{
		ClearLiveInputBindings();
		return false;
	}
	LiveCombatantBindings.Add(PlayerId, Battle->Player.Get());
	LiveCombatantBindings.Add(EnemyId, Battle->Enemy.Get());
	LiveBindingBattleId = BattleId;
	LiveBindingStateRevision = StateRevision;
	if (Outcome != EBattleHUDOutcome::None)
	{
		InteractionState = EBattleHUDInteractionState::Terminal;
		bInputLocked = true;
		bCanEndTurn = false;
		BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Terminal);
		return true;
	}
	if (DisplayedBattleState != EBattleState::PlayerTurn)
	{
		InteractionState = EBattleHUDInteractionState::Resolving;
		bInputLocked = true;
		bCanEndTurn = false;
		BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants);
		return true;
	}
	ClearSelectionInternal();
	InteractionState = EBattleHUDInteractionState::Idle;
	bInputLocked = false;
	bCanEndTurn = bDisplayedSnapshotCanEndTurn && Battle->QueryEndPlayerTurn().bAllowed;
	BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants);
	return true;
}

void UBattleHUDViewModel::EnterPresentationUnavailable(const FText& Reason)
{
	ClearSelectionInternal();
	ClearLiveInputBindings();
	TArray<int32> OwnershipChanges;
	CardPresentationOwnershipEntries.GetKeys(OwnershipChanges);
	ResetCardPresentationOwnershipState(false);
	InteractionState = EBattleHUDInteractionState::PresentationUnavailable;
	bInputLocked = true;
	bCanEndTurn = false;
	LastFeedback = Reason.IsEmpty() ? FText::FromString(TEXT("Committed Presentation is unavailable for this battle.")) : Reason;
	BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Feedback | EBattleHUDDirtyFlags::Terminal | EBattleHUDDirtyFlags::PresentationAvailability);
	PublishCardPresentationOwnershipChanged(OwnershipChanges);
}

bool UBattleHUDViewModel::IsPresentationDisplayOwned() const
{
	return bPresentationDisplayOwned;
}

void UBattleHUDViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UBattleHUDViewModel::HandleReadStateReady(uint64 InBattleId, uint64 InStateRevision)
{
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle)) return;
	const bool bIncomingRevisionChanged = BattleId != static_cast<int64>(InBattleId) || StateRevision != static_cast<int64>(InStateRevision);
	if (bIncomingRevisionChanged)
	{
		ClearSelectionInternal();
		ClearLiveInputBindings();
		SetResolving();
		if (bPresentationDisplayOwned) BroadcastPreviewChanged();
		else BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants);
	}
	if (!Battle->IsPresentationAvailable())
	{
		FPresentationStateSnapshot LatestBaseline;
		if (Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline) && LatestBaseline.BattleId == static_cast<int64>(InBattleId) && LatestBaseline.StateRevision == static_cast<int64>(InStateRevision))
		{
			ApplyPresentationSnapshot(LatestBaseline, true);
		}
		EnterPresentationUnavailable(Battle->GetPresentationUnavailableReason());
		return;
	}
	if (bPresentationDisplayOwned) return;
	if (BattleId == static_cast<int64>(InBattleId) && StateRevision == static_cast<int64>(InStateRevision) && IsLiveBindingCurrent()) return;
	ApplyLatestFrozenBaselineAndRefresh(true);
}

bool UBattleHUDViewModel::ApplyLatestFrozenBaselineAndRefresh(bool bResetInteraction)
{
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle)) return false;
	FPresentationStateSnapshot Snapshot;
	if (!Battle->TryGetLatestFrozenPresentationBaseline(Snapshot)) return false;
	ApplyPresentationSnapshot(Snapshot, bResetInteraction);
	return RefreshLiveInputBindingsIfCaughtUp();
}

void UBattleHUDViewModel::RebuildLegalTargets(UCardInstance* Card)
{
	LegalTargets.Reset();
	LegalTargetObjects.Reset();
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle) || !IsValid(Card) || !IsLiveBindingCurrent()) return;
	TArray<ACombatant*> Targets;
	Battle->GetLegalTargetsForCard(Card, Targets);
	for (ACombatant* Target : Targets)
	{
		if (!IsValid(Target)) continue;
		FName PresentationId = NAME_None;
		if (!Battle->TryResolveCombatantPresentationId(Target, PresentationId)) continue;
		const FBattleHUDCombatantView* FrozenCombatant = nullptr;
		if (Player.PresentationId == PresentationId) FrozenCombatant = &Player;
		else if (Enemy.PresentationId == PresentationId) FrozenCombatant = &Enemy;
		if (FrozenCombatant == nullptr) continue;
		LegalTargetObjects.Add(Target);
		FBattleHUDTargetView View;
		View.TargetId = LegalTargetObjects.Num();
		View.PresentationId = PresentationId;
		View.DisplayName = FrozenCombatant->DisplayName;
		View.bPlayer = FrozenCombatant->bPlayer;
		LegalTargets.Add(View);
	}
}

bool UBattleHUDViewModel::SubmitSelectedCard(ACombatant* Target)
{
	ABattleManager* Battle = BattleManager.Get();
	UCardInstance* Card = FindHandCardByRuntimeId(SelectedCardRuntimeId);
	if (!IsValid(Battle) || !IsValid(Card) || !IsLiveBindingCurrent())
	{
		SetFeedback(EGameplayRequestFailureReason::CardNoLongerInHand);
		ClearSelectionInternal();
		bInputLocked = true;
		bCanEndTurn = false;
		BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Feedback);
		return false;
	}
	const FGameplayRequestResult Result = Battle->RequestPlayCard(Card, Target);
	if (!Result.IsAcceptedForResolution())
	{
		SetFeedback(Result.FailureReason);
		EBattleHUDDirtyFlags DirtyFlags = EBattleHUDDirtyFlags::Feedback;
		if (!IsLiveBindingCurrent())
		{
			ClearSelectionInternal();
			ClearLiveInputBindings();
			InteractionState = EBattleHUDInteractionState::Resolving;
			bInputLocked = true;
			bCanEndTurn = false;
			DirtyFlags |= EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants;
		}
		BroadcastChanged(DirtyFlags);
		return false;
	}
	ClearSelectionInternal();
	ClearFeedback();
	ClearLiveInputBindings();
	SetResolving();
	BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Feedback);
	return true;
}

void UBattleHUDViewModel::SetResolving()
{
	InteractionState = EBattleHUDInteractionState::Resolving;
	bInputLocked = true;
	bCanEndTurn = false;
}

void UBattleHUDViewModel::ClearSelectionInternal()
{
	SelectedCardRuntimeId = INDEX_NONE;
	LegalTargets.Reset();
	LegalTargetObjects.Reset();
	ClearImmediatePreviewInternal();
}

void UBattleHUDViewModel::ClearLiveInputBindings()
{
	LiveCardBindings.Reset();
	LiveCombatantBindings.Reset();
	LegalTargetObjects.Reset();
	LegalTargets.Reset();
	LiveBindingBattleId = 0;
	LiveBindingStateRevision = 0;
	ClearImmediatePreviewInternal();
}

void UBattleHUDViewModel::SetFeedback(EGameplayRequestFailureReason Reason)
{
	LastFeedback = FailureReasonToText(Reason);
}

void UBattleHUDViewModel::ClearFeedback()
{
	LastFeedback = FText::GetEmpty();
}

void UBattleHUDViewModel::BroadcastChanged(EBattleHUDDirtyFlags DirtyFlags)
{
	LastChangeFlags = DirtyFlags;
	OnNativeChanged.Broadcast(DirtyFlags);
	OnChanged.Broadcast();
}

bool UBattleHUDViewModel::CanAcceptSelectionInput() const
{
	return !bInputLocked && Outcome == EBattleHUDOutcome::None && InteractionState != EBattleHUDInteractionState::Resolving && InteractionState != EBattleHUDInteractionState::Terminal && InteractionState != EBattleHUDInteractionState::PresentationUnavailable;
}

bool UBattleHUDViewModel::IsLiveBindingCurrent() const
{
	return LiveBindingBattleId != 0 && LiveBindingStateRevision != 0 && LiveBindingBattleId == BattleId && LiveBindingStateRevision == StateRevision;
}

const FBattleHUDCardView* UBattleHUDViewModel::FindDisplayedCardByRuntimeId(int32 RuntimeId) const
{
	return HandCards.FindByPredicate([RuntimeId](const FBattleHUDCardView& Card){ return Card.RuntimeId == RuntimeId; });
}

UCardInstance* UBattleHUDViewModel::FindHandCardByRuntimeId(int32 RuntimeId) const
{
	const TWeakObjectPtr<UCardInstance>* Found = LiveCardBindings.Find(RuntimeId);
	return Found ? Found->Get() : nullptr;
}

ACombatant* UBattleHUDViewModel::FindLegalTargetById(int32 TargetId) const
{
	const int32 Index = TargetId - 1;
	return LegalTargetObjects.IsValidIndex(Index) ? LegalTargetObjects[Index].Get() : nullptr;
}
