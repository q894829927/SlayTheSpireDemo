#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../ModifierTypes.h"
#include "DamageModifier.generated.h"

struct FDamageSpec;
class UStatusInstance;
class FPreviewTextArgumentBuilder;

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDamageModifier : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage")
	EModifierScope Scope = EModifierScope::Source;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Damage")
	EDamageKind ApplicableDamageKind = EDamageKind::Attack;

	bool IsApplicable(EDamageKind DamageKind, EModifierScope ContributionScope) const;
	virtual EDamageModifierPhase GetPhase() const PURE_VIRTUAL(UDamageModifier::GetPhase, return EDamageModifierPhase::FlatAdd;);
	virtual void Apply(const UStatusInstance* StatusInstance, FDamageSpec& Spec) const PURE_VIRTUAL(UDamageModifier::Apply, );
	virtual void GetDescriptionArgumentNames(TArray<FName>& OutNames) const
		PURE_VIRTUAL(UDamageModifier::GetDescriptionArgumentNames, );
	virtual void BuildDescriptionArguments(
		const UStatusInstance* StatusInstance,
		FPreviewTextArgumentBuilder& OutArguments
	) const PURE_VIRTUAL(UDamageModifier::BuildDescriptionArguments, );
	virtual void ValidateDescriptionConfiguration(TArray<FText>& OutErrors) const
		PURE_VIRTUAL(UDamageModifier::ValidateDescriptionConfiguration, );
};
