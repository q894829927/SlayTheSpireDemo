#pragma once

#include "CoreMinimal.h"
#include "CardEffect.h"
#include "DrawCardEffect.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDrawCardEffect : public UCardEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect", meta = (ClampMin = "0"))
	int32 DrawCount = 1;

	virtual void BuildActions(
		const FCardPlayContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;
};
