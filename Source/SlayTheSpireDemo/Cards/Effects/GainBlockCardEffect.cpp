#include "GainBlockCardEffect.h"

#include "../CardPlayContext.h"
#include "../../Actions/GainBlockAction.h"
#include "../../Combat/Combatant.h"
#include "../../Modifiers/Block/BlockModifierPipeline.h"
#include "../../Modifiers/Block/BlockSpec.h"
#include "../../Battle/BattleTextTypes.h"

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
	OutArguments.AddInteger(DescriptionArgumentName, Spec.ResolvedAmount);
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
}
