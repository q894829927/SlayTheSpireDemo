#include "Battle/BattleManager.h"

#include "Combat/Combatant.h"

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

	Player->InitializeCombatant();
	Enemy->InitializeCombatant();

	BattleState = EBattleState::BattleStart;
	Energy = 0;

	UE_LOG(LogTemp, Log, TEXT("[Battle] Battle started."));
	StartPlayerTurn();
}

void ABattleManager::TestAttack()
{
	if (!HasValidCombatants())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestAttack failed: Player or Enemy reference is not assigned."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestAttack rejected: it is not the player's turn."));
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
	UE_LOG(LogTemp, Log, TEXT("[Battle] Player test attack: damage=%d energy=%d/%d"), PlayerTestAttackDamage, Energy, MaxEnergy);

	Enemy->TakeCombatDamage(PlayerTestAttackDamage);
	CheckBattleResult();
}

void ABattleManager::EndPlayerTurn()
{
	if (!HasValidCombatants())
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] EndPlayerTurn failed: Player or Enemy reference is not assigned."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] EndPlayerTurn rejected: it is not the player's turn."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] Player turn ended."));
	StartEnemyTurn();
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

	BattleState = EBattleState::EnemyTurn;
	Enemy->ClearBlock();

	UE_LOG(LogTemp, Log, TEXT("[Battle] Enemy turn started. Test attack damage=%d"), EnemyTestAttackDamage);
	Player->TakeCombatDamage(EnemyTestAttackDamage);

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

bool ABattleManager::HasValidCombatants() const
{
	return IsValid(Player.Get()) && IsValid(Enemy.Get());
}
