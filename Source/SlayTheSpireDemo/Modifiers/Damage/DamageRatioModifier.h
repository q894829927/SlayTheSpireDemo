#pragma once

#include "CoreMinimal.h"
#include "DamageModifier.h"
#include "DamageRatioModifier.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDamageRatioModifier : public UDamageModifier
{
	GENERATED_BODY()

public:
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
};
