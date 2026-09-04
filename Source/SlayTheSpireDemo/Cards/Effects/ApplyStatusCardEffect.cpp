#include "ApplyStatusCardEffect.h"

#include "../CardInstance.h"
#include "../CardPlayContext.h"
#include "../../Actions/ApplyStatusAction.h"
#include "../../Battle/BattleManager.h"
#include "../../Combat/Combatant.h"
#include "../../Status/StatusData.h"
#include "../../Battle/BattleTextTypes.h"

int32 UApplyStatusCardEffect::GetEffectiveAmount(bool bIsUpgraded) const
{
	return bIsUpgraded ? UpgradedAmount : Amount;
}

void UApplyStatusCardEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.ActionOuter) ||
		!IsValid(Context.Battle) ||
		!IsValid(Context.Source) ||
		!IsValid(Context.Target) ||
		!IsValid(StatusDefinition))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] ApplyStatus build skipped: invalid dependency."));
		return;
	}

	const bool bIsUpgraded = IsValid(Context.Card) && Context.Card->IsUpgraded();
	const int32 EffectiveAmount = GetEffectiveAmount(bIsUpgraded);
	if (EffectiveAmount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] ApplyStatus build skipped: effective Amount=%d."), EffectiveAmount);
		return;
	}

	UApplyStatusAction* Action = NewObject<UApplyStatusAction>(Context.ActionOuter);
	Action->Initialize(
		Context.Battle,
		Context.Source,
		Context.Target,
		StatusDefinition,
		EffectiveAmount
	);
	OutActions.Add(Action);
}

void UApplyStatusCardEffect::GetPreviewArgumentNames(TArray<FName>& OutNames) const
{
	OutNames.Add(DescriptionArgumentName);
}

void UApplyStatusCardEffect::BuildPreviewArguments(
	const FCardEffectPreviewContext& Context,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	const bool bIsUpgraded = IsValid(Context.Card) && Context.Card->IsUpgraded();
	OutArguments.AddInteger(DescriptionArgumentName, GetEffectiveAmount(bIsUpgraded));
}

void UApplyStatusCardEffect::ValidatePreviewConfiguration(TArray<FText>& OutErrors) const
{
	if (DescriptionArgumentName.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("ApplyStatusCardEffect requires a DescriptionArgumentName.")));
	}
	if (!IsValid(StatusDefinition))
	{
		OutErrors.Add(FText::FromString(TEXT("ApplyStatusCardEffect requires a StatusDefinition.")));
	}
	if (Amount <= 0)
	{
		OutErrors.Add(FText::FromString(TEXT("ApplyStatusCardEffect Amount must be positive.")));
	}
	if (UpgradedAmount <= 0)
	{
		OutErrors.Add(FText::FromString(TEXT("ApplyStatusCardEffect UpgradedAmount must be positive.")));
	}
}
