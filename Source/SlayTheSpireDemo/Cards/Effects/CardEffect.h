#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CardEffect.generated.h"

class UBattleAction;
struct FCardPlayContext;

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UCardEffect : public UObject
{
	GENERATED_BODY()

public:
	virtual void BuildActions(
		const FCardPlayContext& Context,
		TArray<UBattleAction*>& OutActions
	) const PURE_VIRTUAL(UCardEffect::BuildActions, );
};
