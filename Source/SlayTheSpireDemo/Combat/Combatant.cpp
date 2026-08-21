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

FDamageCommitResult ACombatant::TakeCombatDamage(int32 Amount)
{
	FDamageCommitResult Result;
	if (Amount <= 0 || IsDead())
	{
		return Result;
	}

	Result.bCommitted = true;
	Result.IncomingDamage = Amount;
	Result.HPBefore = HP;
	Result.BlockBefore = Block;

	const int32 BlockedDamage = FMath::Min(Block, Amount);
	Block -= BlockedDamage;

	const int32 UnblockedDamage = Amount - BlockedDamage;
	if (UnblockedDamage > 0)
	{
		HP = FMath::Max(0, HP - UnblockedDamage);
	}

	Result.HPAfter = HP;
	Result.BlockAfter = Block;
	Result.BlockedDamage = Result.BlockBefore - Result.BlockAfter;
	Result.HPDamage = Result.HPBefore - Result.HPAfter;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[%s] took %d combat damage: blocked=%d hpDamage=%d HP=%d/%d Block=%d"),
		*GetName(),
		Amount,
		Result.BlockedDamage,
		Result.HPDamage,
		HP,
		MaxHP,
		Block
	);

	return Result;
}

FBlockCommitResult ACombatant::GainBlock(int32 Amount)
{
	FBlockCommitResult Result;
	if (Amount <= 0 || IsDead())
	{
		return Result;
	}

	Result.bCommitted = true;
	Result.BlockBefore = Block;
	Block += Amount;
	Result.BlockAfter = Block;
	Result.BlockDelta = Result.BlockAfter - Result.BlockBefore;

	UE_LOG(LogTemp, Log, TEXT("[%s] gained %d block: Block=%d"), *GetName(), Amount, Block);
	return Result;
}

FBlockCommitResult ACombatant::ClearBlock()
{
	FBlockCommitResult Result;
	if (Block == 0)
	{
		return Result;
	}

	Result.bCommitted = true;
	Result.BlockBefore = Block;
	UE_LOG(LogTemp, Log, TEXT("[%s] block cleared: %d -> 0"), *GetName(), Block);
	Block = 0;
	Result.BlockAfter = Block;
	Result.BlockDelta = Result.BlockAfter - Result.BlockBefore;
	return Result;
}

bool ACombatant::IsDead() const
{
	return HP <= 0;
}

UStatusContainer* ACombatant::GetStatusContainer() const
{
	return StatusContainer.Get();
}
