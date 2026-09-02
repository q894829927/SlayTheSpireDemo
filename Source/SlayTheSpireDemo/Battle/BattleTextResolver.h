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
	// A3-1 card-face resolver. Enemy-target cards intentionally omit a concrete
	// target so the normal Hand surface reflects only current source-side effects.
	static FText ResolveCardDescription(const UCardInstance* Card, ACombatant* Source);

	// A3 target-specific variant. The same authored format/effect argument system
	// is reused, but a concrete current target may participate in read-only Damage
	// resolution. Self-target Block still resolves Source as its own target.
	static FText ResolveCardDescription(
		const UCardInstance* Card,
		ACombatant* Source,
		ACombatant* Target);

	static FText ResolveStatusDescription(const UStatusInstance* StatusInstance);

	// Editor/DataAsset validation helpers. Runtime resolution remains fail-soft.
	static bool ValidateCardDefinition(const UCardData* Definition, TArray<FText>& OutErrors);
	static bool ValidateStatusDefinition(const UStatusData* Definition, TArray<FText>& OutErrors);
};
