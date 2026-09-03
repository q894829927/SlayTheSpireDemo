#pragma once

#include "CoreMinimal.h"
#include "BattleState.h"
#include "../Cards/CardInstance.h"
#include "../Cards/CardTypes.h"
#include "../Combat/Combatant.h"
#include "../Enemy/EnemyIntent.h"

class URelicData;
class URelicInstance;
class UStatusData;
class UStatusInstance;

struct SLAYTHESPIREDEMO_API FStatusReadView
{
	TWeakObjectPtr<UStatusInstance> Status;
	TWeakObjectPtr<UStatusData> Definition;
	FName StatusId = NAME_None;
	int32 Amount = 0;
	uint64 RuntimeSequence = 0;

	// Populated by TryBuildPlayerFacingReadSnapshot for this exact revision.
	FText CurrentDescription;
};

struct SLAYTHESPIREDEMO_API FRelicReadView
{
	// Read-only observation handles. Frozen Presentation must not retain either
	// mutable runtime pointer; it copies immutable definition fields and scalar
	// runtime facts into FBattleHUDRelicView.
	TWeakObjectPtr<URelicInstance> Relic;
	TWeakObjectPtr<URelicData> Definition;
	FName RelicId = NAME_None;
	uint64 RuntimeSequence = 0;
	int32 Counter = 0;
};

struct SLAYTHESPIREDEMO_API FCombatantReadView
{
	TWeakObjectPtr<ACombatant> Combatant;
	FName PresentationId = NAME_None;
	FText DisplayName;
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

	// Source-side deterministic baseline populated by the player-facing read
	// snapshot. Enemy target modifiers are intentionally excluded.
	FText CurrentDescription;

	// Same current-state facts as CurrentDescription, with comparison RichText
	// tags attached only to Damage/Block values that differ from authored base.
	// This is presentation markup, not a second Gameplay calculation.
	FText CurrentRichDescription;
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
	EBattleState BattleState = EBattleState::BattleStart;
	int32 Energy = 0;
	int32 MaxEnergy = 0;

	FCombatantReadView Player;
	FCombatantReadView Enemy;

	// Ordered player-owned Relic runtime facts for this exact read revision.
	TArray<FRelicReadView> Relics;

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
