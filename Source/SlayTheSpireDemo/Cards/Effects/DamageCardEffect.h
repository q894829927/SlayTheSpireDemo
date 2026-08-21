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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Description")
	FName DescriptionArgumentName = FName(TEXT("Damage"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect", meta = (ClampMin = "0"))
	int32 BaseAmount = 6;

	// Number of independent DamageActions built from this immutable effect
	// definition. Each hit resolves the current modifier pipeline separately.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect", meta = (ClampMin = "1"))
	int32 HitCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect")
	EDamageKind DamageKind = EDamageKind::Attack;

	virtual void BuildActions(
		const FCardPlayContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;
	virtual void GetPreviewArgumentNames(TArray<FName>& OutNames) const override;
	virtual void BuildPreviewArguments(
		const FCardEffectPreviewContext& Context,
		FPreviewTextArgumentBuilder& OutArguments
	) const override;
	virtual void ValidatePreviewConfiguration(TArray<FText>& OutErrors) const override;
};
