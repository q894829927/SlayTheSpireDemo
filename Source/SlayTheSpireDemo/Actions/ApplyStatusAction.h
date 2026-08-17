#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "ApplyStatusAction.generated.h"

class ABattleManager;
class ACombatant;
class UStatusData;

UCLASS()
class SLAYTHESPIREDEMO_API UApplyStatusAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(
		ABattleManager* InBattle,
		ACombatant* InSource,
		ACombatant* InTarget,
		UStatusData* InStatusDefinition,
		int32 InAmountToAdd
	);

	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ABattleManager> Battle = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Source = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Target = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStatusData> StatusDefinition = nullptr;

	int32 AmountToAdd = 0;
};
