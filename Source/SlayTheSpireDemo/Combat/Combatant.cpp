#include "Combatant.h"

#include "../Status/StatusContainer.h"

ACombatant::ACombatant()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACombatant::InitializeCombatant()
{
	HP = FMath::Max(1, MaxHP);
	Block = 0;

	StatusContainer = NewObject<UStatusContainer>(this);
	if (IsValid(StatusContainer.Get()))
	{
		StatusContainer->Initialize(this);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] failed to create StatusContainer."), *GetName());
	}

	UE_LOG(LogTemp, Log, TEXT("[%s] initialized: HP=%d/%d Block=%d"), *GetName(), HP, MaxHP, Block);
}

void ACombatant::TakeCombatDamage(int32 Amount)
{
	if (Amount <= 0 || IsDead())
	{
		return;
	}

	const int32 BlockedDamage = FMath::Min(Block, Amount);
	Block -= BlockedDamage;

	const int32 HPDamage = Amount - BlockedDamage;
	if (HPDamage > 0)
	{
		HP = FMath::Max(0, HP - HPDamage);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[%s] took %d combat damage: blocked=%d hpDamage=%d HP=%d/%d Block=%d"),
		*GetName(),
		Amount,
		BlockedDamage,
		HPDamage,
		HP,
		MaxHP,
		Block
	);
}

void ACombatant::GainBlock(int32 Amount)
{
	if (Amount <= 0 || IsDead())
	{
		return;
	}

	Block += Amount;
	UE_LOG(LogTemp, Log, TEXT("[%s] gained %d block: Block=%d"), *GetName(), Amount, Block);
}

void ACombatant::ClearBlock()
{
	if (Block == 0)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[%s] block cleared: %d -> 0"), *GetName(), Block);
	Block = 0;
}

bool ACombatant::IsDead() const
{
	return HP <= 0;
}

UStatusContainer* ACombatant::GetStatusContainer() const
{
	return StatusContainer.Get();
}
