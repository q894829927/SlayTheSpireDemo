#pragma once

#include "CoreMinimal.h"

struct FDamageSpec;

class SLAYTHESPIREDEMO_API FDamageModifierPipeline
{
public:
	static void Resolve(FDamageSpec& Spec);
};
