#include "PresentationCardSnapshotBuilder.h"

#include "../Battle/BattleTextResolver.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"

bool PresentationCardSnapshot::TryBuild(
	const UCardInstance* Card,
	ACombatant* Source,
	FPresentationCardSnapshot& OutSnapshot
)
{
	OutSnapshot = FPresentationCardSnapshot{};
	if (!IsValid(Card)
		|| !IsValid(Card->GetDefinition())
		|| Card->GetRuntimeId() == INDEX_NONE
		|| Card->GetCardId().IsNone())
	{
		return false;
	}

	OutSnapshot.RuntimeId = Card->GetRuntimeId();
	OutSnapshot.CardId = Card->GetCardId();
	const FText EffectiveDisplayName = Card->GetDisplayName();
	OutSnapshot.DisplayName = EffectiveDisplayName.IsEmpty()
		? FText::FromName(OutSnapshot.CardId)
		: EffectiveDisplayName;
	OutSnapshot.Cost = Card->GetCurrentCost();
	OutSnapshot.CardType = Card->GetCardType();
	OutSnapshot.TargetType = Card->GetTargetType();
	OutSnapshot.Description = FBattleTextResolver::ResolveCardDescription(Card, Source);
	OutSnapshot.RichDescription = FBattleTextResolver::ResolveCardRichDescription(Card, Source);
	OutSnapshot.CardArt = Card->GetCardArt();
	return true;
}
