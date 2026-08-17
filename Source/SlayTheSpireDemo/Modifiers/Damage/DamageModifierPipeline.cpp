#include "DamageModifierPipeline.h"

#include "DamageModifier.h"
#include "DamageSpec.h"
#include "../../Combat/Combatant.h"
#include "../../Status/StatusContainer.h"
#include "../../Status/StatusData.h"
#include "../../Status/StatusInstance.h"

namespace
{
	struct FCollectedDamageModifier
	{
		const UDamageModifier* Modifier = nullptr;
		const UStatusInstance* StatusInstance = nullptr;
		int32 LocalModifierIndex = INDEX_NONE;
	};

	void CollectFromCombatant(
		ACombatant* Combatant,
		EModifierScope ContributionScope,
		EDamageKind DamageKind,
		TArray<FCollectedDamageModifier>& OutEntries
	)
	{
		if (!IsValid(Combatant))
		{
			return;
		}

		const UStatusContainer* Container = Combatant->GetStatusContainer();
		if (!IsValid(Container))
		{
			return;
		}

		for (const TObjectPtr<UStatusInstance>& StatusPtr : Container->GetStatuses())
		{
			const UStatusInstance* StatusInstance = StatusPtr.Get();
			if (!IsValid(StatusInstance) || StatusInstance->GetAmount() <= 0)
			{
				continue;
			}

			const UStatusData* Definition = StatusInstance->GetDefinition();
			if (!IsValid(Definition))
			{
				continue;
			}

			for (int32 ModifierIndex = 0; ModifierIndex < Definition->DamageModifiers.Num(); ++ModifierIndex)
			{
				const UDamageModifier* Modifier = Definition->DamageModifiers[ModifierIndex].Get();
				if (!IsValid(Modifier) || !Modifier->IsApplicable(DamageKind, ContributionScope))
				{
					continue;
				}

				FCollectedDamageModifier& Entry = OutEntries.AddDefaulted_GetRef();
				Entry.Modifier = Modifier;
				Entry.StatusInstance = StatusInstance;
				Entry.LocalModifierIndex = ModifierIndex;
			}
		}
	}
}

void FDamageModifierPipeline::Resolve(FDamageSpec& Spec)
{
	Spec.WorkingAmount = FMath::Max(0, Spec.BaseAmount);
	Spec.ResolvedAmount = Spec.WorkingAmount;

	TArray<FCollectedDamageModifier> Entries;
	CollectFromCombatant(Spec.Source, EModifierScope::Source, Spec.DamageKind, Entries);
	CollectFromCombatant(Spec.Target, EModifierScope::Target, Spec.DamageKind, Entries);

	Entries.Sort(
		[](const FCollectedDamageModifier& A, const FCollectedDamageModifier& B)
		{
			const uint8 APhase = static_cast<uint8>(A.Modifier->GetPhase());
			const uint8 BPhase = static_cast<uint8>(B.Modifier->GetPhase());
			if (APhase != BPhase)
			{
				return APhase < BPhase;
			}

			if (A.Modifier->Priority != B.Modifier->Priority)
			{
				return A.Modifier->Priority < B.Modifier->Priority;
			}

			const uint64 ASequence = A.StatusInstance->GetRuntimeSequence();
			const uint64 BSequence = B.StatusInstance->GetRuntimeSequence();
			if (ASequence != BSequence)
			{
				return ASequence < BSequence;
			}

			return A.LocalModifierIndex < B.LocalModifierIndex;
		}
	);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[DamagePipeline] Begin: Kind=%s Source=%s Target=%s Base=%d Modifiers=%d"),
		DamageKindToString(Spec.DamageKind),
		*GetNameSafe(Spec.Source),
		*GetNameSafe(Spec.Target),
		Spec.BaseAmount,
		Entries.Num()
	);

	for (const FCollectedDamageModifier& Entry : Entries)
	{
		Entry.Modifier->Apply(Entry.StatusInstance, Spec);
	}

	Spec.ResolvedAmount = FMath::Clamp(Spec.WorkingAmount, 0, MAX_int32);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[DamagePipeline] Resolved: Kind=%s Base=%d Resolved=%d"),
		DamageKindToString(Spec.DamageKind),
		Spec.BaseAmount,
		Spec.ResolvedAmount
	);
}
