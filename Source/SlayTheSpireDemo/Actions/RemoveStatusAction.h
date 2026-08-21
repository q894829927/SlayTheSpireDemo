#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "RemoveStatusAction.generated.h"

class ABattleManager;
class ACombatant;
class UStatusInstance;

UCLASS()
class SLAYTHESPIREDEMO_API URemoveStatusAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(
		ABattleManager* InBattle,
		ACombatant* InSource,
		ACombatant* InTarget,
		UStatusInstance* InExpectedInstance
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
	TObjectPtr<UStatusInstance> ExpectedInstance = nullptr;
};
