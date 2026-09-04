#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "../Cards/CardTypes.h"
#include "BattleHUDTypes.generated.h"

UENUM(BlueprintType)
enum class EBattleHUDInteractionState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	ChoosingTarget UMETA(DisplayName = "Choosing Target"),
	ReadyToConfirm UMETA(DisplayName = "Ready To Confirm"),
	Resolving UMETA(DisplayName = "Resolving"),
	Terminal UMETA(DisplayName = "Terminal"),
	PresentationUnavailable UMETA(DisplayName = "Presentation Unavailable")
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
	int64 RuntimeSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 Amount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	bool bUseAtlasIcon = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FVector2D UVOffset = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FVector2D UVScale = FVector2D::UnitVector;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FVector2D TrimOffset = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FVector2D TrimScale = FVector2D::UnitVector;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FBattleHUDRelicView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Relic")
	FName RelicId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Relic")
	int64 RuntimeSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Relic")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Relic")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Relic")
	bool bShowCounter = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Relic")
	int32 Counter = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Relic")
	int32 CounterMax = 0;

	// Immutable presentation asset reference only. No URelicInstance or other
	// mutable Gameplay runtime pointer is permitted in the frozen HUD DTO.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Relic")
	TObjectPtr<UTexture2D> Icon = nullptr;
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FBattleHUDCombatantView
{
	GENERATED_BODY()

	// Presentation-only stable key used to match combatant widgets with the
	// gameplay-provided legal-target set. It is never a gameplay ordering key.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FName PresentationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	bool bPlayer = false;

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

	// Phase 7 Relics are player-owned. The Enemy view keeps this empty. Keeping
	// the frozen Relic collection on the Player HUD view lets existing
	// FPresentationStateSnapshot/ViewModel value-copy boundaries carry it without
	// introducing mutable Gameplay ownership into Presentation.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD|Relic")
	TArray<FBattleHUDRelicView> Relics;
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

	// Frozen presentation fact. The visible name text remains the authored
	// DisplayName; presentation may style upgraded cards differently.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	bool bUpgraded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	int32 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	ECardType CardType = ECardType::Attack;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	ECardTargetType TargetType = ECardTargetType::None;

	// Plain semantic card description retained for non-RichText consumers and
	// immutable contract comparisons.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FText Description;

	// Optional Native RichText version of Description. When empty, presentation
	// falls back to Description. Only semantic numeric values carry style tags.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FText RichDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	TObjectPtr<UTexture2D> CardArt = nullptr;

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

	// Matches FBattleHUDCombatantView.PresentationId so the View can place this
	// legal target affordance over the correct combatant presentation.
	UPROPERTY(BlueprintReadOnly, Category = "Battle HUD")
	FName PresentationId = NAME_None;

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
