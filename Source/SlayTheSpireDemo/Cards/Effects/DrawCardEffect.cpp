#include "DrawCardEffect.h"

#include "../CardPlayContext.h"
#include "../../Actions/DrawCardAction.h"

void UDrawCardEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.ActionOuter) || !IsValid(Context.Deck))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] Draw build skipped: invalid ActionOuter or Deck."));
		return;
	}

	const bool bHasEventContext = IsValid(Context.EventDispatcher) && Context.EventCombatants.Num() > 0;

	for (int32 Index = 0; Index < DrawCount; ++Index)
	{
		UDrawCardAction* Action = NewObject<UDrawCardAction>(Context.ActionOuter);
		if (bHasEventContext)
		{
			Action->Initialize(Context.Deck, Context.EventDispatcher, Context.EventCombatants);
		}
		else
		{
			// Preserve the pre-6C construction contract for isolated card-effect tests
			// and non-shuffle draws. If this action later needs to shuffle, it must
			// resolve valid battle-scoped event wiring before committing that shuffle.
			Action->Initialize(Context.Deck);
		}
		OutActions.Add(Action);
	}
}
