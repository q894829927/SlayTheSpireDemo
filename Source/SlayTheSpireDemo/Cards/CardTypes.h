#pragma once

#include "CoreMinimal.h"
#include "CardTypes.generated.h"

UENUM(BlueprintType)
enum class ECardType : uint8
{
	Attack UMETA(DisplayName = "Attack"),
	Skill UMETA(DisplayName = "Skill"),
	Power UMETA(DisplayName = "Power"),
	Status UMETA(DisplayName = "Status"),
	Curse UMETA(DisplayName = "Curse")
};

UENUM(BlueprintType)
enum class ECardRarity : uint8
{
	Basic UMETA(DisplayName = "Basic"),
	Common UMETA(DisplayName = "Common"),
	Uncommon UMETA(DisplayName = "Uncommon"),
	Rare UMETA(DisplayName = "Rare"),
	Special UMETA(DisplayName = "Special"),
	Curse UMETA(DisplayName = "Curse")
};

UENUM(BlueprintType)
enum class ECardColor : uint8
{
	Red UMETA(DisplayName = "Red"),
	Green UMETA(DisplayName = "Green"),
	Blue UMETA(DisplayName = "Blue"),
	Purple UMETA(DisplayName = "Purple"),
	Colorless UMETA(DisplayName = "Colorless"),
	Curse UMETA(DisplayName = "Curse")
};

UENUM(BlueprintType)
enum class ECardTargetType : uint8
{
	None UMETA(DisplayName = "None"),
	Self UMETA(DisplayName = "Self"),
	Enemy UMETA(DisplayName = "Enemy")
};

UENUM(BlueprintType)
enum class ECardDestination : uint8
{
	Discard UMETA(DisplayName = "Discard"),
	Exhaust UMETA(DisplayName = "Exhaust"),
	Removed UMETA(DisplayName = "Removed")
};
