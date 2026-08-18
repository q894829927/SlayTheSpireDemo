#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BattleTrigger.generated.h"

class ACombatant;
class UBattleAction;
class UStatusInstance;
struct FBattleEvent;

struct FTriggerContext
{
public:
	FTriggerContext(UStatusInstance* InRuntimeSource, UObject* InActionOuter);

	UStatusInstance* GetRuntimeSource() const;
	ACombatant* GetOwner() const;
	UObject* GetActionOuter() const;

private:
	UStatusInstance* RuntimeSource = nullptr;
	ACombatant* Owner = nullptr;
	UObject* ActionOuter = nullptr;
};

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UBattleTrigger : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	int32 Priority = 0;

	virtual bool CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const;
	virtual void BuildReactions(
		const FBattleEvent& Event,
		const FTriggerContext& Context,
		TArray<UBattleAction*>& OutActions
	) const;
};
