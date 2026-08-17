#include "DamageModifier.h"

bool UDamageModifier::IsApplicable(EDamageKind DamageKind, EModifierScope ContributionScope) const
{
	return Scope == ContributionScope && ApplicableDamageKind == DamageKind;
}
