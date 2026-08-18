#pragma once

#include "CoreMinimal.h"
#include "BattleTrigger.h"
#include "TurnEndStatusDecayTrigger.generated.h"

UCLASS(EditInlineNew)
class SLAYTHESPIREDEMO_API UTurnEndStatusDecayTrigger : public UBattleTrigger
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|TurnEnd")
	int32 AmountToRemove = 1;

	virtual bool CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const override;
	virtual void BuildReactions(
		const FBattleEvent& Event,
		const FTriggerContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;
};
