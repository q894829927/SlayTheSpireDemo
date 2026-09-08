#include "SelectExhaustHandCardEffect.h"

#include "../CardInstance.h"
#include "../CardPlayContext.h"
#include "../../Actions/BattleActionQueue.h"
#include "../../Actions/RandomSelectionAction.h"
#include "../../Actions/DeferredSelectionAction.h"
#include "../../Selection/SelectionCandidateSource.h"
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

	UCurrentHandSelectionSource* CandidateSource = NewObject<UCurrentHandSelectionSource>(Context.ActionOuter);
	CandidateSource->Initialize(Context.Deck);

	UExhaustSelectedContinuation* Continuation = NewObject<UExhaustSelectedContinuation>(Context.ActionOuter);
	Continuation->Initialize(
		Context.Deck,
		Context.Source,
		Context.EventDispatcher,
		Context.EventCombatants
	);

	FSelectionInteractiveBoundaryAccess Boundary;
	Boundary.BindUObject(Context.Battle, &ABattleManager::AdvancePresentationAtInteractiveSelectionBoundary);
	FSelectionRandomIndexChooser Random;
	Random.BindUObject(Context.Deck, &UDeckRuntime::TryChooseRandomIndex);
	UDeferredSelectionAction* Action = NewObject<UDeferredSelectionAction>(Context.ActionOuter);
	Action->Initialize(CandidateSource, Context.Battle->GetSelectionResolver(), Continuation,
		AuthoredSelectionCount, ESelectionCancelPolicy::Forbidden, TEXT("SelectExhaustHandCard"),
		EffectiveMode == ESelectExhaustSelectionMode::Random ? EDeferredSelectionMode::Random : EDeferredSelectionMode::Player,
		Boundary, Random);
	OutActions.Add(Action);
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

FText USelectExhaustHandCardEffect::GetAutoDescriptionFormat(const FCardEffectPreviewContext& Context) const
{
	const bool bIsUpgraded = IsValid(Context.Card) && Context.Card->IsUpgraded();
	switch (GetEffectiveSelectionMode(bIsUpgraded))
	{
	case ESelectExhaustSelectionMode::Random:
		return NSLOCTEXT(
			"CardEffect",
			"SelectExhaustRandomDescriptionFormat",
			"消耗 {Count} 张随机手牌。");
	case ESelectExhaustSelectionMode::Player:
	default:
		return NSLOCTEXT(
			"CardEffect",
			"SelectExhaustPlayerDescriptionFormat",
			"消耗 {Count} 张手牌。");
	}
}

void USelectExhaustHandCardEffect::BuildAutoDescriptionArguments(
	const FCardEffectPreviewContext& Context,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	const bool bIsUpgraded = IsValid(Context.Card) && Context.Card->IsUpgraded();
	OutArguments.AddInteger(
		FName(TEXT("Count")),
		GetEffectiveSelectionCount(bIsUpgraded));
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
