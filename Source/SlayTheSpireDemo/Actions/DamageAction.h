#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "../Modifiers/ModifierTypes.h"
#include "DamageAction.generated.h"

class ACombatant;

UCLASS()
class SLAYTHESPIREDEMO_API UDamageAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(ACombatant* InSource, ACombatant* InTarget, int32 InBaseAmount, EDamageKind InDamageKind);
	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Source = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Target = nullptr;

	int32 BaseAmount = 0;
	EDamageKind DamageKind = EDamageKind::Attack;
};
