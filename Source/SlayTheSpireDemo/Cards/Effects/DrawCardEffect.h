#pragma once

#include "CoreMinimal.h"
#include "CardEffect.h"
#include "DrawCardEffect.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDrawCardEffect : public UCardEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Description")
	FName DescriptionArgumentName = FName(TEXT("Draw"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect", meta = (ClampMin = "0"))
	int32 DrawCount = 1;

	// No sentinel/fallback semantics. If draw count is unchanged by upgrade,
	// author the same explicit value here as DrawCount.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Upgrade", meta = (ClampMin = "0"))
	int32 UpgradedDrawCount = 1;

	int32 GetEffectiveDrawCount(bool bIsUpgraded) const;

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
