#pragma once

#include "CoreMinimal.h"
#include "CardEffect.h"
#include "SelectExhaustHandCardEffect.generated.h"

// Authored selection mode for the reusable Hand Select-Exhaust composition.
// Player opens the exact-N pending Selection UI path; Random resolves the same
// exact-N request synchronously through deterministic battle RNG with no UI.
UENUM(BlueprintType)
enum class ESelectExhaustSelectionMode : uint8
{
	Player,
	Random
};

// Composable "choose exactly N Hand cards, then exhaust them" Effect.
//
// C0 keeps the existing UCLASS identity for serialized Burning Pact
// compatibility while generalizing its authored Base/Upgraded configuration.
// Candidate discovery remains current Hand cards excluding Context.Card.
UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API USelectExhaustHandCardEffect : public UCardEffect
{
	GENERATED_BODY()

public:
	// Optional dynamic count argument. NAME_None is a deliberate legacy-compatible
	// opt-out so existing Burning Pact content with hard-coded "Exhaust 1 card"
	// text does not require a binary asset migration. New authored cards may set
	// this to e.g. "Exhaust" and reference {Exhaust} in their description.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Description")
	FName DescriptionArgumentName = NAME_None;

	// Optional dynamic selection-mode phrase argument. NAME_None preserves legacy
	// content. When authored, Random resolves to "随机消耗" and Player to "消耗",
	// using the effective Base/Upgraded selection mode.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Description")
	FName SelectionModeDescriptionArgumentName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect")
	ESelectExhaustSelectionMode BaseSelectionMode = ESelectExhaustSelectionMode::Player;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect", meta = (ClampMin = "0"))
	int32 BaseSelectionCount = 1;

	// No sentinel/fallback semantics. If upgrade leaves the mode unchanged,
	// author the same explicit value as BaseSelectionMode.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Upgrade")
	ESelectExhaustSelectionMode UpgradedSelectionMode = ESelectExhaustSelectionMode::Player;

	// No sentinel/fallback semantics. If upgrade leaves the count unchanged,
	// author the same explicit value as BaseSelectionCount.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Upgrade", meta = (ClampMin = "0"))
	int32 UpgradedSelectionCount = 1;

	ESelectExhaustSelectionMode GetEffectiveSelectionMode(bool bIsUpgraded) const;
	int32 GetEffectiveSelectionCount(bool bIsUpgraded) const;

	virtual void BuildActions(
		const FCardPlayContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;
	virtual void GetPreviewArgumentNames(TArray<FName>& OutNames) const override;
	virtual void BuildPreviewArguments(
		const FCardEffectPreviewContext& Context,
		FPreviewTextArgumentBuilder& OutArguments
	) const override;
	virtual void ValidatePreviewConfiguration(TArray<FText>& OutErrors) const override;
};
