#pragma once

#include "CoreMinimal.h"
#include "../Events/BattleTrigger.h"
#include "RelicCountTrigger.generated.h"

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API URelicCountTrigger : public UBattleTrigger
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic|Trigger|Count", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	int32 GetRequiredCount() const { return RequiredCount; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
