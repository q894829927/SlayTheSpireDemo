#pragma once

#include "CoreMinimal.h"

class URelicInstance;

enum class ERelicAddOutcome : uint8
{
	Invalid,
	Duplicate,
	Added
};

struct SLAYTHESPIREDEMO_API FRelicAddResult
{
	ERelicAddOutcome Outcome = ERelicAddOutcome::Invalid;
	URelicInstance* Instance = nullptr;

	bool WasAdded() const
	{
		return Outcome == ERelicAddOutcome::Added && Instance != nullptr;
	}
};
