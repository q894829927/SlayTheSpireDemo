#pragma once

#include "CoreMinimal.h"

class ACombatant;
class UCardData;
class UCardInstance;
class UStatusData;
class UStatusInstance;

class SLAYTHESPIREDEMO_API FBattleTextResolver
{
public:
	static FText ResolveCardDescription(const UCardInstance* Card, ACombatant* Source);
	static FText ResolveStatusDescription(const UStatusInstance* StatusInstance);

	// Editor/DataAsset validation helpers. Runtime resolution remains fail-soft.
	static bool ValidateCardDefinition(const UCardData* Definition, TArray<FText>& OutErrors);
	static bool ValidateStatusDefinition(const UStatusData* Definition, TArray<FText>& OutErrors);
};
