#include "SelectHandCardToDrawPileTopEffect.h"

#include "../CardInstance.h"
#include "../CardPlayContext.h"
#include "../../Actions/DeferredSelectionAction.h"
#include "../../Selection/SelectionCandidateSource.h"
#include "../../Battle/BattleManager.h"
#include "../../Battle/BattleTextTypes.h"
#include "../../Deck/DeckRuntime.h"
#include "../../Selection/MoveSelectedHandCardsToDrawPileTopContinuation.h"
#include "../../Selection/SelectionResolver.h"

int32 USelectHandCardToDrawPileTopEffect::GetEffectiveSelectionCount(bool bIsUpgraded) const
{
	return bIsUpgraded ? UpgradedSelectionCount : BaseSelectionCount;
}

void USelectHandCardToDrawPileTopEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.ActionOuter)
		|| !IsValid(Context.Battle)
		|| !IsValid(Context.Deck)
		|| !IsValid(Context.Card))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] SelectHandCardToDrawPileTop build skipped: invalid ActionOuter, Battle, Deck or Card."));
		return;
	}

	const int32 EffectiveCount = GetEffectiveSelectionCount(Context.Card->IsUpgraded());
	if (EffectiveCount <= 0)
	{
		return;
	}

	USelectionResolver* Resolver = Context.Battle->GetSelectionResolver();
	if (!IsValid(Resolver))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] SelectHandCardToDrawPileTop build skipped: Battle has no SelectionResolver."));
		return;
	}

	UMoveSelectedHandCardsToDrawPileTopContinuation* Continuation =
		NewObject<UMoveSelectedHandCardsToDrawPileTopContinuation>(Context.ActionOuter);
	Continuation->Initialize(Context.Deck, Context.Source);

	UCurrentHandSelectionSource* CandidateSource = NewObject<UCurrentHandSelectionSource>(Context.ActionOuter);
	CandidateSource->Initialize(Context.Deck);
	FSelectionInteractiveBoundaryAccess Boundary;
	Boundary.BindUObject(Context.Battle, &ABattleManager::AdvancePresentationAtInteractiveSelectionBoundary);
	UDeferredSelectionAction* DeferredSelection = NewObject<UDeferredSelectionAction>(Context.ActionOuter);
	DeferredSelection->Initialize(CandidateSource, Resolver, Continuation, EffectiveCount,
		ESelectionCancelPolicy::Forbidden, TEXT("SelectHandCardToDrawPileTop"),
		EDeferredSelectionMode::Player, Boundary);
	OutActions.Add(DeferredSelection);
}

void USelectHandCardToDrawPileTopEffect::GetPreviewArgumentNames(TArray<FName>& OutNames) const
{
	OutNames.Add(DescriptionArgumentName);
}

void USelectHandCardToDrawPileTopEffect::BuildPreviewArguments(
	const FCardEffectPreviewContext& Context,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	const bool bIsUpgraded = IsValid(Context.Card) && Context.Card->IsUpgraded();
	OutArguments.AddInteger(DescriptionArgumentName, GetEffectiveSelectionCount(bIsUpgraded));
}

FText USelectHandCardToDrawPileTopEffect::GetAutoDescriptionFormat(const FCardEffectPreviewContext& Context) const
{
	return NSLOCTEXT(
		"CardEffect",
		"SelectHandCardToDrawPileTopDescriptionFormat",
		"将手牌中的 {Count} 张牌放到你的抽牌堆顶部。"
	);
}

void USelectHandCardToDrawPileTopEffect::BuildAutoDescriptionArguments(
	const FCardEffectPreviewContext& Context,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	const bool bIsUpgraded = IsValid(Context.Card) && Context.Card->IsUpgraded();
	OutArguments.AddInteger(FName(TEXT("Count")), GetEffectiveSelectionCount(bIsUpgraded));
}

void USelectHandCardToDrawPileTopEffect::ValidatePreviewConfiguration(TArray<FText>& OutErrors) const
{
	if (DescriptionArgumentName.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("SelectHandCardToDrawPileTopEffect requires a DescriptionArgumentName.")));
	}
	if (BaseSelectionCount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("SelectHandCardToDrawPileTopEffect BaseSelectionCount cannot be negative.")));
	}
	if (UpgradedSelectionCount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("SelectHandCardToDrawPileTopEffect UpgradedSelectionCount cannot be negative.")));
	}
}
