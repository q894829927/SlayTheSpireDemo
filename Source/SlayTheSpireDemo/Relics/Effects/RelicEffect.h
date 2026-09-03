#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RelicEffect.generated.h"

class ABattleManager;
class ACombatant;
class UBattleAction;
class URelicInstance;

struct SLAYTHESPIREDEMO_API FRelicEffectContext
{
	URelicInstance* Relic = nullptr;
	ABattleManager* Battle = nullptr;
	ACombatant* Owner = nullptr;
	FName OwnerPresentationId = NAME_None;
	UObject* ActionOuter = nullptr;
};

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API URelicEffect : public UObject
{
	GENERATED_BODY()

public:
	virtual bool BuildActions(
		const FRelicEffectContext& Context,
		TArray<UBattleAction*>& OutActions
	) const PURE_VIRTUAL(URelicEffect::BuildActions, return false;);
};
