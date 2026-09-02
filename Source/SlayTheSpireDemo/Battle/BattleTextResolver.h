#pragma once

#include "CoreMinimal.h"

class ACombatant;
class UCardData;
class UCardInstance;
class UStatusData;
class UStatusInstance;
struct FImmediatePreviewOperation;

class SLAYTHESPIREDEMO_API FBattleTextResolver
{
public:
	// A3-1 card-face resolver. Enemy-target cards intentionally omit a concrete
	// target so the normal Hand surface reflects only current source-side effects.
	static FText ResolveCardDescription(const UCardInstance* Card, ACombatant* Source);

	// A3 target-specific card face: start from the validated A3-1 semantic
	// arguments, then override only supported Damage/Block semantic names with the
	// already Gameplay-resolved ImmediatePreview Operations. Unsupported effects
	// keep their normal current card-face values.
	static FText ResolveCardDescriptionForImmediatePreview(
		const UCardInstance* Card,
		ACombatant* Source,
		const TArray<FImmediatePreviewOperation>& Operations);

	// Same target-specific values as the plain resolver, but changed numeric
	// arguments are wrapped with Native RichText style tags. The resolver uses
	// semantic argument identity and never searches the formatted sentence for a
	// matching number.
	static FText ResolveCardRichDescriptionForImmediatePreview(
		const UCardInstance* Card,
		ACombatant* Source,
		const TArray<FImmediatePreviewOperation>& Operations);

	static FText ResolveStatusDescription(const UStatusInstance* StatusInstance);

	// Editor/DataAsset validation helpers. Runtime resolution remains fail-soft.
	static bool ValidateCardDefinition(const UCardData* Definition, TArray<FText>& OutErrors);
	static bool ValidateStatusDefinition(const UStatusData* Definition, TArray<FText>& OutErrors);
};
