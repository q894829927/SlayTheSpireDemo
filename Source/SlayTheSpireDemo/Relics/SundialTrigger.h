#pragma once

#include "CoreMinimal.h"
#include "../Events/BattleTrigger.h"
#include "SundialTrigger.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API USundialTrigger : public UBattleTrigger
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Sundial", meta = (ClampMin = "1"))
	int32 ShufflesRequired = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Sundial", meta = (ClampMin = "1"))
	int32 EnergyGain = 2;

	virtual bool CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const override;
	virtual void BuildReactions(
		const FBattleEvent& Event,
		const FTriggerContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;
};
