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

	for (int32 Index = 0; Index < DrawCount; ++Index)
	{
		UDrawCardAction* Action = NewObject<UDrawCardAction>(Context.ActionOuter);
		Action->Initialize(Context.Deck);
		OutActions.Add(Action);
	}
}
