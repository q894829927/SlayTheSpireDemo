#pragma once

#include "CoreMinimal.h"
#include "RelicCountTrigger.h"
#include "DeckShuffledCountTrigger.generated.h"

class URelicEffect;

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDeckShuffledCountTrigger : public URelicCountTrigger
{
	GENERATED_BODY()

public:
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
