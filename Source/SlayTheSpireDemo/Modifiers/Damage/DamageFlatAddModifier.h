#pragma once

#include "CoreMinimal.h"
#include "DamageModifier.h"
#include "DamageFlatAddModifier.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDamageFlatAddModifier : public UDamageModifier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage|FlatAdd")
	int32 Value = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage|FlatAdd")
	EModifierAmountMode AmountMode = EModifierAmountMode::ScaleWithAmount;

	virtual EDamageModifierPhase GetPhase() const override
	{
		return EDamageModifierPhase::FlatAdd;
	}

	virtual void Apply(const UStatusInstance* StatusInstance, FDamageSpec& Spec) const override;
};
