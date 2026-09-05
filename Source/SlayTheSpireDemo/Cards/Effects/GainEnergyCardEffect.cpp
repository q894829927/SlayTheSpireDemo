#include "GainEnergyCardEffect.h"

#include "../CardInstance.h"
#include "../CardPlayContext.h"
#include "../../Actions/GainEnergyAction.h"
#include "../../Battle/BattleTextTypes.h"

int32 UGainEnergyCardEffect::GetEffectiveAmount(bool bIsUpgraded) const
{
	return bIsUpgraded ? UpgradedAmount : BaseAmount;
}

void UGainEnergyCardEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.ActionOuter) || !IsValid(Context.Battle))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] GainEnergy build skipped: invalid ActionOuter or Battle."));
		return;
	}

	const bool bIsUpgraded = IsValid(Context.Card) && Context.Card->IsUpgraded();
	const int32 EffectiveAmount = GetEffectiveAmount(bIsUpgraded);
	if (EffectiveAmount <= 0)
	{
		return;
	}

	UGainEnergyAction* Action = NewObject<UGainEnergyAction>(Context.ActionOuter);
	Action->Initialize(Context.Battle, EffectiveAmount);
	OutActions.Add(Action);
}

void UGainEnergyCardEffect::GetPreviewArgumentNames(TArray<FName>& OutNames) const
{
	OutNames.Add(DescriptionArgumentName);
}

void UGainEnergyCardEffect::BuildPreviewArguments(
	const FCardEffectPreviewContext& Context,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	const bool bIsUpgraded = IsValid(Context.Card) && Context.Card->IsUpgraded();
	OutArguments.AddInteger(DescriptionArgumentName, GetEffectiveAmount(bIsUpgraded));
}

void UGainEnergyCardEffect::ValidatePreviewConfiguration(TArray<FText>& OutErrors) const
{
	if (DescriptionArgumentName.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("GainEnergyCardEffect requires a DescriptionArgumentName.")));
	}
	if (BaseAmount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("GainEnergyCardEffect BaseAmount cannot be negative.")));
	}
	if (UpgradedAmount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("GainEnergyCardEffect UpgradedAmount cannot be negative.")));
	}
}
