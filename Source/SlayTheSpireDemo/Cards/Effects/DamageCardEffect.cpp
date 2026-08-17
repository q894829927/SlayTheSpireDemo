#include "DamageCardEffect.h"

#include "../CardPlayContext.h"
#include "../../Actions/DamageAction.h"

void UDamageCardEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.ActionOuter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] Damage build skipped: invalid ActionOuter."));
		return;
	}

	UDamageAction* Action = NewObject<UDamageAction>(Context.ActionOuter);
	Action->Initialize(Context.Source, Context.Target, BaseAmount, DamageKind);
	OutActions.Add(Action);
}
