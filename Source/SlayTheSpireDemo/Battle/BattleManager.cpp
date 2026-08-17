#include "BattleManager.h"

#include "../Actions/BattleActionQueue.h"
#include "../Actions/DamageAction.h"
#include "../Actions/DiscardCardAction.h"
#include "../Actions/DrawCardAction.h"
#include "../Actions/GainBlockAction.h"
#include "../Actions/PlayCardAction.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"

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

	ActionQueue = NewObject<UBattleActionQueue>(this);
	if (!HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] StartBattle failed: could not create ActionQueue."));
		return;
	}

	ActionQueue->OnQueueEmpty.AddUObject(this, &ABattleManager::HandleActionQueueEmpty);

	DeckRuntime = NewObject<UDeckRuntime>(this);
	if (!HasValidDeckRuntime())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] StartBattle failed: could not create DeckRuntime."));
		return;
	}
	DeckRuntime->InitializeFromDefinitions(DebugStartingDeck, DeckDebugSeed);

	if (DebugStartingDeck.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] DebugStartingDeck is empty. Configure Phase 4 CardData assets on BP_BattleManager."));
	}

	Player->InitializeCombatant();
	Enemy->InitializeCombatant();

	BattleState = EBattleState::BattleStart;
	Energy = 0;

	UE_LOG(LogTemp, Log, TEXT("[Battle] Battle started. ActionQueue and CardInstance DeckRuntime initialized."));
	StartPlayerTurn();
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

	QueueDamageAction(Player.Get(), Enemy.Get(), PlayerTestAttackDamage);
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

	QueueDamageAction(Player.Get(), Enemy.Get(), 7);
	QueueDamageAction(Player.Get(), Enemy.Get(), 8);

	UDamageAction* FrontAction = NewObject<UDamageAction>(ActionQueue.Get());
	FrontAction->Initialize(Player.Get(), Enemy.Get(), 6);
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

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPlayFirstCard rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPlayFirstCard rejected: action queue is busy."));
		return;
	}

	UCardInstance* CardToPlay = DeckRuntime->GetFirstHandCard();
	if (!IsValid(CardToPlay))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestPlayFirstCard skipped: Hand is empty."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Test play requested for %s."), *CardToPlay->GetDebugLabel());
	QueuePlayCardAction(CardToPlay, Enemy.Get());
	ActionQueue->StartProcessing();
}

void ABattleManager::EndPlayerTurn()
{
	if (!HasValidCombatants())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] EndPlayerTurn failed: Player or Enemy reference is not assigned."));
		return;
	}

	if (!HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] EndPlayerTurn failed: ActionQueue is not initialized."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] EndPlayerTurn rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] EndPlayerTurn rejected: action queue is busy."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Player turn ended."));
	StartEnemyTurn();
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

void ABattleManager::StartPlayerTurn()
{
	if (!HasValidCombatants() || Player->IsDead() || Enemy->IsDead())
	{
		CheckBattleResult();
		return;
	}

	BattleState = EBattleState::PlayerTurn;
	Energy = MaxEnergy;
	Player->ClearBlock();

	UE_LOG(LogTemp, Log, TEXT("[Battle] Player turn started. Energy=%d/%d"), Energy, MaxEnergy);
}

void ABattleManager::StartEnemyTurn()
{
	if (!HasValidCombatants() || Player->IsDead() || Enemy->IsDead())
	{
		CheckBattleResult();
		return;
	}

	if (!HasValidActionQueue())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] StartEnemyTurn failed: ActionQueue is not initialized."));
		return;
	}

	BattleState = EBattleState::EnemyTurn;
	Enemy->ClearBlock();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] Enemy turn started. Queueing DamageAction BaseAmount=%d"),
		EnemyTestAttackDamage
	);

	QueueDamageAction(Enemy.Get(), Player.Get(), EnemyTestAttackDamage);
	ActionQueue->StartProcessing();
}

void ABattleManager::HandleActionQueueEmpty()
{
	UE_LOG(LogTemp, Log, TEXT("[Battle] ActionQueue empty. Resolving post-queue battle flow."));

	CheckBattleResult();

	if (BattleState == EBattleState::EnemyTurn)
	{
		StartPlayerTurn();
	}
}

void ABattleManager::CheckBattleResult()
{
	if (!HasValidCombatants())
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

void ABattleManager::QueueDamageAction(ACombatant* Source, ACombatant* Target, int32 BaseAmount)
{
	if (!HasValidActionQueue())
	{
		return;
	}

	UDamageAction* Action = NewObject<UDamageAction>(ActionQueue.Get());
	Action->Initialize(Source, Target, BaseAmount);
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
	Action->Initialize(DeckRuntime.Get());
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

void ABattleManager::QueuePlayCardAction(UCardInstance* Card, ACombatant* Target)
{
	if (!HasValidActionQueue() || !HasValidDeckRuntime() || !IsValid(Card))
	{
		return;
	}

	UPlayCardAction* Action = NewObject<UPlayCardAction>(ActionQueue.Get());
	Action->Initialize(this, Card, Player.Get(), Target, DeckRuntime.Get());
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

bool ABattleManager::IsActionQueueBusy() const
{
	return HasValidActionQueue() && ActionQueue->IsBusy();
}
