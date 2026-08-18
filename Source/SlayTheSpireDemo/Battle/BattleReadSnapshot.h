#pragma once

#include "CoreMinimal.h"
#include "../Cards/CardInstance.h"
#include "../Cards/CardTypes.h"
#include "../Combat/Combatant.h"
#include "../Enemy/EnemyIntent.h"

enum class EBattleState : uint8;

struct SLAYTHESPIREDEMO_API FStatusReadView
{
	FName StatusId = NAME_None;
	int32 Amount = 0;
	uint64 RuntimeSequence = 0;
};

struct SLAYTHESPIREDEMO_API FCombatantReadView
{
	TWeakObjectPtr<ACombatant> Combatant;
	int32 HP = 0;
	int32 MaxHP = 0;
	int32 Block = 0;
	bool bDead = true;
	TArray<FStatusReadView> Statuses;
};

struct SLAYTHESPIREDEMO_API FCardReadView
{
	TWeakObjectPtr<UCardInstance> Card;
	FName CardId = NAME_None;
	int32 RuntimeId = INDEX_NONE;
	int32 CurrentCost = 0;
	ECardTargetType TargetType = ECardTargetType::None;
};

// Gameplay-derived player-facing data for the committed Enemy Intent at the
// exact snapshot revision being observed. CurrentResolvedDamageAmount reuses the
// same Damage Modifier Pipeline as DamageAction, but it is intentionally a
// current-state value, not a promise about a future EnemyTurn after intervening
// TurnEnded reactions or other authoritative state changes.
struct SLAYTHESPIREDEMO_API FEnemyIntentPlayerFacingReadView
{
	bool bHasCurrentResolvedDamageAmount = false;
	int32 CurrentResolvedDamageAmount = 0;
};

struct SLAYTHESPIREDEMO_API FBattleReadSnapshot
{
	uint64 BattleId = 0;
	uint64 StateRevision = 0;
	EBattleState BattleState = static_cast<EBattleState>(0);
	int32 Energy = 0;
	int32 MaxEnergy = 0;

	FCombatantReadView Player;
	FCombatantReadView Enemy;

	// The committed authoritative action plan. BaseAmount remains the source used
	// later to build the EnemyTurn Action.
	FEnemyIntent EnemyIntent;

	// Player-facing value derived from the current snapshot state. UI must not
	// relabel this as guaranteed future damage unless a future gameplay predictor
	// explicitly models all mandatory pre-execution state transitions.
	FEnemyIntentPlayerFacingReadView EnemyIntentPlayerFacing;

	TArray<FCardReadView> HandCards;
	TArray<FCardReadView> DiscardCards;
	TArray<FCardReadView> ExhaustCards;

	int32 DrawCount = 0;
	int32 HandCount = 0;
	int32 DiscardCount = 0;
	int32 ExhaustCount = 0;
	int32 PlayAreaCount = 0;
};
