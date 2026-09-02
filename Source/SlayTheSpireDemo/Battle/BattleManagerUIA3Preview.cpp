#include "BattleManager.h"

#include "BattleImmediatePreview.h"
#include "BattleTextResolver.h"
#include "BattleTextTypes.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"
#include "../Cards/Effects/CardEffect.h"
#include "../Combat/Combatant.h"

bool ABattleManager::TryBuildImmediateCardPreview(
	const UCardInstance* Card,
	const ACombatant* Target,
	FImmediateCardPreview& OutPreview
) const
{
	OutPreview = FImmediateCardPreview{};

	if (BattleId == 0
		|| StateRevision == 0
		|| BattleId > static_cast<uint64>(MAX_int64)
		|| StateRevision > static_cast<uint64>(MAX_int64)
		|| !IsValid(Player.Get())
		|| !IsValid(Card))
	{
		return false;
	}

	const UCardData* Definition = Card->GetDefinition();
	if (!IsValid(Definition) || Card->GetRuntimeId() == INDEX_NONE)
	{
		return false;
	}

	FName SourcePresentationId = NAME_None;
	if (!TryResolveCombatantPresentationId(Player.Get(), SourcePresentationId)
		|| SourcePresentationId.IsNone())
	{
		return false;
	}

	FName TargetPresentationId = NAME_None;
	if (Target != nullptr)
	{
		if (!IsValid(Target)
			|| !TryResolveCombatantPresentationId(Target, TargetPresentationId)
			|| TargetPresentationId.IsNone())
		{
			return false;
		}
	}

	FImmediateCardPreview Preview;
	Preview.BattleId = static_cast<int64>(BattleId);
	Preview.StateRevision = static_cast<int64>(StateRevision);
	Preview.CardRuntimeId = Card->GetRuntimeId();
	Preview.SourcePresentationId = SourcePresentationId;
	Preview.TargetPresentationId = TargetPresentationId;

	// A3-3 reuses the existing Gameplay validation vocabulary instead of
	// reproducing target or Energy rules in UI code. A missing concrete target is
	// a coherent pre-target state and therefore uses the target-agnostic query.
	Preview.Validation = Target != nullptr
		? QueryPlayCard(Card, Target)
		: QueryCardPlayability(Card);

	Preview.EnergyBefore = Energy;
	Preview.EffectiveCost = Card->GetCurrentCost();
	if (Preview.Validation.bAllowed && Preview.EnergyBefore >= Preview.EffectiveCost)
	{
		Preview.bHasEnergyAfter = true;
		Preview.EnergyAfter = Preview.EnergyBefore - Preview.EffectiveCost;
	}

	Preview.Operations.Reserve(Definition->Effects.Num());

	FCardEffectPreviewContext Context;
	Context.Card = Card;
	Context.Source = Player.Get();
	// A3-1's shared preview context predates the public const Query surface and
	// still stores mutable actor pointers. The Effect contribution contract is
	// read-only; this narrow adapter must not be used to mutate Target.
	Context.Target = const_cast<ACombatant*>(Target);

	for (int32 EffectIndex = 0; EffectIndex < Definition->Effects.Num(); ++EffectIndex)
	{
		const UCardEffect* Effect = Definition->Effects[EffectIndex].Get();
		if (!IsValid(Effect))
		{
			return false;
		}

		const int32 OperationStart = Preview.Operations.Num();
		Effect->BuildImmediatePreviewOperations(Context, EffectIndex, Preview.Operations);

		for (int32 OperationIndex = OperationStart; OperationIndex < Preview.Operations.Num(); ++OperationIndex)
		{
			const FImmediatePreviewOperation& Operation = Preview.Operations[OperationIndex];
			if (Operation.EffectIndex != EffectIndex
				|| Operation.SemanticArgumentName.IsNone()
				|| Operation.BaseAmount < 0
				|| Operation.ResolvedAmount < 0
				|| Operation.HitCount <= 0)
			{
				return false;
			}
		}
	}

	Preview.CardFaceDescription = FBattleTextResolver::ResolveCardDescriptionForImmediatePreview(
		Card,
		Player.Get(),
		Preview.Operations);

	OutPreview = MoveTemp(Preview);
	return true;
}
