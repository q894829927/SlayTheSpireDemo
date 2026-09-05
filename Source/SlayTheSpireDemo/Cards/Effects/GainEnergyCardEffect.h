#pragma once

#include "CoreMinimal.h"
#include "CardEffect.h"
#include "GainEnergyCardEffect.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UGainEnergyCardEffect : public UCardEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Description")
	FName DescriptionArgumentName = FName(TEXT("Energy"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect", meta = (ClampMin = "0"))
	int32 BaseAmount = 1;

	// No sentinel/fallback semantics. If energy gain is unchanged by upgrade,
	// author the same explicit value here as BaseAmount.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Upgrade", meta = (ClampMin = "0"))
	int32 UpgradedAmount = 1;

	int32 GetEffectiveAmount(bool bIsUpgraded) const;

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
