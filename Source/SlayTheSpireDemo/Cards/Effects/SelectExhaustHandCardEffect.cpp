#include "SelectExhaustHandCardEffect.h"

#include "../CardInstance.h"
#include "../CardPlayContext.h"
#include "../../Actions/BattleActionQueue.h"
#include "../../Actions/RandomSelectionAction.h"
#include "../../Actions/SelectionRequestAction.h"
#include "../../Battle/BattleManager.h"
#include "../../Battle/BattleTextTypes.h"
#include "../../Deck/DeckRuntime.h"
#include "../../Selection/ExhaustSelectedContinuation.h"
#include "../../Selection/SelectionResolver.h"

namespace
{
	FText GetSelectionModeDescriptionText(ESelectExhaustSelectionMode Mode)
	{
		switch (Mode)
		{
		case ESelectExhaustSelectionMode::Random:
			return NSLOCTEXT("SelectExhaustHandCardEffect", "RandomSelectionModeDescription", "随机消耗");
		case ESelectExhaustSelectionMode::Player:
		default:
			return NSLOCTEXT("SelectExhaustHandCardEffect", "PlayerSelectionModeDescription", "消耗");
		}
	}
}

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

	switch (EffectiveMode)
	{
	case ESelectExhaustSelectionMode::Player:
	{
		USelectionResolver* Resolver = Context.Battle->GetSelectionResolver();
		if (!IsValid(Resolver))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CardEffect] SelectExhaust Player build skipped: Battle has no SelectionResolver."));
			return;
		}

		USelectionRequestAction* Action = NewObject<USelectionRequestAction>(Context.ActionOuter);
		Action->Initialize(Resolver, Request, Continuation);
		OutActions.Add(Action);
		return;
	}

	case ESelectExhaustSelectionMode::Random:
	{
		FSelectionRandomIndexChooser RandomIndexChooser;
		RandomIndexChooser.BindUObject(Context.Deck, &UDeckRuntime::TryChooseRandomIndex);

		URandomSelectionAction* Action = NewObject<URandomSelectionAction>(Context.ActionOuter);
		Action->Initialize(Request, Continuation, RandomIndexChooser);
		OutActions.Add(Action);
		return;
	}

	default:
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] SelectExhaust build skipped: unsupported selection mode."));
		return;
	}
}

void USelectExhaustHandCardEffect::GetPreviewArgumentNames(TArray<FName>& OutNames) const
{
	if (!DescriptionArgumentName.IsNone())
	{
		OutNames.Add(DescriptionArgumentName);
	}
	if (!SelectionModeDescriptionArgumentName.IsNone())
	{
		OutNames.Add(SelectionModeDescriptionArgumentName);
	}
}

void USelectExhaustHandCardEffect::BuildPreviewArguments(
	const FCardEffectPreviewContext& Context,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	if (DescriptionArgumentName.IsNone() && SelectionModeDescriptionArgumentName.IsNone())
	{
		return;
	}

	const bool bIsUpgraded = IsValid(Context.Card) && Context.Card->IsUpgraded();
	if (!DescriptionArgumentName.IsNone())
	{
		OutArguments.AddInteger(
			DescriptionArgumentName,
			GetEffectiveSelectionCount(bIsUpgraded)
		);
	}
	if (!SelectionModeDescriptionArgumentName.IsNone())
	{
		OutArguments.AddText(
			SelectionModeDescriptionArgumentName,
			GetSelectionModeDescriptionText(GetEffectiveSelectionMode(bIsUpgraded))
		);
	}
}

void USelectExhaustHandCardEffect::ValidatePreviewConfiguration(TArray<FText>& OutErrors) const
{
	// NAME_None remains a deliberate legacy-compatible dynamic-text opt-out for
	// both count and selection-mode text. When a semantic name is authored, the
	// shared card-text validator enforces that the description template uses it.
	if (BaseSelectionCount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("SelectExhaustHandCardEffect BaseSelectionCount cannot be negative.")));
	}
	if (UpgradedSelectionCount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("SelectExhaustHandCardEffect UpgradedSelectionCount cannot be negative.")));
	}
}
