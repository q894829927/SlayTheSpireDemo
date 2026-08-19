#include "BattleHUDViewModel.h"

#include "../Battle/BattleManager.h"
#include "../Battle/BattleReadSnapshot.h"
#include "../Battle/BattleRequestTypes.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Enemy/EnemyIntent.h"

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

	FBattleHUDCombatantView MakeCombatantView(const FCombatantReadView& Source, const FText& DisplayName)
	{
		FBattleHUDCombatantView Result;
		Result.DisplayName = DisplayName;
		Result.HP = Source.HP;
		Result.MaxHP = Source.MaxHP;
		Result.Block = Source.Block;
		Result.bDead = Source.bDead;
		Result.Statuses.Reserve(Source.Statuses.Num());
		for (const FStatusReadView& Status : Source.Statuses)
		{
			FBattleHUDStatusView StatusView;
			StatusView.StatusId = Status.StatusId;
			StatusView.Amount = Status.Amount;
			Result.Statuses.Add(StatusView);
		}
		return Result;
	}
}

bool UBattleHUDViewModel::Initialize(ABattleManager* InBattleManager)
{
	Shutdown();
	if (!IsValid(InBattleManager))
	{
		LastFeedback = FText::FromString(TEXT("Battle HUD has no BattleManager."));
		BroadcastChanged();
		return false;
	}

	BattleManager = InBattleManager;
	InBattleManager->OnReadStateReady.AddUObject(this, &UBattleHUDViewModel::HandleReadStateReady);

	// UI-A0 contract: subscribe first, then pull. OnReadStateReady is an edge,
	// not a replaying state container.
	if (PullAndApplySnapshot(true))
	{
		return true;
	}

	InteractionState = EBattleHUDInteractionState::Resolving;
	bInputLocked = true;
	bCanEndTurn = false;
	BroadcastChanged();
	return true;
}

void UBattleHUDViewModel::Shutdown()
{
	if (ABattleManager* Battle = BattleManager.Get())
	{
		Battle->OnReadStateReady.RemoveAll(this);
	}

	BattleManager.Reset();
	PendingConfirmationTarget.Reset();
	LegalTargetObjects.Reset();
	CachedHandObjects.Reset();
	HandCards.Reset();
	LegalTargets.Reset();
	SelectedCardRuntimeId = INDEX_NONE;
	BattleId = 0;
	StateRevision = 0;
	InteractionState = EBattleHUDInteractionState::Resolving;
	Outcome = EBattleHUDOutcome::None;
	bInputLocked = true;
	bCanEndTurn = false;
}

bool UBattleHUDViewModel::SelectCardByRuntimeId(int32 RuntimeId)
{
	if (!CanAcceptSelectionInput())
	{
		SetFeedback(EGameplayRequestFailureReason::ResolutionBusy);
		BroadcastChanged();
		return false;
	}

	if (SelectedCardRuntimeId == RuntimeId)
	{
		CancelSelection();
		return true;
	}

	UCardInstance* Card = FindHandCardByRuntimeId(RuntimeId);
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Card) || !IsValid(Battle))
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidCard);
		BroadcastChanged();
		return false;
	}

	const FGameplayValidationResult Validation = Battle->QueryCardPlayability(Card);
	if (!Validation.bAllowed)
	{
		SetFeedback(Validation.FailureReason);
		BroadcastChanged();
		return false;
	}

	ClearFeedback();
	SelectedCardRuntimeId = RuntimeId;
	RebuildLegalTargets(Card);

	switch (Card->GetTargetType())
	{
	case ECardTargetType::None:
		InteractionState = EBattleHUDInteractionState::ReadyToConfirm;
		break;

	case ECardTargetType::Self:
		if (IsValid(PendingConfirmationTarget.Get()))
		{
			InteractionState = EBattleHUDInteractionState::ReadyToConfirm;
			break;
		}

		ClearSelectionInternal();
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged();
		return false;

	case ECardTargetType::Enemy:
		if (LegalTargetObjects.Num() > 0)
		{
			InteractionState = EBattleHUDInteractionState::ChoosingTarget;
			break;
		}

		ClearSelectionInternal();
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged();
		return false;

	default:
		ClearSelectionInternal();
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged();
		return false;
	}

	bInputLocked = false;
	bCanEndTurn = Battle->QueryEndPlayerTurn().bAllowed;
	BroadcastChanged();
	return true;
}

void UBattleHUDViewModel::CancelSelection()
{
	if (InteractionState == EBattleHUDInteractionState::Resolving ||
		InteractionState == EBattleHUDInteractionState::Terminal)
	{
		return;
	}

	ClearSelectionInternal();
	ClearFeedback();
	InteractionState = EBattleHUDInteractionState::Idle;
	bInputLocked = false;
	if (ABattleManager* Battle = BattleManager.Get())
	{
		bCanEndTurn = Battle->QueryEndPlayerTurn().bAllowed;
	}
	BroadcastChanged();
}

bool UBattleHUDViewModel::SelectTargetById(int32 TargetId)
{
	if (InteractionState != EBattleHUDInteractionState::ChoosingTarget || bInputLocked)
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged();
		return false;
	}

	ACombatant* Target = FindLegalTargetById(TargetId);
	if (!IsValid(Target))
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged();
		return false;
	}

	return SubmitSelectedCard(Target);
}

bool UBattleHUDViewModel::ConfirmSelectedCard()
{
	if (InteractionState != EBattleHUDInteractionState::ReadyToConfirm || bInputLocked)
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged();
		return false;
	}

	UCardInstance* Card = FindHandCardByRuntimeId(SelectedCardRuntimeId);
	if (!IsValid(Card))
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidCard);
		BroadcastChanged();
		return false;
	}

	switch (Card->GetTargetType())
	{
	case ECardTargetType::None:
		return SubmitSelectedCard(nullptr);

	case ECardTargetType::Self:
		if (ACombatant* Target = PendingConfirmationTarget.Get())
		{
			return SubmitSelectedCard(Target);
		}
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged();
		return false;

	default:
		SetFeedback(EGameplayRequestFailureReason::InvalidTarget);
		BroadcastChanged();
		return false;
	}
}

bool UBattleHUDViewModel::RequestEndTurn()
{
	if (!CanAcceptSelectionInput())
	{
		SetFeedback(EGameplayRequestFailureReason::ResolutionBusy);
		BroadcastChanged();
		return false;
	}

	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle))
	{
		SetFeedback(EGameplayRequestFailureReason::InvalidBattle);
		BroadcastChanged();
		return false;
	}

	const FGameplayRequestResult Result = Battle->RequestEndPlayerTurn();
	if (!Result.IsAcceptedForResolution())
	{
		SetFeedback(Result.FailureReason);
		if (!PullAndApplySnapshot(true))
		{
			BroadcastChanged();
		}
		return false;
	}

	ClearSelectionInternal();
	ClearFeedback();
	SetResolving();
	BroadcastChanged();
	return true;
}

void UBattleHUDViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UBattleHUDViewModel::HandleReadStateReady(uint64 /*InBattleId*/, uint64 /*InStateRevision*/)
{
	PullAndApplySnapshot(true);
}

bool UBattleHUDViewModel::PullAndApplySnapshot(bool bResetInteraction)
{
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle))
	{
		return false;
	}

	FBattleReadSnapshot Snapshot;
	if (!Battle->TryBuildPlayerFacingReadSnapshot(Snapshot))
	{
		return false;
	}

	ApplySnapshot(Snapshot, bResetInteraction);
	BroadcastChanged();
	return true;
}

void UBattleHUDViewModel::ApplySnapshot(const FBattleReadSnapshot& Snapshot, bool bResetInteraction)
{
	BattleId = static_cast<int64>(Snapshot.BattleId);
	StateRevision = static_cast<int64>(Snapshot.StateRevision);
	Energy = Snapshot.Energy;
	MaxEnergy = Snapshot.MaxEnergy;
	DrawCount = Snapshot.DrawCount;
	DiscardCount = Snapshot.DiscardCount;
	ExhaustCount = Snapshot.ExhaustCount;
	Player = MakeCombatantView(Snapshot.Player, FText::FromString(TEXT("Player")));
	Enemy = MakeCombatantView(Snapshot.Enemy, FText::FromString(TEXT("Enemy")));

	EnemyIntent = FBattleHUDIntentView{};
	EnemyIntent.BaseAmount = Snapshot.EnemyIntent.BaseAmount;
	EnemyIntent.bHasCurrentResolvedDamageAmount = Snapshot.EnemyIntentPlayerFacing.bHasCurrentResolvedDamageAmount;
	EnemyIntent.CurrentResolvedDamageAmount = Snapshot.EnemyIntentPlayerFacing.CurrentResolvedDamageAmount;
	if (Snapshot.EnemyIntent.Type == EEnemyIntentType::Attack)
	{
		EnemyIntent.Type = EBattleHUDIntentType::Attack;
		EnemyIntent.DisplayName = FText::FromString(TEXT("Attack"));
	}

	Outcome = EBattleHUDOutcome::None;
	switch (Snapshot.BattleState)
	{
	case EBattleState::Victory:
		Outcome = EBattleHUDOutcome::Victory;
		break;
	case EBattleState::Defeat:
		Outcome = EBattleHUDOutcome::Defeat;
		break;
	case EBattleState::ResolutionFaulted:
		Outcome = EBattleHUDOutcome::ResolutionFaulted;
		break;
	default:
		break;
	}

	RebuildHandViews(Snapshot);

	if (Outcome != EBattleHUDOutcome::None)
	{
		ClearSelectionInternal();
		InteractionState = EBattleHUDInteractionState::Terminal;
		bInputLocked = true;
		bCanEndTurn = false;
		return;
	}

	if (Snapshot.BattleState != EBattleState::PlayerTurn)
	{
		ClearSelectionInternal();
		InteractionState = EBattleHUDInteractionState::Resolving;
		bInputLocked = true;
		bCanEndTurn = false;
		return;
	}

	if (bResetInteraction)
	{
		ClearSelectionInternal();
		InteractionState = EBattleHUDInteractionState::Idle;
	}

	bInputLocked = false;
	if (ABattleManager* Battle = BattleManager.Get())
	{
		bCanEndTurn = Battle->QueryEndPlayerTurn().bAllowed;
	}
}

void UBattleHUDViewModel::RebuildHandViews(const FBattleReadSnapshot& Snapshot)
{
	HandCards.Reset();
	CachedHandObjects.Reset();
	HandCards.Reserve(Snapshot.HandCards.Num());
	CachedHandObjects.Reserve(Snapshot.HandCards.Num());

	ABattleManager* Battle = BattleManager.Get();
	for (const FCardReadView& Source : Snapshot.HandCards)
	{
		UCardInstance* Card = Source.Card.Get();
		FBattleHUDCardView View;
		View.RuntimeId = Source.RuntimeId;
		View.CardId = Source.CardId;
		View.Cost = Source.CurrentCost;
		View.TargetType = Source.TargetType;
		if (IsValid(Card))
		{
			if (const UCardData* Definition = Card->GetDefinition())
			{
				View.CardType = Definition->CardType;
				View.Description = Definition->Description;
				View.CardArt = Definition->CardArt;
				View.DisplayName = Definition->DisplayName.IsEmpty()
					? FText::FromString(Source.CardId.ToString())
					: Definition->DisplayName;
			}
		}
		if (View.DisplayName.IsEmpty())
		{
			View.DisplayName = FText::FromString(Source.CardId.ToString());
		}

		if (IsValid(Battle) && IsValid(Card))
		{
			const FGameplayValidationResult Validation = Battle->QueryCardPlayability(Card);
			View.bGameplayPlayable = Validation.bAllowed;
			if (!Validation.bAllowed)
			{
				View.UnplayableReason = FailureReasonToText(Validation.FailureReason);
			}
		}

		HandCards.Add(View);
		CachedHandObjects.Add(Card);
	}
}

void UBattleHUDViewModel::RebuildLegalTargets(UCardInstance* Card)
{
	LegalTargets.Reset();
	LegalTargetObjects.Reset();
	PendingConfirmationTarget.Reset();

	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle) || !IsValid(Card))
	{
		return;
	}

	TArray<ACombatant*> Targets;
	Battle->GetLegalTargetsForCard(Card, Targets);

	switch (Card->GetTargetType())
	{
	case ECardTargetType::None:
		return;

	case ECardTargetType::Self:
		if (Targets.Num() == 1 &&
			IsValid(Targets[0]) &&
			Targets[0] == Battle->Player.Get())
		{
			// Selection-time advisory candidate only. RequestPlayCard revalidates
			// the target against authoritative gameplay state at confirmation time.
			PendingConfirmationTarget = Targets[0];
		}
		return;

	case ECardTargetType::Enemy:
		break;

	default:
		return;
	}

	for (ACombatant* Target : Targets)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		LegalTargetObjects.Add(Target);
		FBattleHUDTargetView View;
		View.TargetId = LegalTargetObjects.Num();
		View.bPlayer = Target == Battle->Player.Get();
		View.DisplayName = FText::FromString(View.bPlayer ? TEXT("Player") : TEXT("Enemy"));
		LegalTargets.Add(View);
	}
}

bool UBattleHUDViewModel::SubmitSelectedCard(ACombatant* Target)
{
	ABattleManager* Battle = BattleManager.Get();
	UCardInstance* Card = FindHandCardByRuntimeId(SelectedCardRuntimeId);
	if (!IsValid(Battle) || !IsValid(Card))
	{
		SetFeedback(EGameplayRequestFailureReason::CardNoLongerInHand);
		ClearSelectionInternal();
		InteractionState = EBattleHUDInteractionState::Idle;
		BroadcastChanged();
		return false;
	}

	const FGameplayRequestResult Result = Battle->RequestPlayCard(Card, Target);
	if (!Result.IsAcceptedForResolution())
	{
		SetFeedback(Result.FailureReason);
		if (!PullAndApplySnapshot(true))
		{
			ClearSelectionInternal();
			InteractionState = EBattleHUDInteractionState::Idle;
			BroadcastChanged();
		}
		return false;
	}

	ClearSelectionInternal();
	ClearFeedback();
	SetResolving();
	BroadcastChanged();
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
	PendingConfirmationTarget.Reset();
}

void UBattleHUDViewModel::SetFeedback(EGameplayRequestFailureReason Reason)
{
	LastFeedback = FailureReasonToText(Reason);
}

void UBattleHUDViewModel::ClearFeedback()
{
	LastFeedback = FText::GetEmpty();
}

void UBattleHUDViewModel::BroadcastChanged()
{
	OnChanged.Broadcast();
}

bool UBattleHUDViewModel::CanAcceptSelectionInput() const
{
	return !bInputLocked
		&& Outcome == EBattleHUDOutcome::None
		&& InteractionState != EBattleHUDInteractionState::Resolving
		&& InteractionState != EBattleHUDInteractionState::Terminal;
}

UCardInstance* UBattleHUDViewModel::FindHandCardByRuntimeId(int32 RuntimeId) const
{
	for (int32 Index = 0; Index < HandCards.Num() && Index < CachedHandObjects.Num(); ++Index)
	{
		if (HandCards[Index].RuntimeId == RuntimeId)
		{
			return CachedHandObjects[Index].Get();
		}
	}
	return nullptr;
}

ACombatant* UBattleHUDViewModel::FindLegalTargetById(int32 TargetId) const
{
	const int32 Index = TargetId - 1;
	return LegalTargetObjects.IsValidIndex(Index) ? LegalTargetObjects[Index].Get() : nullptr;
}
