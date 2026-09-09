#pragma once

#include "CoreMinimal.h"
#include "CardEffect.h"
#include "SelectHandCardToDrawPileTopEffect.generated.h"

// Reusable authored Effect: choose exactly N cards from the current Hand at
// execution time, then move those exact CardInstances to DrawPile top.
UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API USelectHandCardToDrawPileTopEffect : public UCardEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Description")
	FName DescriptionArgumentName = FName(TEXT("DrawPileTopCount"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect", meta = (ClampMin = "0"))
	int32 BaseSelectionCount = 1;

	// Explicit upgrade value; no sentinel/fallback semantics.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Upgrade", meta = (ClampMin = "0"))
	int32 UpgradedSelectionCount = 1;

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
	virtual FText GetAutoDescriptionFormat(const FCardEffectPreviewContext& Context) const override;
	virtual void BuildAutoDescriptionArguments(
		const FCardEffectPreviewContext& Context,
		FPreviewTextArgumentBuilder& OutArguments
	) const override;
	virtual void ValidatePreviewConfiguration(TArray<FText>& OutErrors) const override;
};
