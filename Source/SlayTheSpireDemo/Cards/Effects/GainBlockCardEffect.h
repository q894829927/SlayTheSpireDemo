#pragma once

#include "CoreMinimal.h"
#include "CardEffect.h"
#include "GainBlockCardEffect.generated.h"

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UGainBlockCardEffect : public UCardEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Description")
	FName DescriptionArgumentName = FName(TEXT("Block"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect", meta = (ClampMin = "0"))
	int32 BaseAmount = 5;

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
	virtual void BuildImmediatePreviewOperations(
		const FCardEffectPreviewContext& Context,
		int32 EffectIndex,
		TArray<FImmediatePreviewOperation>& OutOperations
	) const override;
};
