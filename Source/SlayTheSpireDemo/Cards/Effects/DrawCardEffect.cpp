#include "DrawCardEffect.h"

#include "../CardPlayContext.h"
#include "../../Actions/DrawCardAction.h"
#include "../../Deck/DeckRuntime.h"
#include "../../Events/BattleEventDispatcher.h"
#include "../../Battle/BattleTextTypes.h"

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
			Action->Initialize(
				Context.Deck,
				Context.EventDispatcher,
				Context.EventCombatants,
				Context.Source
			);
		}
		else
		{
			Action->Initialize(Context.Deck, Context.Source);
		}
		OutActions.Add(Action);
	}
}

void UDrawCardEffect::GetPreviewArgumentNames(TArray<FName>& OutNames) const
{
	OutNames.Add(DescriptionArgumentName);
}

void UDrawCardEffect::BuildPreviewArguments(
	const FCardEffectPreviewContext& /*Context*/,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	OutArguments.AddInteger(DescriptionArgumentName, DrawCount);
}

void UDrawCardEffect::ValidatePreviewConfiguration(TArray<FText>& OutErrors) const
{
	if (DescriptionArgumentName.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("DrawCardEffect requires a DescriptionArgumentName.")));
	}
	if (DrawCount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("DrawCardEffect DrawCount cannot be negative.")));
	}
}
