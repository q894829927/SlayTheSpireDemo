#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "TurnEndedAction.generated.h"

class ABattleManager;
class ACombatant;

UCLASS()
class SLAYTHESPIREDEMO_API UTurnEndedAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(ABattleManager* InBattle, ACombatant* InTurnOwner);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ABattleManager> Battle = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> TurnOwner = nullptr;
};
