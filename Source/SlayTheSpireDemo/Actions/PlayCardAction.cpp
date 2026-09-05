#include "PlayCardAction.h"

#include "BattleActionQueue.h"
#include "FinishCardPlayAction.h"
#include "../Battle/BattleManager.h"
#include "../Battle/BattleTextResolver.h"
#include "../Battle/BattleTextTypes.h"
#include "../Battle/EnergyMutation.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"
#include "../Cards/CardPlayContext.h"
#include "../Cards/Effects/CardEffect.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Events/BattleEventDispatcher.h"
#include "../Presentation/PresentationCardSnapshotBuilder.h"

namespace
{
	bool TryBuildCommittedCardFaceRichDescription(
		const UCardInstance* Card,
		ACombatant* Source,
		ACombatant* Target,
		FText& OutRichDescription)
	{
		OutRichDescription = FText::GetEmpty();
		if (!IsValid(Card) || !IsValid(Card->GetDefinition()) || !IsValid(Source))
		{
			return false;
		}

		FCardEffectPreviewContext PreviewContext;
		PreviewContext.Card = Card;
		PreviewContext.Source = Source;
		PreviewContext.Target = Target;

		const TArray<TObjectPtr<UCardEffect>>& Effects = Card->GetEffects();
		TArray<FImmediatePreviewOperation> Operations;
		Operations.Reserve(Effects.Num());
		for (int32 EffectIndex = 0; EffectIndex < Effects.Num(); ++EffectIndex)
		{
			const UCardEffect* Effect = Effects[EffectIndex].Get();
			if (!IsValid(Effect))
			{
				return false;
			}

			const int32 OperationStart = Operations.Num();
			Effect->BuildImmediatePreviewOperations(PreviewContext, EffectIndex, Operations);
			for (int32 OperationIndex = OperationStart; OperationIndex < Operations.Num(); ++OperationIndex)
			{
				const FImmediatePreviewOperation& Operation = Operations[OperationIndex];
				if (Operation.EffectIndex != EffectIndex
					|| Operation.SemanticArgumentName.IsNone()
					|| Operation.BaseAmount < 0
					|| Operation.ResolvedAmount < 0
					|| Operation.HitCount <= 0)
				{
					return false;
				}
			}
		}

		OutRichDescription = FBattleTextResolver::ResolveCardRichDescriptionForImmediatePreview(
			Card,
			Source,
			Operations);
		return !OutRichDescription.IsEmpty();
	}
}

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
	EventDispatcher = nullptr;
	EventCombatants.Reset();
}

void UPlayCardAction::Initialize(
	ABattleManager* InBattle,
	UCardInstance* InCard,
	ACombatant* InSource,
	ACombatant* InRequestedTarget,
	UDeckRuntime* InDeck,
	UBattleEventDispatcher* InEventDispatcher,
	const TArray<ACombatant*>& InEventCombatants
)
{
	Battle = InBattle;
	Card = InCard;
	Source = InSource;
	RequestedTarget = InRequestedTarget;
	Deck = InDeck;
	EventDispatcher = InEventDispatcher;
	EventCombatants.Reset();
	for (ACombatant* Combatant : InEventCombatants)
	{
		EventCombatants.Add(Combatant);
	}
}

void UPlayCardAction::Execute(UBattleActionQueue* Queue)
{
	if (!IsValid(Queue) || !IsValid(Battle.Get()) || !IsValid(Card.Get()) || !IsValid(Source.Get()) || !IsValid(Deck.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction skipped: invalid runtime dependency."));
		Finish();
		return;
	}

	UBattleEventDispatcher* ResolvedEventDispatcher = EventDispatcher.Get();
	TArray<ACombatant*> RawEventCombatants;

	if (IsValid(ResolvedEventDispatcher) && EventCombatants.Num() > 0)
	{
		RawEventCombatants.Reserve(EventCombatants.Num());
		for (const TObjectPtr<ACombatant>& Combatant : EventCombatants)
		{
			if (!IsValid(Combatant.Get()))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction skipped: invalid authoritative combatant in event-dispatch context."));
				Finish();
				return;
			}
			RawEventCombatants.Add(Combatant.Get());
		}
	}
	else
	{
		ResolvedEventDispatcher = nullptr;
		RawEventCombatants.Reset();
		Battle->TryBuildEventDispatchContext(ResolvedEventDispatcher, RawEventCombatants);
	}

	if (Source->IsDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction skipped: source is dead."));
		Finish();
		return;
	}

	if (!IsValid(Card->GetDefinition()))
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
	switch (Card->GetTargetType())
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
	Context.Battle = Battle.Get();
	Context.Card = Card.Get();
	Context.Source = Source.Get();
	Context.Target = ResolvedTarget;
	Context.Deck = Deck.Get();
	Context.EventDispatcher = ResolvedEventDispatcher;
	Context.EventCombatants = RawEventCombatants;
	Context.ActionOuter = Queue;
	Context.PresentationRecordWriter = GetPresentationRecordWriter();
	Battle->TryResolveCombatantPresentationId(Source.Get(), Context.SourcePresentationId);
	if (IsValid(ResolvedTarget))
	{
		Battle->TryResolveCombatantPresentationId(ResolvedTarget, Context.TargetPresentationId);
	}

	FText CommittedCardFaceRichDescription;
	const bool bHasCommittedCardFaceRichDescription = TryBuildCommittedCardFaceRichDescription(
		Card.Get(),
		Source.Get(),
		ResolvedTarget,
		CommittedCardFaceRichDescription);

	const TArray<TObjectPtr<UCardEffect>>& Effects = Card->GetEffects();
	TArray<UBattleAction*> FollowUpActions;
	for (const TObjectPtr<UCardEffect>& EffectPtr : Effects)
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

		FollowUpAction->SetPresentationRecordWriter(GetPresentationRecordWriter());
	}

	UFinishCardPlayAction* FinishPlayAction = NewObject<UFinishCardPlayAction>(Queue);
	FinishPlayAction->Initialize(
		Deck.Get(),
		Card.Get(),
		Source.Get(),
		ResolvedEventDispatcher,
		RawEventCombatants
	);
	FinishPlayAction->SetPresentationRecordWriter(GetPresentationRecordWriter());
	FollowUpActions.Add(FinishPlayAction);

	const FCardZoneMutationResult ZoneCommit = Deck->TryMoveHandCardToPlayAreaCommit(Card.Get());
	if (!ZoneCommit.bCommitted)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] PlayCardAction aborted: failed to move %s from Hand to PlayArea."), *Card->GetDebugLabel());
		Finish();
		return;
	}

	const FEnergyCommitResult EnergyCommit = BattleEnergyMutation::TrySpend(Battle.Get(), Cost);
	if (!EnergyCommit.bSucceeded)
	{
		UE_LOG(LogTemp, Error, TEXT("[Action] PlayCardAction rollback: energy spend unexpectedly failed for %s."), *Card->GetDebugLabel());
		const FCardZoneMutationResult Rollback = Deck->TryReturnPlayAreaCardToHandAtIndexCommit(
			Card.Get(),
			ZoneCommit.FromIndex
		);
		if (!Rollback.bCommitted
			|| Rollback.FromZone != ECardZone::PlayArea
			|| Rollback.ToZone != ECardZone::Hand
			|| Rollback.ToIndex != ZoneCommit.FromIndex)
		{
			Queue->RequestResolutionFault(FString::Printf(
				TEXT("PlayCardAction failed exact Hand-order rollback after energy spend failure for %s."),
				*Card->GetDebugLabel()
			));
		}
		Finish();
		return;
	}

	const FPresentationRecordWriter& Writer = GetPresentationRecordWriter();
	if (Writer.IsAvailable())
	{
		FName SourcePresentationId = NAME_None;
		FName TargetPresentationId = NAME_None;
		const bool bSourceValid = Battle->TryResolveCombatantPresentationId(Source.Get(), SourcePresentationId)
			&& !SourcePresentationId.IsNone();
		const bool bTargetValid = !IsValid(ResolvedTarget)
			|| (Battle->TryResolveCombatantPresentationId(ResolvedTarget, TargetPresentationId)
				&& !TargetPresentationId.IsNone());
		FPresentationCardSnapshot CardSnapshot;
		const bool bCardValid = PresentationCardSnapshot::TryBuild(Card.Get(), Source.Get(), CardSnapshot)
			&& CardSnapshot.RuntimeId == ZoneCommit.CardRuntimeId
			&& CardSnapshot.CardId == ZoneCommit.CardId;
		if (bCardValid && bHasCommittedCardFaceRichDescription)
		{
			CardSnapshot.RichDescription = CommittedCardFaceRichDescription;
		}

		if (!bSourceValid || !bTargetValid || !bCardValid)
		{
			Writer.InvalidateCurrentResolution();
			UE_LOG(LogTemp, Warning, TEXT("[Presentation] CardPlayed commit could not build a trustworthy frozen payload."));
		}
		else
		{
			FPresentationRecord Record;
			Record.Type = EBattlePresentationRecordType::CardPlayed;
			Record.CardPlayed.Card = MoveTemp(CardSnapshot);
			Record.CardPlayed.SourcePresentationId = SourcePresentationId;
			Record.CardPlayed.TargetPresentationId = TargetPresentationId;
			Record.CardPlayed.HandIndexBefore = ZoneCommit.FromIndex;
			Record.CardPlayed.PlayAreaIndexAfter = ZoneCommit.ToIndex;
			Record.CardPlayed.EnergyBefore = EnergyCommit.EnergyBefore;
			Record.CardPlayed.EnergyAfter = EnergyCommit.EnergyAfter;
			Record.CardPlayed.CostPaid = EnergyCommit.EnergyBefore - EnergyCommit.EnergyAfter;
			if (!Writer.Append(MoveTemp(Record)))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Presentation] CardPlayed append failed; Gameplay commits remain authoritative."));
			}
		}
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Action] PlayCardAction committed: Card=%s Cost=%d FollowUps=%d."),
		*Card->GetDebugLabel(),
		Cost,
		FollowUpActions.Num()
	);

	if (!Queue->AddBatchToBackPreserveOrder(FollowUpActions))
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("PlayCardAction committed %s but failed to enqueue its dependent follow-up batch."),
			*Card->GetDebugLabel()
		));
		Finish();
		return;
	}

	Finish();
}
