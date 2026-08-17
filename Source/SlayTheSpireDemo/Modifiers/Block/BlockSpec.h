#pragma once

#include "CoreMinimal.h"

class ACombatant;

struct FBlockSpec
{
	ACombatant* Source = nullptr;
	ACombatant* Target = nullptr;
	int32 BaseAmount = 0;
	int32 WorkingAmount = 0;
	int32 ResolvedAmount = 0;
};
