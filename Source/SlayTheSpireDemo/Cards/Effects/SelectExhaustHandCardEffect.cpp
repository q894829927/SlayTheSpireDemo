#include "SelectExhaustHandCardEffect.h"

#include "../CardInstance.h"
#include "../CardPlayContext.h"
#include "../../Actions/BattleActionQueue.h"
#include "../../Actions/SelectionRequestAction.h"
#include "../../Battle/BattleManager.h"
#include "../../Deck/DeckRuntime.h"
#include "../../Selection/ExhaustSelectedContinuation.h"
#include "../../Selection/SelectionResolver.h"

ESelectExhaustSelectionMode USelectExhaustHandCardEffect::GetEffectiveSelectionMode(bool bIsUpgraded) const
{
	return bIsUpgraded ? UpgradedSelectionMode : BaseSelectionMode;
}

int32 USelectExhaustHandCardEffect::GetEffectiveSelectionCount(bool bIsUpgraded) const
{
	return bIsUpgraded ? UpgradedSelectionCount : BaseSelectionCount;
}

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

	const bool bIsUpgraded = Context.Card->IsUpgraded();
	const ESelectExhaustSelectionMode EffectiveMode = GetEffectiveSelectionMode(bIsUpgraded);
	const int32 AuthoredSelectionCount = GetEffectiveSelectionCount(bIsUpgraded);
	if (AuthoredSelectionCount <= 0)
	{
		return;
	}

	// C0-1 establishes the authored mode contract. Deterministic Random execution
	// is intentionally deferred to C0-6 so C0-1~3 do not invent an interim RNG
	// path or silently use a non-battle random source.
	if (EffectiveMode != ESelectExhaustSelectionMode::Player)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] SelectExhaust Random mode is authored but not executable until Wave 1C-C0 random-choice implementation."));
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

	const int32 RequiredCount = FMath::Min(AuthoredSelectionCount, Request.Candidates.Num());
	if (RequiredCount <= 0)
	{
		return;
	}
	Request.MinCount = RequiredCount;
	Request.MaxCount = RequiredCount;

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
	if (BaseSelectionCount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("SelectExhaustHandCardEffect BaseSelectionCount cannot be negative.")));
	}
	if (UpgradedSelectionCount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("SelectExhaustHandCardEffect UpgradedSelectionCount cannot be negative.")));
	}
}
