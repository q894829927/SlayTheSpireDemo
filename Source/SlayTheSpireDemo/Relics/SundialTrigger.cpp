#include "SundialTrigger.h"

#include "../Actions/SundialAdvanceAction.h"
#include "../Battle/BattleManager.h"
#include "../Events/BattleEvent.h"
#include "RelicInstance.h"

bool USundialTrigger::CanReact(const FBattleEvent& Event, const FTriggerContext& Context) const
{
	const FDeckShuffledEvent* DeckShuffled = Event.TryGet<FDeckShuffledEvent>();
	URelicInstance* Relic = Context.GetRelicSource();
	ABattleManager* Battle = Context.GetBattle();

	return DeckShuffled != nullptr
		&& IsValid(DeckShuffled->Deck)
		&& IsValid(Relic)
		&& IsValid(Battle)
		&& Relic->GetBattle() == Battle
		&& Battle->IsAuthoritativeDeckRuntime(DeckShuffled->Deck)
		&& ShufflesRequired > 0
		&& EnergyGain > 0;
}

void USundialTrigger::BuildReactions(
	const FBattleEvent& Event,
	const FTriggerContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!CanReact(Event, Context) || !IsValid(Context.GetActionOuter()))
	{
		return;
	}

	USundialAdvanceAction* Action = NewObject<USundialAdvanceAction>(Context.GetActionOuter());
	Action->Initialize(Context.GetRelicSource(), ShufflesRequired, EnergyGain);
	OutActions.Add(Action);
}
