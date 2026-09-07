#include "SelectExhaustHandCardEffect.h"

#include "../CardInstance.h"
#include "../CardPlayContext.h"
#include "../../Actions/BattleActionQueue.h"
#include "../../Actions/SelectionRequestAction.h"
#include "../../Battle/BattleManager.h"
#include "../../Deck/DeckRuntime.h"
#include "../../Selection/ExhaustSelectedContinuation.h"
#include "../../Selection/SelectionResolver.h"

void USelectExhaustHandCardEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.ActionOuter)
		|| !IsValid(Context.Battle)
		|| !IsValid(Context.Deck)
		|| !IsValid(Context.Card))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] SelectExhaust build skipped: invalid ActionOuter, Battle, Deck or Card."));
		return;
	}

	USelectionResolver* Resolver = Context.Battle->GetSelectionResolver();
	if (!IsValid(Resolver))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] SelectExhaust build skipped: Battle has no SelectionResolver."));
		return;
	}

	FSelectionRequest Request;
	Request.SelectionSource = TEXT("SelectExhaustHandCard");
	Request.MinCount = 1;
	Request.MaxCount = 1;
	// This effect represents a mandatory authored cost: when at least one other
	// Hand card exists, the player must choose exactly one before later Effects
	// (for example Burning Pact's Draw) may continue.
	Request.CancelPolicy = ESelectionCancelPolicy::Forbidden;

	for (const TObjectPtr<UCardInstance>& HandCard : Context.Deck->GetHandCards())
	{
		if (!IsValid(HandCard.Get()))
		{
			continue;
		}
		// The played card is still held by Context.Card; exclude it from the
		// burnable candidates so the Effect never offers the played card itself.
		if (HandCard.Get() == Context.Card)
		{
			continue;
		}

		FSelectionCandidate Candidate;
		Candidate.RuntimeObject = HandCard.Get();
		Candidate.RuntimeSequence = HandCard->GetRuntimeId();
		Candidate.SelectionKey = HandCard->GetCardId();
		Request.Candidates.Add(Candidate);
	}

	if (Request.Candidates.Num() == 0)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[CardEffect] SelectExhaust skipped: no other Hand card to exhaust.")
		);
		return;
	}

	UExhaustSelectedContinuation* Continuation = NewObject<UExhaustSelectedContinuation>(Context.ActionOuter);
	Continuation->Initialize(
		Context.Deck,
		Context.Source,
		Context.EventDispatcher,
		Context.EventCombatants
	);

	USelectionRequestAction* Action = NewObject<USelectionRequestAction>(Context.ActionOuter);
	Action->Initialize(Resolver, Request, Continuation);
	OutActions.Add(Action);
}

void USelectExhaustHandCardEffect::GetPreviewArgumentNames(TArray<FName>& OutNames) const
{
}

void USelectExhaustHandCardEffect::BuildPreviewArguments(
	const FCardEffectPreviewContext& Context,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
}

void USelectExhaustHandCardEffect::ValidatePreviewConfiguration(TArray<FText>& OutErrors) const
{
}
