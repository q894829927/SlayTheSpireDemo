#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../Events/BattleTrigger.h"
#include "RelicData.generated.h"

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

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Relic|Triggers")
	TArray<TObjectPtr<UBattleTrigger>> Triggers;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
