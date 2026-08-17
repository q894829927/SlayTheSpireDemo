#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../ModifierTypes.h"
#include "BlockModifier.generated.h"

struct FBlockSpec;
class UStatusInstance;

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UBlockModifier : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Block")
	EModifierScope Scope = EModifierScope::Target;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier|Block")
	int32 Priority = 0;

	bool IsApplicable(EModifierScope ContributionScope) const
	{
		return Scope == ContributionScope;
	}

	virtual EBlockModifierPhase GetPhase() const PURE_VIRTUAL(UBlockModifier::GetPhase, return EBlockModifierPhase::FlatAdd;);
	virtual void Apply(const UStatusInstance* StatusInstance, FBlockSpec& Spec) const PURE_VIRTUAL(UBlockModifier::Apply, );
};
