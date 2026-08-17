#include "PlayCardAction.h"

#include "BattleActionQueue.h"
#include "FinishCardPlayAction.h"
#include "../Battle/BattleManager.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"
#include "../Cards/CardPlayContext.h"
#include "../Cards/Effects/CardEffect.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"

void UPlayCardAction::Initialize(
	ABattleManager* InBattle,
	UCardInstance* InCard,
	ACombatant* InSource,
	ACombatant* InRequestedTarget,
	UDeckRuntime* InDeck
)
{
	Battle = InBattle;
	Card = InCard;
	Source = InSource;
	RequestedTarget = InRequestedTarget;
	Deck = InDeck;
}

void UPlayCardAction::Execute(UBattleActionQueue* Queue)
{
	if (!IsValid(Queue) || !IsValid(Battle.Get()) || !IsValid(Card.Get()) || !IsValid(Source.Get()) || !IsValid(Deck.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction skipped: invalid runtime dependency."));
		Finish();
		return;
	}

	if (Source->IsDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction skipped: source is dead."));
		Finish();
		return;
	}

	const UCardData* Definition = Card->GetDefinition();
	if (Definition == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction skipped: %s has no valid definition."), *Card->GetDebugLabel());
		Finish();
		return;
	}

	if (!Deck->IsCardInHand(Card.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction skipped: %s is no longer in Hand."), *Card->GetDebugLabel());
		Finish();
		return;
	}

	ACombatant* ResolvedTarget = nullptr;
	switch (Definition->TargetType)
	{
	case ECardTargetType::None:
		break;

	case ECardTargetType::Self:
		ResolvedTarget = Source.Get();
		break;

	case ECardTargetType::Enemy:
		if (!IsValid(RequestedTarget.Get()) || RequestedTarget->IsDead())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction skipped: %s requires a valid living enemy target."), *Card->GetDebugLabel());
			Finish();
			return;
		}
		ResolvedTarget = RequestedTarget.Get();
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction skipped: unsupported target type for %s."), *Card->GetDebugLabel());
		Finish();
		return;
	}

	const int32 Cost = Card->GetCurrentCost();
	if (!Battle->CanSpendEnergy(Cost))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction rejected: not enough energy for %s Cost=%d."), *Card->GetDebugLabel(), Cost);
		Finish();
		return;
	}

	FCardPlayContext Context;
	Context.Card = Card.Get();
	Context.Source = Source.Get();
	Context.Target = ResolvedTarget;
	Context.Deck = Deck.Get();
	Context.ActionOuter = Queue;

	TArray<UBattleAction*> FollowUpActions;
	for (const TObjectPtr<UCardEffect>& EffectPtr : Definition->Effects)
	{
		const UCardEffect* Effect = EffectPtr.Get();
		if (!IsValid(Effect))
		{
			UE_LOG(LogTemp, Error, TEXT("[Action] PlayCardAction aborted: %s contains an invalid Effect definition."), *Card->GetDebugLabel());
			Finish();
			return;
		}

		Effect->BuildActions(Context, FollowUpActions);
	}

	for (UBattleAction* FollowUpAction : FollowUpActions)
	{
		if (!IsValid(FollowUpAction) || FollowUpAction->IsFinished() || FollowUpAction->GetOuter() != Queue)
		{
			UE_LOG(LogTemp, Error, TEXT("[Action] PlayCardAction aborted: %s built an invalid follow-up action batch."), *Card->GetDebugLabel());
			Finish();
			return;
		}
	}

	UFinishCardPlayAction* FinishPlayAction = NewObject<UFinishCardPlayAction>(Queue);
	FinishPlayAction->Initialize(Deck.Get(), Card.Get());
	FollowUpActions.Add(FinishPlayAction);

	if (!Deck->TryMoveHandCardToPlayArea(Card.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction aborted: failed to move %s from Hand to PlayArea."), *Card->GetDebugLabel());
		Finish();
		return;
	}

	if (!Battle->TrySpendEnergy(Cost))
	{
		UE_LOG(LogTemp, Error, TEXT("[Action] PlayCardAction rollback: energy spend unexpectedly failed for %s."), *Card->GetDebugLabel());
		Deck->TryReturnPlayAreaCardToHand(Card.Get());
		Finish();
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Action] PlayCardAction committed: Card=%s Cost=%d FollowUps=%d."),
		*Card->GetDebugLabel(),
		Cost,
		FollowUpActions.Num()
	);

	// Commit the entire dependent batch before Finish(). This preserves one
	// continuous queue chain and prevents an intermediate QueueEmpty broadcast.
	for (UBattleAction* FollowUpAction : FollowUpActions)
	{
		Queue->AddToBack(FollowUpAction);
	}

	Finish();
}
