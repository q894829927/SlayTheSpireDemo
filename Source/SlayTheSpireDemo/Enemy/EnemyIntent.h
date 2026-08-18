#pragma once

#include "CoreMinimal.h"

enum class EEnemyIntentType : uint8
{
	None,
	Attack
};

struct SLAYTHESPIREDEMO_API FEnemyIntent
{
	EEnemyIntentType Type = EEnemyIntentType::None;
	int32 BaseAmount = 0;

	static FEnemyIntent MakeAttack(int32 InBaseAmount)
	{
		FEnemyIntent Intent;
		Intent.Type = EEnemyIntentType::Attack;
		Intent.BaseAmount = FMath::Max(0, InBaseAmount);
		return Intent;
	}

	bool IsCommitted() const
	{
		return Type != EEnemyIntentType::None;
	}
};
