#include "BlockModifierPipeline.h"

#include "BlockModifier.h"
#include "BlockSpec.h"
#include "../../Combat/Combatant.h"
#include "../../Status/StatusContainer.h"
#include "../../Status/StatusData.h"
#include "../../Status/StatusInstance.h"

namespace
{
	struct FCollectedBlockModifier
	{
		const UBlockModifier* Modifier = nullptr;
		const UStatusInstance* StatusInstance = nullptr;
		int32 LocalModifierIndex = INDEX_NONE;
	};

	void CollectFromCombatant(
		ACombatant* Combatant,
		EModifierScope ContributionScope,
		TArray<FCollectedBlockModifier>& OutEntries
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

			for (int32 ModifierIndex = 0; ModifierIndex < Definition->BlockModifiers.Num(); ++ModifierIndex)
			{
				const UBlockModifier* Modifier = Definition->BlockModifiers[ModifierIndex].Get();
				if (!IsValid(Modifier) || !Modifier->IsApplicable(ContributionScope))
				{
					continue;
				}

				FCollectedBlockModifier& Entry = OutEntries.AddDefaulted_GetRef();
				Entry.Modifier = Modifier;
				Entry.StatusInstance = StatusInstance;
				Entry.LocalModifierIndex = ModifierIndex;
			}
		}
	}
}

void FBlockModifierPipeline::Resolve(FBlockSpec& Spec)
{
	Spec.WorkingAmount = FMath::Max(0, Spec.BaseAmount);
	Spec.ResolvedAmount = Spec.WorkingAmount;

	TArray<FCollectedBlockModifier> Entries;
	CollectFromCombatant(Spec.Source, EModifierScope::Source, Entries);
	CollectFromCombatant(Spec.Target, EModifierScope::Target, Entries);

	Entries.Sort(
		[](const FCollectedBlockModifier& A, const FCollectedBlockModifier& B)
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
		TEXT("[BlockPipeline] Begin: Source=%s Target=%s Base=%d Modifiers=%d"),
		*GetNameSafe(Spec.Source),
		*GetNameSafe(Spec.Target),
		Spec.BaseAmount,
		Entries.Num()
	);

	for (const FCollectedBlockModifier& Entry : Entries)
	{
		Entry.Modifier->Apply(Entry.StatusInstance, Spec);
	}

	Spec.ResolvedAmount = FMath::Clamp(Spec.WorkingAmount, 0, MAX_int32);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[BlockPipeline] Resolved: Base=%d Resolved=%d"),
		Spec.BaseAmount,
		Spec.ResolvedAmount
	);
}
