#pragma once

#include "CoreMinimal.h"
#include "RelicEffect.h"
#include "GainBlockRelicEffect.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UGainBlockRelicEffect : public URelicEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Effect|Block", meta = (ClampMin = "1"))
	int32 Amount = 1;

	virtual bool BuildActions(
		const FRelicEffectContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
