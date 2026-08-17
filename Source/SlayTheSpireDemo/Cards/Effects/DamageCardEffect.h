#pragma once

#include "CoreMinimal.h"
#include "CardEffect.h"
#include "../../Modifiers/ModifierTypes.h"
#include "DamageCardEffect.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UDamageCardEffect : public UCardEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect", meta = (ClampMin = "0"))
	int32 BaseAmount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect")
	EDamageKind DamageKind = EDamageKind::Attack;

	virtual void BuildActions(
		const FCardPlayContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;
};
