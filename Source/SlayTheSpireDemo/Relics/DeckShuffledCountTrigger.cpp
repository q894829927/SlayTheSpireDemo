#include "DeckShuffledCountTrigger.h"

#include "../Actions/AdvanceRelicCounterAction.h"
#include "../Actions/BattleActionQueue.h"
#include "../Battle/BattleManager.h"
#include "../Combat/Combatant.h"
#include "../Events/BattleEvent.h"
#include "Effects/RelicEffect.h"
#include "RelicInstance.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UDeckShuffledCountTrigger::CanReact(
	const FBattleEvent& Event,
	const FTriggerContext& Context
) const
{
	const FDeckShuffledEvent* DeckShuffled = Event.TryGet<FDeckShuffledEvent>();
	URelicInstance* Relic = Context.GetRelicSource();
	ABattleManager* Battle = Context.GetBattle();

	if (DeckShuffled == nullptr
		|| !IsValid(DeckShuffled->Deck)
		|| !IsValid(Relic)
		|| !IsValid(Battle)
		|| Relic->GetBattle() != Battle
		|| !Battle->IsAuthoritativeDeckRuntime(DeckShuffled->Deck)
		|| RequiredCount <= 0
		|| Effects.Num() == 0)
	{
		return false;
	}

	for (const TObjectPtr<URelicEffect>& Effect : Effects)
	{
		if (!IsValid(Effect.Get()))
		{
			return false;
		}
	}

	return true;
}

void UDeckShuffledCountTrigger::BuildReactions(
	const FBattleEvent& Event,
	const FTriggerContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!CanReact(Event, Context))
	{
		return;
	}

	UBattleActionQueue* Queue = Cast<UBattleActionQueue>(Context.GetActionOuter());
	URelicInstance* Relic = Context.GetRelicSource();
	ABattleManager* Battle = Context.GetBattle();
	ACombatant* Owner = IsValid(Battle) ? Battle->Player.Get() : nullptr;
	if (!IsValid(Queue) || !IsValid(Relic) || !IsValid(Battle) || !IsValid(Owner))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] DeckShuffledCountTrigger build rejected invalid context."));
		return;
	}

	FName OwnerPresentationId = NAME_None;
	Battle->TryResolveCombatantPresentationId(Owner, OwnerPresentationId);

	FRelicEffectContext EffectContext;
	EffectContext.Relic = Relic;
	EffectContext.Battle = Battle;
	EffectContext.Owner = Owner;
	EffectContext.OwnerPresentationId = OwnerPresentationId;
	EffectContext.ActionOuter = Queue;

	TArray<UBattleAction*> PreparedRewards;
	for (const TObjectPtr<URelicEffect>& EffectPtr : Effects)
	{
		const URelicEffect* Effect = EffectPtr.Get();
		TArray<UBattleAction*> EffectActions;
		if (!IsValid(Effect) || !Effect->BuildActions(EffectContext, EffectActions))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Relic] DeckShuffledCountTrigger discarded reaction because an Effect failed to build."));
			return;
		}
		PreparedRewards.Append(EffectActions);
	}

	if (PreparedRewards.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] DeckShuffledCountTrigger discarded reaction because prepared rewards were empty."));
		return;
	}

	UAdvanceRelicCounterAction* CounterAction = NewObject<UAdvanceRelicCounterAction>(Queue);
	if (!IsValid(CounterAction)
		|| !CounterAction->Initialize(Relic, RequiredCount, PreparedRewards))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] DeckShuffledCountTrigger discarded reaction because CounterAction initialization failed."));
		return;
	}

	OutActions.Add(CounterAction);
}

#if WITH_EDITOR
EDataValidationResult UDeckShuffledCountTrigger::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bValid = ParentResult != EDataValidationResult::Invalid;

	if (RequiredCount <= 0)
	{
		Context.AddError(FText::FromString(TEXT("DeckShuffledCountTrigger RequiredCount must be greater than zero.")));
		bValid = false;
	}

	if (Effects.Num() == 0)
	{
		Context.AddError(FText::FromString(TEXT("DeckShuffledCountTrigger requires at least one RelicEffect.")));
		bValid = false;
	}

	for (int32 Index = 0; Index < Effects.Num(); ++Index)
	{
		const URelicEffect* Effect = Effects[Index].Get();
		if (!IsValid(Effect))
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("DeckShuffledCountTrigger contains an invalid RelicEffect at index %d."),
				Index)));
			bValid = false;
			continue;
		}

		if (Effect->IsDataValid(Context) == EDataValidationResult::Invalid)
		{
			bValid = false;
		}
	}

	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
