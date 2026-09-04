#include "UpgradeCardAction.h"

#include "../Cards/CardInstance.h"

void UUpgradeCardAction::Initialize(UCardInstance* InCard)
{
	Card = InCard;
}

void UUpgradeCardAction::Execute(UBattleActionQueue* /*Queue*/)
{
	if (!IsValid(Card.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] UpgradeCardAction skipped: invalid CardInstance."));
		Finish();
		return;
	}

	if (!Card->CommitUpgrade())
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Action] UpgradeCardAction rejected: %s cannot be upgraded."),
			*Card->GetDebugLabel());
		Finish();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Action] UpgradeCardAction committed: %s."), *Card->GetDebugLabel());
	Finish();
}
