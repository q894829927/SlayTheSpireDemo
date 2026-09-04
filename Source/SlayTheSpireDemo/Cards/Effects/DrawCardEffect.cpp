#include "DrawCardEffect.h"

#include "../CardPlayContext.h"
#include "../../Actions/DrawCardsAction.h"
#include "../../Deck/DeckRuntime.h"
#include "../../Events/BattleEventDispatcher.h"
#include "../../Battle/BattleTextTypes.h"

int32 UDrawCardEffect::GetEffectiveDrawCount(bool bIsUpgraded) const
{
	return bIsUpgraded ? UpgradedDrawCount : DrawCount;
}

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

	if (DrawCount <= 0)
	{
		return;
	}

	const bool bHasEventContext = IsValid(Context.EventDispatcher) && Context.EventCombatants.Num() > 0;
	UDrawCardsAction* Action = NewObject<UDrawCardsAction>(Context.ActionOuter);
	if (bHasEventContext)
	{
		Action->Initialize(
			Context.Deck,
			DrawCount,
			Context.EventDispatcher,
			Context.EventCombatants,
			Context.Source
		);
	}
	else
	{
		Action->Initialize(Context.Deck, DrawCount, Context.Source);
	}
	OutActions.Add(Action);
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
	if (UpgradedDrawCount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("DrawCardEffect UpgradedDrawCount cannot be negative.")));
	}
}
