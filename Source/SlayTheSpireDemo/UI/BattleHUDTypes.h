#pragma once

#include "CoreMinimal.h"
#include "../Cards/CardTypes.h"
#include "BattleHUDTypes.generated.h"

UENUM(BlueprintType)
enum class EBattleHUDInteractionState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	ChoosingTarget UMETA(DisplayName = "Choosing Target"),
	ReadyToConfirm UMETA(DisplayName = "Ready To Confirm"),
	Resolving UMETA(DisplayName = "Resolving"),
	Terminal UMETA(DisplayName = "Terminal")
};

UENUM(BlueprintType)
enum class EBattleHUDOutcome : uint8
{
	None UMETA(DisplayName = "None"),
	Victory UMETA(DisplayName = "Victory"),
	Defeat UMETA(DisplayName = "Defeat"),
	ResolutionFaulted UMETA(DisplayName = "Resolution Faulted")
};

UENUM(BlueprintType)
enum class EBattleHUDIntentType : uint8
{
	None UMETA(DisplayName = "None"),
	Attack UMETA(DisplayName = "Attack")
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FBattleHUDStatusView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FName StatusId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 Amount = 0;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FBattleHUDCombatantView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 HP = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 MaxHP = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 Block = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	bool bDead = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	TArray<FBattleHUDStatusView> Statuses;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FBattleHUDCardView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 RuntimeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	ECardType CardType = ECardType::Attack;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	ECardTargetType TargetType = ECardTargetType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	bool bGameplayPlayable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FText UnplayableReason;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FBattleHUDTargetView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 TargetId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	bool bPlayer = false;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FBattleHUDIntentView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	EBattleHUDIntentType Type = EBattleHUDIntentType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FText DisplayName;

	// Committed authoritative plan input. Normal HUD should prefer the explicit
	// player-facing current value below when one is available.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 BaseAmount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	bool bHasCurrentResolvedDamageAmount = false;

	// Current-snapshot gameplay-derived value only. This is not a guaranteed
	// future EnemyTurn result after intervening TurnEnded reactions.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 CurrentResolvedDamageAmount = 0;
};
