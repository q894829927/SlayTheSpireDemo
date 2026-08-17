#pragma once

#include "CoreMinimal.h"
#include "ModifierTypes.generated.h"

UENUM(BlueprintType)
enum class EDamageKind : uint8
{
	Attack UMETA(DisplayName = "Attack"),
	Effect UMETA(DisplayName = "Effect")
};

UENUM(BlueprintType)
enum class EModifierScope : uint8
{
	Source UMETA(DisplayName = "Source"),
	Target UMETA(DisplayName = "Target")
};

UENUM(BlueprintType)
enum class EModifierAmountMode : uint8
{
	PresenceOnly UMETA(DisplayName = "Presence Only"),
	ScaleWithAmount UMETA(DisplayName = "Scale With Amount")
};

UENUM(BlueprintType)
enum class EDamageModifierPhase : uint8
{
	FlatAdd UMETA(DisplayName = "Flat Add")
};

inline const TCHAR* DamageKindToString(EDamageKind DamageKind)
{
	switch (DamageKind)
	{
	case EDamageKind::Attack:
		return TEXT("Attack");
	case EDamageKind::Effect:
		return TEXT("Effect");
	default:
		return TEXT("Unknown");
	}
}
