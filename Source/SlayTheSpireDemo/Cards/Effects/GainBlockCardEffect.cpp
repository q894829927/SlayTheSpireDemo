#include "GainBlockCardEffect.h"

#include "../CardPlayContext.h"
#include "../../Actions/GainBlockAction.h"

void UGainBlockCardEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.ActionOuter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] Block build skipped: invalid ActionOuter."));
		return;
	}

	UGainBlockAction* Action = NewObject<UGainBlockAction>(Context.ActionOuter);
	Action->Initialize(Context.Source, Context.Target, BaseAmount);
	OutActions.Add(Action);
}
