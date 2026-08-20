#pragma once

#include "CoreMinimal.h"
#include "DamageModifier.h"
#include "DamageRatioModifier.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDamageRatioModifier : public UDamageModifier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage|Description")
	FName DescriptionArgumentName = FName(TEXT("DamagePercent"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage|Ratio")
	EDamageModifierPhase Phase = EDamageModifierPhase::SourceMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage|Ratio", meta = (ClampMin = "0"))
	int32 Numerator = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage|Ratio", meta = (ClampMin = "1"))
	int32 Denominator = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage|Ratio")
	EModifierAmountMode AmountMode = EModifierAmountMode::PresenceOnly;

	virtual EDamageModifierPhase GetPhase() const override
	{
		return Phase;
	}

	virtual void Apply(const UStatusInstance* StatusInstance, FDamageSpec& Spec) const override;
	virtual void GetDescriptionArgumentNames(TArray<FName>& OutNames) const override;
	virtual void BuildDescriptionArguments(
		const UStatusInstance* StatusInstance,
		FPreviewTextArgumentBuilder& OutArguments
	) const override;
	virtual void ValidateDescriptionConfiguration(TArray<FText>& OutErrors) const override;
};
