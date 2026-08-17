#pragma once

#include "CoreMinimal.h"
#include "BlockModifier.h"
#include "BlockRatioModifier.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UBlockRatioModifier : public UBlockModifier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Block|Ratio", meta = (ClampMin = "0"))
	int32 Numerator = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Block|Ratio", meta = (ClampMin = "1"))
	int32 Denominator = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Block|Ratio")
	EModifierAmountMode AmountMode = EModifierAmountMode::PresenceOnly;

	virtual EBlockModifierPhase GetPhase() const override
	{
		return EBlockModifierPhase::Multiplier;
	}

	virtual void Apply(const UStatusInstance* StatusInstance, FBlockSpec& Spec) const override;
};
