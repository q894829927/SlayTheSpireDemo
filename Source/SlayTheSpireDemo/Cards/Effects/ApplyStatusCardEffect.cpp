#include "ApplyStatusCardEffect.h"

#include "../CardPlayContext.h"
#include "../../Actions/ApplyStatusAction.h"
#include "../../Battle/BattleManager.h"
#include "../../Combat/Combatant.h"
#include "../../Status/StatusData.h"

void UApplyStatusCardEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.ActionOuter) ||
		!IsValid(Context.Battle) ||
		!IsValid(Context.Source) ||
		!IsValid(Context.Target) ||
		!IsValid(StatusDefinition))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] ApplyStatus build skipped: invalid dependency."));
		return;
	}

	if (Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] ApplyStatus build skipped: Amount=%d."), Amount);
		return;
	}

	UApplyStatusAction* Action = NewObject<UApplyStatusAction>(Context.ActionOuter);
	Action->Initialize(
		Context.Battle,
		Context.Source,
		Context.Target,
		StatusDefinition,
		Amount
	);
	OutActions.Add(Action);
}
