#pragma once

#include "CoreMinimal.h"
#include "../ModifierTypes.h"

class ACombatant;

struct FDamageSpec
{
	ACombatant* Source = nullptr;
	ACombatant* Target = nullptr;
	EDamageKind DamageKind = EDamageKind::Attack;
	int32 BaseAmount = 0;
	int32 WorkingAmount = 0;
	int32 ResolvedAmount = 0;
};
