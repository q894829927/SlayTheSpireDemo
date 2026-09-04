#include "GainBlockCardEffect.h"

#include "../CardPlayContext.h"
#include "../../Actions/GainBlockAction.h"
#include "../../Combat/Combatant.h"
#include "../../Modifiers/Block/BlockModifierPipeline.h"
#include "../../Modifiers/Block/BlockSpec.h"
#include "../../Battle/BattleTextTypes.h"

int32 UGainBlockCardEffect::GetEffectiveAmount(bool bIsUpgraded) const
{
	return bIsUpgraded ? UpgradedAmount : BaseAmount;
}

void UGainBlockCardEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.ActionOuter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] Block build skipped: invalid ActionOuter."));
		return;
	}

	UGainBlockAction* Action = NewObject<UGainBlockAction>(Context.ActionOuter);
	Action->Initialize(Context.Source, Context.Target, BaseAmount);
	Action->SetPresentationParticipantIds(
		Context.SourcePresentationId,
		Context.TargetPresentationId
	);
	OutActions.Add(Action);
}

void UGainBlockCardEffect::GetPreviewArgumentNames(TArray<FName>& OutNames) const
{
	OutNames.Add(DescriptionArgumentName);
}

void UGainBlockCardEffect::BuildPreviewArguments(
	const FCardEffectPreviewContext& Context,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	if (!IsValid(Context.Source) || !IsValid(Context.Target))
	{
		OutArguments.AddUnknown(DescriptionArgumentName, TEXT("Block preview requires a valid self target."));
		return;
	}

	FBlockSpec Spec;
	Spec.Source = Context.Source;
	Spec.Target = Context.Target;
	Spec.BaseAmount = BaseAmount;
	FBlockModifierPipeline::Resolve(Spec);
	OutArguments.AddIntegerWithAuthoredBase(
		DescriptionArgumentName,
		Spec.ResolvedAmount,
		BaseAmount);
}

void UGainBlockCardEffect::ValidatePreviewConfiguration(TArray<FText>& OutErrors) const
{
	if (DescriptionArgumentName.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("GainBlockCardEffect requires a DescriptionArgumentName.")));
	}
	if (BaseAmount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("GainBlockCardEffect BaseAmount cannot be negative.")));
	}
	if (UpgradedAmount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("GainBlockCardEffect UpgradedAmount cannot be negative.")));
	}
}

void UGainBlockCardEffect::BuildImmediatePreviewOperations(
	const FCardEffectPreviewContext& Context,
	int32 EffectIndex,
	TArray<FImmediatePreviewOperation>& OutOperations
) const
{
	if (!IsValid(Context.Source)
		|| DescriptionArgumentName.IsNone()
		|| BaseAmount < 0)
	{
		return;
	}

	FBlockSpec Spec;
	Spec.Source = Context.Source;
	Spec.Target = Context.Source;
	Spec.BaseAmount = BaseAmount;
	FBlockModifierPipeline::Resolve(Spec);

	FImmediatePreviewOperation Operation;
	Operation.EffectIndex = EffectIndex;
	Operation.SemanticArgumentName = DescriptionArgumentName;
	Operation.Type = EImmediatePreviewOperationType::Block;
	Operation.BaseAmount = BaseAmount;
	Operation.ResolvedAmount = Spec.ResolvedAmount;
	Operation.HitCount = 1;
	OutOperations.Add(Operation);
}
