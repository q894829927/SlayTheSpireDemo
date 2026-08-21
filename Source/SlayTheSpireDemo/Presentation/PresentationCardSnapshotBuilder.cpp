#include "PresentationCardSnapshotBuilder.h"

#include "../Battle/BattleTextResolver.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"

bool PresentationCardSnapshot::TryBuild(
	const UCardInstance* Card,
	const ACombatant* Source,
	FPresentationCardSnapshot& OutSnapshot
)
{
	OutSnapshot = FPresentationCardSnapshot{};
	if (!IsValid(Card))
	{
		return false;
	}

	const UCardData* Definition = Card->GetDefinition();
	if (!IsValid(Definition)
		|| Card->GetRuntimeId() == INDEX_NONE
		|| Card->GetCardId().IsNone())
	{
		return false;
	}

	OutSnapshot.RuntimeId = Card->GetRuntimeId();
	OutSnapshot.CardId = Card->GetCardId();
	OutSnapshot.DisplayName = Definition->DisplayName.IsEmpty()
		? FText::FromName(OutSnapshot.CardId)
		: Definition->DisplayName;
	OutSnapshot.Cost = Card->GetCurrentCost();
	OutSnapshot.CardType = Definition->CardType;
	OutSnapshot.TargetType = Definition->TargetType;
	OutSnapshot.Description = FBattleTextResolver::ResolveCardDescription(Card, Source);
	OutSnapshot.CardArt = Definition->CardArt;
	return true;
}
