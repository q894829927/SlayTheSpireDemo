#pragma once

#include "CoreMinimal.h"

struct FBlockSpec;

class SLAYTHESPIREDEMO_API FBlockModifierPipeline
{
public:
	static void Resolve(FBlockSpec& Spec);
};
