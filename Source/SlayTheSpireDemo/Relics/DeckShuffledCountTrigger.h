#pragma once

#include "CoreMinimal.h"
#include "../Events/BattleTrigger.h"
#include "DeckShuffledCountTrigger.generated.h"

class URelicEffect;

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDeckShuffledCountTrigger : public UBattleTrigger
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Trigger|Count", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Relic|Trigger|Effects")
	TArray<TObjectPtr<URelicEffect>> Effects;

	virtual bool CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const override;
	virtual void BuildReactions(
		const FBattleEvent& Event,
		const FTriggerContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
