#pragma once

#include "CoreMinimal.h"
#include "BattleState.generated.h"

UENUM(BlueprintType)
enum class EBattleState : uint8
{
	BattleStart UMETA(DisplayName = "Battle Start"),
	PlayerTurnStarting UMETA(DisplayName = "Player Turn Starting"),
	PlayerTurn UMETA(DisplayName = "Player Turn"),
	PlayerTurnEnding UMETA(DisplayName = "Player Turn Ending"),
	EnemyTurn UMETA(DisplayName = "Enemy Turn"),
	EnemyTurnEnding UMETA(DisplayName = "Enemy Turn Ending"),
	Victory UMETA(DisplayName = "Victory"),
	Defeat UMETA(DisplayName = "Defeat"),
	ResolutionFaulted UMETA(DisplayName = "Resolution Faulted")
};
