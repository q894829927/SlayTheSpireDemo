#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../Events/BattleTrigger.h"
#include "RelicData.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class SLAYTHESPIREDEMO_API URelicData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Identity")
	FName RelicId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Presentation", meta = (MultiLine = "true"))
	FText Description;

	// Immutable presentation asset. Mutable Gameplay state remains on
	// URelicInstance and is never stored here.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Presentation")
	TObjectPtr<UTexture2D> Icon = nullptr;

	// Presentation choice only. Counter maximum is derived from the unique
	// URelicCountTrigger so Gameplay threshold has a single authored source.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Presentation")
	bool bShowCounter = false;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Relic|Triggers")
	TArray<TObjectPtr<UBattleTrigger>> Triggers;

	// Resolves the single authoritative Gameplay counter threshold. Returns false
	// when there is no count trigger, more than one count trigger, or an invalid
	// RequiredCount. This query never special-cases a concrete Relic or event type.
	bool TryGetCounterMax(int32& OutCounterMax) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
