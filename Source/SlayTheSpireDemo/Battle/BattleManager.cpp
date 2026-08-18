#include "BattleManager.h"

#include "BattleReadSnapshot.h"
#include "../Actions/ApplyStatusAction.h"
#include "../Actions/BattleActionQueue.h"
#include "../Actions/DamageAction.h"
#include "../Actions/DiscardCardAction.h"
#include "../Actions/DrawCardAction.h"
#include "../Actions/GainBlockAction.h"
#include "../Actions/PlayCardAction.h"
#include "../Actions/TurnEndedAction.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEvent.h"
#include "../Events/BattleEventDispatcher.h"
#include "../Modifiers/ModifierTypes.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusData.h"
#include "../Status/StatusInstance.h"

namespace
{
	FCardReadView MakeCardReadView(UCardInstance* Card)
	{
		FCardReadView View;
		if (!IsValid(Card))
		{
			return View;
		}

		View.Card = Card;
		View.CardId = Card->GetCardId();
		View.RuntimeId = Card->GetRuntimeId();
		View.CurrentCost = Card->GetCurrentCost();
		View.TargetType = Card->GetTargetType();
		return View;
	}

	void AppendCardReadViews(
		const TArray<TObjectPtr<UCardInstance>>& Cards,
		TArray<FCardReadView>& OutViews
	)
	{
		OutViews.Reset();
		OutViews.Reserve(Cards.Num());
		for (const TObjectPtr<UCardInstance>& Card : Cards)
		{
			if (IsValid(Card.Get()))
			{
				OutViews.Add(MakeCardReadView(Card.Get()));
			}
		}
	}

	FCombatantReadView MakeCombatantReadView(ACombatant* Combatant)
	{
		FCombatantReadView View;
		if (!IsValid(Combatant))
		{
			return View;
		}

		View.Combatant = Combatant;
		View.HP = Combatant->HP;
		View.MaxHP = Combatant->MaxHP;
		View.Block = Combatant->Block;
		View.bDead = Combatant->IsDead();

		if (const UStatusContainer* StatusContainer = Combatant->GetStatusContainer())
		{
			for (const TObjectPtr<UStatusInstance>& Status : StatusContainer->GetStatuses())
			{
				if (!IsValid(Status.Get()))
				{
					continue;
				}

				FStatusReadView StatusView;
				StatusView.StatusId = Status->GetStatusId();
				StatusView.Amount = Status->GetAmount();
				StatusView.RuntimeSequence = Status->GetRuntimeSequence();
				View.Statuses.Add(StatusView);
			}
		}

		return View;
	}
}

ABattleManager::ABattleManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABattleManager::StartBattle()
{
	if (!HasValidCombatants())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] StartBattle failed: Player or Enemy reference is not assigned."));
		return;
	}

	// A restarted battle replaces the complete Queue scope. Detach this manager
	// from the old Queue first so late completion/fault signals from abandoned
	// work cannot affect the new battle.
	if (IsValid(ActionQueue.Get()))
	{
		ActionQueue->OnQueueEmpty.RemoveAll(this);
		ActionQueue->OnResolutionIdle.RemoveAll(this);
		ActionQueue->OnResolutionFaulted.RemoveAll(this);
	}

	ActionQueue = NewObject<UBattleActionQueue>(this);
	if (!HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] StartBattle failed: could not create ActionQueue."));
		return;
	}

	ActionQueue->OnQueueEmpty.AddUObject(this, &ABattleManager::HandleActionQueueEmpty);
	ActionQueue->OnResolutionIdle.AddUObject(this, &ABattleManager::HandleActionQueueResolutionIdle);
	ActionQueue->OnResolutionFaulted.AddUObject(this, &ABattleManager::HandleActionQueueResolutionFaulted);

	EventDispatcher = NewObject<UBattleEventDispatcher>(this);
	if (!HasValidEventDispatcher())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] StartBattle failed: could not create EventDispatcher."));
		return;
	}

	DeckRuntime = NewObject<UDeckRuntime>(this);
	if (!HasValidDeckRuntime())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] StartBattle failed: could not create DeckRuntime."));
		return;
	}
	DeckRuntime->InitializeFromDefinitions(DebugStartingDeck, DeckDebugSeed);

	if (DebugStartingDeck.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] DebugStartingDeck is empty. Configure CardData assets on BP_BattleManager for a playable opening Hand."));
	}

	NextRuntimeSequence = 1;
	Player->InitializeCombatant();
	Enemy->InitializeCombatant();

	BattleState = EBattleState::BattleStart;
	Energy = 0;
	CommittedEnemyIntent = FEnemyIntent{};

	++BattleId;
	if (BattleId == 0)
	{
		BattleId = 1;
	}
	StateRevision = 1;

#if WITH_DEV_AUTOMATION_TESTS
	bForceInvalidPlayerEndBatchForTesting = false;
	bForceInvalidEnemyTurnBatchForTesting = false;
	StateBeforeLastResolutionFaultForTesting = EBattleState::BattleStart;
#endif

	CommitNextEnemyIntent();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Battle started. BattleId=%llu ActionQueue, EventDispatcher, DeckRuntime and StatusContainers initialized."),
		BattleId
	);
	StartOpeningHand();
}

void ABattleManager::TestAttack()
{
	if (!HasValidCombatants())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestAttack failed: Player or Enemy reference is not assigned."));
		return;
	}

	if (!HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestAttack failed: ActionQueue is not initialized."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestAttack rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestAttack rejected: action queue is busy."));
		return;
	}

	if (Energy < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestAttack rejected: not enough energy."));
		return;
	}

	if (Enemy->IsDead())
	{
		CheckBattleResult();
		return;
	}

	--Energy;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Player test attack queued: BaseAmount=%d Energy=%d/%d"),
		PlayerTestAttackDamage,
		Energy,
		MaxEnergy
	);

	QueueDamageAction(Player.Get(), Enemy.Get(), PlayerTestAttackDamage, EDamageKind::Attack);
	ActionQueue->StartProcessing();
}

void ABattleManager::TestGainBlock()
{
	if (!HasValidCombatants() || !HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestGainBlock failed: battle references or ActionQueue are invalid."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestGainBlock rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestGainBlock rejected: action queue is busy."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Player test block queued: BaseAmount=%d"), PlayerTestBlockAmount);
	QueueGainBlockAction(Player.Get(), Player.Get(), PlayerTestBlockAmount);
	ActionQueue->StartProcessing();
}

void ABattleManager::TestActionQueueOrder()
{
	if (!HasValidCombatants() || !HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestActionQueueOrder failed: battle references or ActionQueue are invalid."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestActionQueueOrder rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestActionQueueOrder rejected: action queue is busy."));
		return;
	}

	if (Enemy->IsDead())
	{
		CheckBattleResult();
		return;
	}

	QueueDamageAction(Player.Get(), Enemy.Get(), 7, EDamageKind::Attack);
	QueueDamageAction(Player.Get(), Enemy.Get(), 8, EDamageKind::Attack);

	UDamageAction* FrontAction = NewObject<UDamageAction>(ActionQueue.Get());
	FrontAction->Initialize(Player.Get(), Enemy.Get(), 6, EDamageKind::Attack);
	ActionQueue->AddToFront(FrontAction);

	UE_LOG(LogTemp, Log, TEXT("[Battle] Queue-order test started. Expected BaseAmount order: 6, 7, 8."));
	ActionQueue->StartProcessing();
}

void ABattleManager::TestDrawCard()
{
	if (!HasValidActionQueue() || !HasValidDeckRuntime())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestDrawCard failed: ActionQueue or DeckRuntime is invalid."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestDrawCard rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestDrawCard rejected: action queue is busy."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Test draw requested."));
	QueueDrawCardAction();
	ActionQueue->StartProcessing();
}

void ABattleManager::TestDiscardCard()
{
	if (!HasValidActionQueue() || !HasValidDeckRuntime())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestDiscardCard failed: ActionQueue or DeckRuntime is invalid."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestDiscardCard rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestDiscardCard rejected: action queue is busy."));
		return;
	}

	UCardInstance* CardToDiscard = DeckRuntime->GetFirstHandCard();
	if (!IsValid(CardToDiscard))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestDiscardCard skipped: Hand is empty."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Test discard requested for %s."), *CardToDiscard->GetDebugLabel());
	QueueDiscardCardAction(CardToDiscard);
	ActionQueue->StartProcessing();
}

void ABattleManager::TestPlayFirstCard()
{
	if (!HasValidCombatants() || !HasValidActionQueue() || !HasValidDeckRuntime())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestPlayFirstCard failed: battle references, ActionQueue or DeckRuntime are invalid."));
		return;
	}

	UCardInstance* CardToPlay = DeckRuntime->GetFirstHandCard();
	if (!IsValid(CardToPlay))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPlayFirstCard skipped: Hand is empty."));
		return;
	}

	ACombatant* RequestedTarget = nullptr;
	switch (CardToPlay->GetTargetType())
	{
	case ECardTargetType::None:
		break;
	case ECardTargetType::Self:
		RequestedTarget = Player.Get();
		break;
	case ECardTargetType::Enemy:
		RequestedTarget = Enemy.Get();
		break;
	default:
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Test play requested through formal RequestPlayCard for %s."), *CardToPlay->GetDebugLabel());
	const FGameplayRequestResult Result = RequestPlayCard(CardToPlay, RequestedTarget);
	if (!Result.IsAcceptedForResolution())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPlayFirstCard rejected. Reason=%d"), static_cast<int32>(Result.FailureReason));
	}
}

void ABattleManager::TestApplyPhase5AStatuses()
{
	if (!HasValidCombatants() || !HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestApplyPhase5AStatuses failed: combatants or ActionQueue are invalid."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5AStatuses rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5AStatuses rejected: action queue is busy."));
		return;
	}

	if (DebugPhase5AStatuses.Num() < 2 || !IsValid(DebugPhase5AStatuses[0].Get()) || !IsValid(DebugPhase5AStatuses[1].Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5AStatuses requires two configured StatusData assets."));
		return;
	}

	UStatusData* FirstStatus = DebugPhase5AStatuses[0].Get();
	UStatusData* SecondStatus = DebugPhase5AStatuses[1].Get();
	if (FirstStatus->StatusId.IsNone() || SecondStatus->StatusId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5AStatuses requires non-empty StatusId values."));
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Phase 5A status test queued: %s +2 to Player, %s +1 to Player, %s +2 to Enemy."),
		*FirstStatus->StatusId.ToString(),
		*FirstStatus->StatusId.ToString(),
		*SecondStatus->StatusId.ToString()
	);

	QueueApplyStatusAction(Player.Get(), Player.Get(), FirstStatus, 2);
	QueueApplyStatusAction(Player.Get(), Player.Get(), FirstStatus, 1);
	QueueApplyStatusAction(Player.Get(), Enemy.Get(), SecondStatus, 2);
	ActionQueue->StartProcessing();
}

void ABattleManager::TestApplyPhase5B1Strength()
{
	if (!HasValidCombatants() || !HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestApplyPhase5B1Strength failed: combatants or ActionQueue are invalid."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5B1Strength rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5B1Strength rejected: action queue is busy."));
		return;
	}

	if (DebugPhase5AStatuses.Num() < 1 || !IsValid(DebugPhase5AStatuses[0].Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5B1Strength requires DebugPhase5AStatuses[0]."));
		return;
	}

	UStatusData* StrengthDefinition = DebugPhase5AStatuses[0].Get();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Phase 5B1 source-status test queued: %s +2 to Player."),
		*StrengthDefinition->StatusId.ToString()
	);
	QueueApplyStatusAction(Player.Get(), Player.Get(), StrengthDefinition, 2);
	ActionQueue->StartProcessing();
}

void ABattleManager::TestPhase5B1EffectDamage()
{
	if (!HasValidCombatants() || !HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestPhase5B1EffectDamage failed: combatants or ActionQueue are invalid."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPhase5B1EffectDamage rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPhase5B1EffectDamage rejected: action queue is busy."));
		return;
	}

	if (Enemy->IsDead())
	{
		CheckBattleResult();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Phase 5B1 Effect-damage test queued: BaseAmount=9."));
	QueueDamageAction(Player.Get(), Enemy.Get(), 9, EDamageKind::Effect);
	ActionQueue->StartProcessing();
}

void ABattleManager::TestApplyPhase5B2DamageStatuses()
{
	if (!HasValidCombatants() || !HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestApplyPhase5B2DamageStatuses failed: combatants or ActionQueue are invalid."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5B2DamageStatuses rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5B2DamageStatuses rejected: action queue is busy."));
		return;
	}

	if (DebugPhase5AStatuses.Num() < 1 || !IsValid(DebugPhase5AStatuses[0].Get()) ||
		DebugPhase5B2Statuses.Num() < 2 || !IsValid(DebugPhase5B2Statuses[0].Get()) || !IsValid(DebugPhase5B2Statuses[1].Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5B2DamageStatuses requires Strength plus Weak/Vulnerable StatusData assets."));
		return;
	}

	UStatusData* StrengthDefinition = DebugPhase5AStatuses[0].Get();
	UStatusData* WeakDefinition = DebugPhase5B2Statuses[0].Get();
	UStatusData* VulnerableDefinition = DebugPhase5B2Statuses[1].Get();

	if (StrengthDefinition->StatusId.IsNone() || WeakDefinition->StatusId.IsNone() || VulnerableDefinition->StatusId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestApplyPhase5B2DamageStatuses requires non-empty StatusId values."));
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Phase 5B2 status test queued in reverse modifier-phase order: %s +2 Enemy, %s +3 Player, %s +2 Player."),
		*VulnerableDefinition->StatusId.ToString(),
		*WeakDefinition->StatusId.ToString(),
		*StrengthDefinition->StatusId.ToString()
	);

	QueueApplyStatusAction(Player.Get(), Enemy.Get(), VulnerableDefinition, 2);
	QueueApplyStatusAction(Player.Get(), Player.Get(), WeakDefinition, 3);
	QueueApplyStatusAction(Player.Get(), Player.Get(), StrengthDefinition, 2);
	ActionQueue->StartProcessing();
}

void ABattleManager::TestPhase5CBlockPipeline()
{
	if (!HasValidCombatants() || !HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestPhase5CBlockPipeline failed: combatants or ActionQueue are invalid."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPhase5CBlockPipeline rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPhase5CBlockPipeline rejected: action queue is busy."));
		return;
	}

	if (DebugPhase5CStatuses.Num() < 2 || !IsValid(DebugPhase5CStatuses[0].Get()) || !IsValid(DebugPhase5CStatuses[1].Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPhase5CBlockPipeline requires Dexterity/Frailty StatusData assets."));
		return;
	}

	UStatusData* DexterityDefinition = DebugPhase5CStatuses[0].Get();
	UStatusData* FrailtyDefinition = DebugPhase5CStatuses[1].Get();
	if (DexterityDefinition->StatusId.IsNone() || FrailtyDefinition->StatusId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPhase5CBlockPipeline requires non-empty StatusId values."));
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Phase 5C block test queued in reverse modifier-phase order: %s +3 Player, %s +2 Player, then BaseBlock=5."),
		*FrailtyDefinition->StatusId.ToString(),
		*DexterityDefinition->StatusId.ToString()
	);

	QueueApplyStatusAction(Player.Get(), Player.Get(), FrailtyDefinition, 3);
	QueueApplyStatusAction(Player.Get(), Player.Get(), DexterityDefinition, 2);
	QueueGainBlockAction(Player.Get(), Player.Get(), 5);
	ActionQueue->StartProcessing();
}

void ABattleManager::EndPlayerTurn()
{
	const FGameplayRequestResult Result = RequestEndPlayerTurn();
	if (!Result.IsAcceptedForResolution())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] EndPlayerTurn wrapper rejected. Reason=%d"), static_cast<int32>(Result.FailureReason));
	}
}

FGameplayValidationResult ABattleManager::QueryCardPlayability(const UCardInstance* Card) const
{
	return ValidateCardPlayBase(Card);
}

FGameplayValidationResult ABattleManager::QueryPlayCard(
	const UCardInstance* Card,
	const ACombatant* RequestedTarget
) const
{
	return ValidatePlayCard(Card, RequestedTarget);
}

FGameplayRequestResult ABattleManager::RequestPlayCard(UCardInstance* Card, ACombatant* RequestedTarget)
{
	const FGameplayValidationResult Validation = ValidatePlayCard(Card, RequestedTarget);
	if (!Validation.bAllowed)
	{
		return FGameplayRequestResult::Rejected(Validation.FailureReason);
	}

	UBattleEventDispatcher* Dispatcher = nullptr;
	TArray<ACombatant*> Combatants;
	if (!TryBuildEventDispatchContext(Dispatcher, Combatants))
	{
		return FGameplayRequestResult::Rejected(EGameplayRequestFailureReason::InvalidBattle);
	}

	UPlayCardAction* Action = NewObject<UPlayCardAction>(ActionQueue.Get());
	Action->Initialize(this, Card, Player.Get(), RequestedTarget, DeckRuntime.Get(), Dispatcher, Combatants);

	if (!ActionQueue->AddToBack(Action))
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] RequestPlayCard failed to enqueue %s after successful validation."), *Card->GetDebugLabel());
		ActionQueue->RequestResolutionFault(TEXT("Formal card-play request validated but its initial PlayCardAction could not be enqueued."));
		return FGameplayRequestResult::Rejected(EGameplayRequestFailureReason::QueueRejected);
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] RequestPlayCard accepted for resolution: %s."), *Card->GetDebugLabel());
	if (!ActionQueue->StartProcessing())
	{
		ActionQueue->RequestResolutionFault(TEXT("Formal card-play request was accepted but ActionQueue could not start processing."));
	}

	return FGameplayRequestResult::Accepted();
}

FGameplayValidationResult ABattleManager::QueryEndPlayerTurn() const
{
	return ValidatePlayerCommandBase();
}

FGameplayRequestResult ABattleManager::RequestEndPlayerTurn()
{
	const FGameplayValidationResult Validation = ValidatePlayerCommandBase();
	if (!Validation.bAllowed)
	{
		return FGameplayRequestResult::Rejected(Validation.FailureReason);
	}

	TArray<UBattleAction*> TurnEndBatch;
	if (!BuildPlayerTurnEndBatch(TurnEndBatch))
	{
		ActionQueue->RequestResolutionFault(TEXT("Player turn-ending batch construction failed after authoritative validation."));
		return FGameplayRequestResult::Rejected(EGameplayRequestFailureReason::QueueRejected);
	}

	if (!ActionQueue->AddBatchToBackPreserveOrder(TurnEndBatch))
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] EndPlayerTurn failed to enqueue the atomic HandCleanup + TurnEndedAction batch."));
		ActionQueue->RequestResolutionFault(TEXT("Player turn-ending batch insertion failed before PlayerTurnEnding state commit."));
		return FGameplayRequestResult::Rejected(EGameplayRequestFailureReason::QueueRejected);
	}

	BattleState = EBattleState::PlayerTurnEnding;
	Energy = 0;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Player turn ending committed. HandCleanup=%d TurnEndedAction queued atomically."),
		FMath::Max(0, TurnEndBatch.Num() - 1)
	);

	if (!ActionQueue->StartProcessing())
	{
		ActionQueue->RequestResolutionFault(TEXT("Player turn-ending batch was accepted but could not start processing."));
	}

	return FGameplayRequestResult::Accepted();
}

void ABattleManager::GetLegalTargetsForCard(const UCardInstance* Card, TArray<ACombatant*>& OutTargets) const
{
	OutTargets.Reset();
	const FGameplayValidationResult Validation = ValidateCardPlayBase(Card);
	if (!Validation.bAllowed || !IsValid(Card))
	{
		return;
	}

	switch (Card->GetTargetType())
	{
	case ECardTargetType::None:
		break;
	case ECardTargetType::Self:
		OutTargets.Add(Player.Get());
		break;
	case ECardTargetType::Enemy:
		OutTargets.Add(Enemy.Get());
		break;
	default:
		break;
	}
}

bool ABattleManager::TryBuildReadSnapshot(FBattleReadSnapshot& OutSnapshot) const
{
	OutSnapshot = FBattleReadSnapshot{};
	if (!HasValidCombatants() || !HasValidActionQueue() || !HasValidDeckRuntime())
	{
		return false;
	}

	if (ActionQueue->IsBusy() && !ActionQueue->IsResolutionFaulted())
	{
		return false;
	}

	OutSnapshot.BattleId = BattleId;
	OutSnapshot.StateRevision = StateRevision;
	OutSnapshot.BattleState = BattleState;
	OutSnapshot.Energy = Energy;
	OutSnapshot.MaxEnergy = MaxEnergy;
	OutSnapshot.Player = MakeCombatantReadView(Player.Get());
	OutSnapshot.Enemy = MakeCombatantReadView(Enemy.Get());
	OutSnapshot.EnemyIntent = CommittedEnemyIntent;

	AppendCardReadViews(DeckRuntime->GetHandCards(), OutSnapshot.HandCards);
	AppendCardReadViews(DeckRuntime->GetDiscardCards(), OutSnapshot.DiscardCards);
	AppendCardReadViews(DeckRuntime->GetExhaustCards(), OutSnapshot.ExhaustCards);

	OutSnapshot.DrawCount = DeckRuntime->GetDrawCount();
	OutSnapshot.HandCount = DeckRuntime->GetHandCount();
	OutSnapshot.DiscardCount = DeckRuntime->GetDiscardCount();
	OutSnapshot.ExhaustCount = DeckRuntime->GetExhaustCount();
	OutSnapshot.PlayAreaCount = DeckRuntime->GetPlayAreaCount();
	return true;
}

const FEnemyIntent& ABattleManager::GetCommittedEnemyIntent() const
{
	return CommittedEnemyIntent;
}

bool ABattleManager::CanSpendEnergy(int32 Amount) const
{
	return Amount >= 0 && Energy >= Amount;
}

bool ABattleManager::TrySpendEnergy(int32 Amount)
{
	if (!CanSpendEnergy(Amount))
	{
		return false;
	}

	Energy -= Amount;
	UE_LOG(LogTemp, Log, TEXT("[Battle] Energy spent: Amount=%d Energy=%d/%d"), Amount, Energy, MaxEnergy);
	return true;
}

uint64 ABattleManager::AllocateRuntimeSequence()
{
	if (NextRuntimeSequence == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] RuntimeSequence allocator overflowed."));
		return 0;
	}

	return NextRuntimeSequence++;
}

#if WITH_DEV_AUTOMATION_TESTS
UBattleActionQueue* ABattleManager::GetActionQueueForTesting() const
{
	return ActionQueue.Get();
}

UDeckRuntime* ABattleManager::GetDeckRuntimeForTesting() const
{
	return DeckRuntime.Get();
}

void ABattleManager::SetForceInvalidPlayerEndBatchForTesting(bool bForceInvalid)
{
	bForceInvalidPlayerEndBatchForTesting = bForceInvalid;
}

void ABattleManager::SetForceInvalidEnemyTurnBatchForTesting(bool bForceInvalid)
{
	bForceInvalidEnemyTurnBatchForTesting = bForceInvalid;
}

void ABattleManager::SetCommittedEnemyAttackIntentForTesting(int32 BaseAmount)
{
	CommittedEnemyIntent = FEnemyIntent::MakeAttack(BaseAmount);
}

EBattleState ABattleManager::GetStateBeforeLastResolutionFaultForTesting() const
{
	return StateBeforeLastResolutionFaultForTesting;
}
#endif

void ABattleManager::StartOpeningHand()
{
	if (BattleState == EBattleState::ResolutionFaulted)
	{
		return;
	}

	if (!HasValidCombatants() || !HasValidActionQueue() || !HasValidDeckRuntime() || !HasValidEventDispatcher())
	{
		ActionQueue->RequestResolutionFault(TEXT("Opening Hand could not start because battle runtime dependencies are invalid."));
		return;
	}

	TArray<UBattleAction*> OpeningBatch;
	if (!BuildDrawActionBatch(OpeningHandDrawCount, OpeningBatch))
	{
		ActionQueue->RequestResolutionFault(TEXT("Opening Hand draw batch could not be built."));
		return;
	}

	if (!ActionQueue->AddBatchToBackPreserveOrder(OpeningBatch))
	{
		ActionQueue->RequestResolutionFault(TEXT("Opening Hand draw batch insertion failed while BattleStart was authoritative."));
		return;
	}

	Energy = MaxEnergy;
	Player->ClearBlock();

	if (OpeningBatch.Num() == 0)
	{
		CompletePlayerTurnStart();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Opening Hand resolution started. DrawAttempts=%d"), OpeningBatch.Num());
	if (!ActionQueue->StartProcessing())
	{
		ActionQueue->RequestResolutionFault(TEXT("Opening Hand batch was accepted but could not start processing."));
	}
}

void ABattleManager::StartPlayerTurn()
{
	if (BattleState == EBattleState::ResolutionFaulted)
	{
		return;
	}

	if (!HasValidCombatants() || Player->IsDead() || Enemy->IsDead())
	{
		CheckBattleResult();
		return;
	}

	if (!HasValidActionQueue() || !HasValidDeckRuntime() || !HasValidEventDispatcher())
	{
		if (HasValidActionQueue())
		{
			ActionQueue->RequestResolutionFault(TEXT("Player turn-start could not begin because battle runtime dependencies are invalid."));
		}
		return;
	}

	TArray<UBattleAction*> TurnStartBatch;
	if (!BuildDrawActionBatch(PlayerTurnDrawCount, TurnStartBatch))
	{
		ActionQueue->RequestResolutionFault(TEXT("Player turn-start draw batch could not be built."));
		return;
	}

	if (!ActionQueue->AddBatchToBackPreserveOrder(TurnStartBatch))
	{
		ActionQueue->RequestResolutionFault(TEXT("Player turn-start draw batch insertion failed before PlayerTurnStarting state commit."));
		return;
	}

	BattleState = EBattleState::PlayerTurnStarting;
	Energy = MaxEnergy;
	Player->ClearBlock();

	if (TurnStartBatch.Num() == 0)
	{
		CompletePlayerTurnStart();
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] PlayerTurnStarting committed. Energy=%d/%d DrawAttempts=%d"),
		Energy,
		MaxEnergy,
		TurnStartBatch.Num()
	);

	if (!ActionQueue->StartProcessing())
	{
		ActionQueue->RequestResolutionFault(TEXT("Player turn-start batch was accepted but could not start processing."));
	}
}

void ABattleManager::CompletePlayerTurnStart()
{
	if (BattleState != EBattleState::BattleStart && BattleState != EBattleState::PlayerTurnStarting)
	{
		if (HasValidActionQueue())
		{
			ActionQueue->RequestResolutionFault(TEXT("Player turn-start completion reached an unexpected BattleState."));
		}
		return;
	}

	BattleState = EBattleState::PlayerTurn;
	AdvanceStateRevision();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Player turn is gameplay request-eligible. Energy=%d/%d Hand=%d Revision=%llu"),
		Energy,
		MaxEnergy,
		HasValidDeckRuntime() ? DeckRuntime->GetHandCount() : 0,
		StateRevision
	);
}

void ABattleManager::StartEnemyTurn()
{
	if (BattleState == EBattleState::ResolutionFaulted)
	{
		return;
	}

	if (!HasValidCombatants() || Player->IsDead() || Enemy->IsDead())
	{
		CheckBattleResult();
		return;
	}

	if (!HasValidActionQueue() || !HasValidEventDispatcher() || !CommittedEnemyIntent.IsCommitted())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] StartEnemyTurn failed: runtime dependencies or committed Enemy Intent are invalid."));
		if (HasValidActionQueue())
		{
			ActionQueue->RequestResolutionFault(TEXT("Enemy turn has no valid committed Intent source."));
		}
		return;
	}

	TArray<UBattleAction*> EnemyTurnBatch;
	switch (CommittedEnemyIntent.Type)
	{
	case EEnemyIntentType::Attack:
	{
		UDamageAction* DamageAction = NewObject<UDamageAction>(ActionQueue.Get());
		DamageAction->Initialize(Enemy.Get(), Player.Get(), CommittedEnemyIntent.BaseAmount, EDamageKind::Attack);
		EnemyTurnBatch.Add(DamageAction);
		break;
	}
	default:
		ActionQueue->RequestResolutionFault(TEXT("Unsupported committed Enemy Intent reached EnemyTurn action construction."));
		return;
	}

	UObject* TurnEndedOuter = ActionQueue.Get();
#if WITH_DEV_AUTOMATION_TESTS
	if (bForceInvalidEnemyTurnBatchForTesting)
	{
		TurnEndedOuter = this;
		bForceInvalidEnemyTurnBatchForTesting = false;
	}
#endif

	UTurnEndedAction* TurnEndedAction = NewObject<UTurnEndedAction>(TurnEndedOuter);
	TurnEndedAction->Initialize(this, Enemy.Get());
	EnemyTurnBatch.Add(TurnEndedAction);

	if (!ActionQueue->AddBatchToBackPreserveOrder(EnemyTurnBatch))
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] StartEnemyTurn failed to enqueue the atomic enemy Intent action batch."));
		ActionQueue->RequestResolutionFault(TEXT("Enemy turn batch insertion failed before EnemyTurn state commit."));
		return;
	}

	BattleState = EBattleState::EnemyTurn;
	Energy = 0;
	Enemy->ClearBlock();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Enemy turn started from committed Intent: Type=%d BaseAmount=%d."),
		static_cast<int32>(CommittedEnemyIntent.Type),
		CommittedEnemyIntent.BaseAmount
	);

	if (!ActionQueue->StartProcessing())
	{
		ActionQueue->RequestResolutionFault(TEXT("Enemy turn batch was accepted but could not start processing."));
	}
}

void ABattleManager::CommitNextEnemyIntent()
{
	CommittedEnemyIntent = ChooseNextEnemyIntent();
	AdvanceStateRevision();
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Enemy Intent committed. Type=%d BaseAmount=%d Revision=%llu"),
		static_cast<int32>(CommittedEnemyIntent.Type),
		CommittedEnemyIntent.BaseAmount,
		StateRevision
	);
}

FEnemyIntent ABattleManager::ChooseNextEnemyIntent() const
{
	return FEnemyIntent::MakeAttack(EnemyTestAttackDamage);
}

void ABattleManager::HandleActionQueueEmpty()
{
	UE_LOG(LogTemp, Log, TEXT("[Battle] ActionQueue empty. Resolving post-queue battle flow. State=%d"), static_cast<int32>(BattleState));

	if (BattleState == EBattleState::ResolutionFaulted)
	{
		return;
	}

	CheckBattleResult();
	AdvanceStateRevision();
	if (BattleState == EBattleState::Victory || BattleState == EBattleState::Defeat || BattleState == EBattleState::ResolutionFaulted)
	{
		return;
	}

	if (!HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] QueueEmpty progression failed: ActionQueue is invalid."));
		return;
	}

	const TWeakObjectPtr<ABattleManager> WeakThis(this);
	bool bDeferred = true;

	switch (BattleState)
	{
	case EBattleState::BattleStart:
	case EBattleState::PlayerTurnStarting:
		bDeferred = ActionQueue->DeferUntilAfterQueueEmptyBroadcast(
			[WeakThis]()
			{
				if (ABattleManager* Battle = WeakThis.Get())
				{
					Battle->CompletePlayerTurnStart();
				}
			}
		);
		break;

	case EBattleState::PlayerTurnEnding:
		bDeferred = ActionQueue->DeferUntilAfterQueueEmptyBroadcast(
			[WeakThis]()
			{
				if (ABattleManager* Battle = WeakThis.Get())
				{
					Battle->StartEnemyTurn();
				}
			}
		);
		break;

	case EBattleState::EnemyTurnEnding:
		bDeferred = ActionQueue->DeferUntilAfterQueueEmptyBroadcast(
			[WeakThis]()
			{
				if (ABattleManager* Battle = WeakThis.Get())
				{
					Battle->CommitNextEnemyIntent();
					Battle->StartPlayerTurn();
				}
			}
		);
		break;

	default:
		return;
	}

	if (!bDeferred)
	{
		ActionQueue->RequestResolutionFault(TEXT("BattleManager failed to defer authoritative macro turn progression until after QueueEmpty observers returned."));
	}
}

void ABattleManager::HandleActionQueueResolutionFaulted(
	const FString& Reason,
	int32 ExecutedCount,
	UBattleAction* LastAction
)
{
#if WITH_DEV_AUTOMATION_TESTS
	StateBeforeLastResolutionFaultForTesting = BattleState;
#endif

	BattleState = EBattleState::ResolutionFaulted;
	Energy = 0;
	AdvanceStateRevision();

	UE_LOG(
		LogTemp,
		Error,
		TEXT("[Battle] Resolution faulted. Reason=%s Executed=%d LastAction=%s"),
		*Reason,
		ExecutedCount,
		*GetNameSafe(LastAction)
	);

	// Fault state is now fully committed at the battle layer. Publish it through
	// the same deferred UI boundary used by healthy Queue settlement.
	ScheduleReadStateReadyPublish();
}

void ABattleManager::HandleTurnEndedActionExecution(ACombatant* TurnOwner, UBattleActionQueue* Queue)
{
	if (!HasValidCombatants() || !HasValidActionQueue() || !HasValidEventDispatcher() ||
		!IsValid(TurnOwner) || Queue != ActionQueue.Get())
	{
		if (IsValid(Queue))
		{
			Queue->RequestResolutionFault(TEXT("TurnEndedAction reached BattleManager with invalid battle wiring."));
		}
		return;
	}

	if (Player->IsDead() || Enemy->IsDead())
	{
		UE_LOG(LogTemp, Log, TEXT("[Battle] TurnEndedAction skipped event dispatch because a combatant is dead. TurnOwner=%s"), *GetNameSafe(TurnOwner));
		return;
	}

	if (TurnOwner == Player.Get())
	{
		if (BattleState != EBattleState::PlayerTurnEnding)
		{
			Queue->RequestResolutionFault(TEXT("Player TurnEndedAction executed outside PlayerTurnEnding state."));
			return;
		}
	}
	else if (TurnOwner == Enemy.Get())
	{
		if (BattleState != EBattleState::EnemyTurn)
		{
			Queue->RequestResolutionFault(TEXT("Enemy TurnEndedAction executed outside EnemyTurn state."));
			return;
		}

		BattleState = EBattleState::EnemyTurnEnding;
	}
	else
	{
		Queue->RequestResolutionFault(TEXT("TurnEndedAction TurnOwner is not an authoritative battle combatant."));
		return;
	}

	TArray<ACombatant*> Combatants;
	Combatants.Add(Player.Get());
	Combatants.Add(Enemy.Get());

	if (!EventDispatcher->Dispatch(FBattleEvent::MakeTurnEnded(TurnOwner), Queue, Combatants))
	{
		Queue->RequestResolutionFault(TEXT("TurnEnded event dispatch failed during battle turn wiring."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] TurnEnded event dispatched. TurnOwner=%s State=%d"), *GetNameSafe(TurnOwner), static_cast<int32>(BattleState));
}

void ABattleManager::CheckBattleResult()
{
	if (BattleState == EBattleState::ResolutionFaulted || !HasValidCombatants())
	{
		return;
	}

	if (Enemy->IsDead())
	{
		BattleState = EBattleState::Victory;
		Energy = 0;
		UE_LOG(LogTemp, Log, TEXT("[Battle] Victory."));
		return;
	}

	if (Player->IsDead())
	{
		BattleState = EBattleState::Defeat;
		Energy = 0;
		UE_LOG(LogTemp, Log, TEXT("[Battle] Defeat."));
	}
}

FGameplayValidationResult ABattleManager::ValidatePlayerCommandBase() const
{
	if (!HasValidCombatants() || !HasValidActionQueue() || !HasValidDeckRuntime() || !HasValidEventDispatcher())
	{
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::InvalidBattle);
	}

	if (BattleState == EBattleState::ResolutionFaulted || ActionQueue->IsResolutionFaulted())
	{
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::ResolutionFaulted);
	}

	if (BattleState == EBattleState::Victory || BattleState == EBattleState::Defeat || Player->IsDead() || Enemy->IsDead())
	{
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::BattleEnded);
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::WrongTurn);
	}

	if (IsActionQueueBusy())
	{
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::ResolutionBusy);
	}

	return FGameplayValidationResult::Allowed();
}

FGameplayValidationResult ABattleManager::ValidateCardPlayBase(const UCardInstance* Card) const
{
	const FGameplayValidationResult Base = ValidatePlayerCommandBase();
	if (!Base.bAllowed)
	{
		return Base;
	}

	if (!IsValid(Card) || Card->GetDefinition() == nullptr)
	{
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::InvalidCard);
	}

	if (!DeckRuntime->IsCardInHand(Card))
	{
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::CardNoLongerInHand);
	}

	const int32 Cost = Card->GetCurrentCost();
	if (Cost < 0)
	{
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::InvalidCard);
	}

	if (!CanSpendEnergy(Cost))
	{
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::NotEnoughEnergy);
	}

	switch (Card->GetTargetType())
	{
	case ECardTargetType::None:
	case ECardTargetType::Self:
	case ECardTargetType::Enemy:
		return FGameplayValidationResult::Allowed();
	default:
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::InvalidTarget);
	}
}

FGameplayValidationResult ABattleManager::ValidatePlayCard(
	const UCardInstance* Card,
	const ACombatant* RequestedTarget
) const
{
	const FGameplayValidationResult Base = ValidateCardPlayBase(Card);
	if (!Base.bAllowed)
	{
		return Base;
	}

	switch (Card->GetTargetType())
	{
	case ECardTargetType::None:
		return RequestedTarget == nullptr
			? FGameplayValidationResult::Allowed()
			: FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::InvalidTarget);

	case ECardTargetType::Self:
		return RequestedTarget == Player.Get()
			? FGameplayValidationResult::Allowed()
			: FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::InvalidTarget);

	case ECardTargetType::Enemy:
		return RequestedTarget == Enemy.Get() && IsValid(Enemy.Get()) && !Enemy->IsDead()
			? FGameplayValidationResult::Allowed()
			: FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::InvalidTarget);

	default:
		return FGameplayValidationResult::Rejected(EGameplayRequestFailureReason::InvalidTarget);
	}
}

bool ABattleManager::BuildDrawActionBatch(int32 DrawCount, TArray<UBattleAction*>& OutActions)
{
	OutActions.Reset();
	if (DrawCount < 0 || !HasValidActionQueue() || !HasValidDeckRuntime() || !HasValidEventDispatcher())
	{
		return false;
	}

	UBattleEventDispatcher* Dispatcher = nullptr;
	TArray<ACombatant*> Combatants;
	if (!TryBuildEventDispatchContext(Dispatcher, Combatants))
	{
		return false;
	}

	OutActions.Reserve(DrawCount);
	for (int32 Index = 0; Index < DrawCount; ++Index)
	{
		UDrawCardAction* Action = NewObject<UDrawCardAction>(ActionQueue.Get());
		Action->Initialize(DeckRuntime.Get(), Dispatcher, Combatants);
		OutActions.Add(Action);
	}
	return true;
}

bool ABattleManager::BuildPlayerTurnEndBatch(TArray<UBattleAction*>& OutActions)
{
	OutActions.Reset();
	if (!HasValidActionQueue() || !HasValidDeckRuntime())
	{
		return false;
	}

	const TArray<TObjectPtr<UCardInstance>>& HandCards = DeckRuntime->GetHandCards();
	OutActions.Reserve(HandCards.Num() + 1);
	for (const TObjectPtr<UCardInstance>& Card : HandCards)
	{
		if (!IsValid(Card.Get()))
		{
			return false;
		}

		UDiscardCardAction* DiscardAction = NewObject<UDiscardCardAction>(ActionQueue.Get());
		DiscardAction->Initialize(DeckRuntime.Get(), Card.Get());
		OutActions.Add(DiscardAction);
	}

	UObject* TurnEndedOuter = ActionQueue.Get();
#if WITH_DEV_AUTOMATION_TESTS
	if (bForceInvalidPlayerEndBatchForTesting)
	{
		TurnEndedOuter = this;
		bForceInvalidPlayerEndBatchForTesting = false;
	}
#endif

	UTurnEndedAction* TurnEndedAction = NewObject<UTurnEndedAction>(TurnEndedOuter);
	TurnEndedAction->Initialize(this, Player.Get());
	OutActions.Add(TurnEndedAction);
	return true;
}

void ABattleManager::AdvanceStateRevision()
{
	++StateRevision;
	if (StateRevision == 0)
	{
		StateRevision = 1;
	}
}

void ABattleManager::QueueDamageAction(
	ACombatant* Source,
	ACombatant* Target,
	int32 BaseAmount,
	EDamageKind DamageKind
)
{
	if (!HasValidActionQueue())
	{
		return;
	}

	UDamageAction* Action = NewObject<UDamageAction>(ActionQueue.Get());
	Action->Initialize(Source, Target, BaseAmount, DamageKind);
	ActionQueue->AddToBack(Action);
}

void ABattleManager::QueueGainBlockAction(ACombatant* Source, ACombatant* Target, int32 BaseAmount)
{
	if (!HasValidActionQueue())
	{
		return;
	}

	UGainBlockAction* Action = NewObject<UGainBlockAction>(ActionQueue.Get());
	Action->Initialize(Source, Target, BaseAmount);
	ActionQueue->AddToBack(Action);
}

void ABattleManager::QueueDrawCardAction()
{
	if (!HasValidActionQueue() || !HasValidDeckRuntime())
	{
		return;
	}

	UDrawCardAction* Action = NewObject<UDrawCardAction>(ActionQueue.Get());
	UBattleEventDispatcher* Dispatcher = nullptr;
	TArray<ACombatant*> Combatants;
	if (TryBuildEventDispatchContext(Dispatcher, Combatants))
	{
		Action->Initialize(DeckRuntime.Get(), Dispatcher, Combatants);
	}
	else
	{
		Action->Initialize(DeckRuntime.Get());
	}
	ActionQueue->AddToBack(Action);
}

void ABattleManager::QueueDiscardCardAction(UCardInstance* Card)
{
	if (!HasValidActionQueue() || !HasValidDeckRuntime() || !IsValid(Card))
	{
		return;
	}

	UDiscardCardAction* Action = NewObject<UDiscardCardAction>(ActionQueue.Get());
	Action->Initialize(DeckRuntime.Get(), Card);
	ActionQueue->AddToBack(Action);
}

void ABattleManager::QueueApplyStatusAction(
	ACombatant* Source,
	ACombatant* Target,
	UStatusData* StatusDefinition,
	int32 AmountToAdd
)
{
	if (!HasValidActionQueue() || !IsValid(Target) || !IsValid(StatusDefinition) || AmountToAdd <= 0)
	{
		return;
	}

	UApplyStatusAction* Action = NewObject<UApplyStatusAction>(ActionQueue.Get());
	Action->Initialize(this, Source, Target, StatusDefinition, AmountToAdd);
	ActionQueue->AddToBack(Action);
}

bool ABattleManager::HasValidCombatants() const
{
	return IsValid(Player.Get()) && IsValid(Enemy.Get());
}

bool ABattleManager::HasValidActionQueue() const
{
	return IsValid(ActionQueue.Get());
}

bool ABattleManager::HasValidDeckRuntime() const
{
	return IsValid(DeckRuntime.Get());
}

bool ABattleManager::HasValidEventDispatcher() const
{
	return IsValid(EventDispatcher.Get());
}

bool ABattleManager::IsActionQueueBusy() const
{
	return HasValidActionQueue() && ActionQueue->IsBusy();
}
