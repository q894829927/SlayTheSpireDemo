#pragma once

#include "CoreMinimal.h"
#include "CardEffect.h"
#include "ApplyStatusCardEffect.generated.h"

class UStatusData;

UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UApplyStatusCardEffect : public UCardEffect
{
	GENERATED_BODY()

public:
	// Must be unique within the owning card, e.g. Weak / Vulnerable.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect|Description")
	FName DescriptionArgumentName = FName(TEXT("StatusAmount"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect")
	TObjectPtr<UStatusData> StatusDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Effect", meta = (ClampMin = "1"))
	int32 Amount = 1;

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
