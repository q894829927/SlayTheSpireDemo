#pragma once

#include "CoreMinimal.h"
#include "DamageModifier.h"
#include "DamageFlatAddModifier.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDamageFlatAddModifier : public UDamageModifier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage|Description")
	FName DescriptionArgumentName = FName(TEXT("DamageBonus"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage|FlatAdd")
	int32 Value = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage|FlatAdd")
	EModifierAmountMode AmountMode = EModifierAmountMode::ScaleWithAmount;

	virtual EDamageModifierPhase GetPhase() const override
	{
		return EDamageModifierPhase::FlatAdd;
	}

	virtual void Apply(const UStatusInstance* StatusInstance, FDamageSpec& Spec) const override;
	virtual void GetDescriptionArgumentNames(TArray<FName>& OutNames) const override;
	virtual void BuildDescriptionArguments(
		const UStatusInstance* StatusInstance,
		FPreviewTextArgumentBuilder& OutArguments
	) const override;
	virtual void ValidateDescriptionConfiguration(TArray<FText>& OutErrors) const override;
};
