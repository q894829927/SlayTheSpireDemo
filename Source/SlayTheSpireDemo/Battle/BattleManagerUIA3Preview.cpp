#include "BattleManager.h"

#include "BattleImmediatePreview.h"
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
		|| !IsValid(Card)
		|| !IsValid(Target))
	{
		return false;
	}

	const UCardData* Definition = Card->GetDefinition();
	if (!IsValid(Definition) || Card->GetRuntimeId() == INDEX_NONE)
	{
		return false;
	}

	FName SourcePresentationId = NAME_None;
	FName TargetPresentationId = NAME_None;
	if (!TryResolveCombatantPresentationId(Player.Get(), SourcePresentationId)
		|| !TryResolveCombatantPresentationId(Target, TargetPresentationId)
		|| SourcePresentationId.IsNone()
		|| TargetPresentationId.IsNone())
	{
		return false;
	}

	FImmediateCardPreview Preview;
	Preview.BattleId = static_cast<int64>(BattleId);
	Preview.StateRevision = static_cast<int64>(StateRevision);
	Preview.CardRuntimeId = Card->GetRuntimeId();
	Preview.SourcePresentationId = SourcePresentationId;
	Preview.TargetPresentationId = TargetPresentationId;
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
				|| Operation.ResolvedAmount < 0
				|| Operation.HitCount <= 0)
			{
				return false;
			}
		}
	}

	OutPreview = MoveTemp(Preview);
	return true;
}
