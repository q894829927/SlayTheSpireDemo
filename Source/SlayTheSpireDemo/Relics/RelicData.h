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

	// Generic presentation metadata. Widgets consume the frozen HUD DTO only and
	// never special-case RelicId (for example, "Sundial") to decide counter UI.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Presentation")
	bool bShowCounter = false;

	// Presentation-only display maximum. It does not drive Gameplay mutation or
	// trigger thresholds. Authored content must keep it aligned with the mechanic
	// it is describing when bShowCounter is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Presentation", meta = (EditCondition = "bShowCounter", ClampMin = "1"))
	int32 CounterDisplayMax = 0;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Relic|Triggers")
	TArray<TObjectPtr<UBattleTrigger>> Triggers;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
