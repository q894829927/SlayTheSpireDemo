#pragma once

#include "CoreMinimal.h"
#include "BlockModifier.h"
#include "BlockFlatAddModifier.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UBlockFlatAddModifier : public UBlockModifier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Block|FlatAdd")
	int32 Value = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Block|FlatAdd")
	EModifierAmountMode AmountMode = EModifierAmountMode::ScaleWithAmount;

	virtual EBlockModifierPhase GetPhase() const override
	{
		return EBlockModifierPhase::FlatAdd;
	}

	virtual void Apply(const UStatusInstance* StatusInstance, FBlockSpec& Spec) const override;
};
