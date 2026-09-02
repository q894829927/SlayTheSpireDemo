#include "DamageCardEffect.h"

#include "../CardPlayContext.h"
#include "../../Actions/DamageAction.h"
#include "../../Combat/Combatant.h"
#include "../../Modifiers/Damage/DamageModifierPipeline.h"
#include "../../Modifiers/Damage/DamageSpec.h"
#include "../../Battle/BattleTextTypes.h"

void UDamageCardEffect::BuildActions(
	const FCardPlayContext& Context,
	TArray<UBattleAction*>& OutActions
) const
{
	if (!IsValid(Context.ActionOuter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] Damage build skipped: invalid ActionOuter."));
		return;
	}

	if (HitCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CardEffect] Damage build skipped: invalid HitCount=%d."), HitCount);
		return;
	}

	for (int32 HitIndex = 0; HitIndex < HitCount; ++HitIndex)
	{
		UDamageAction* Action = NewObject<UDamageAction>(Context.ActionOuter);
		Action->Initialize(Context.Source, Context.Target, BaseAmount, DamageKind);
		Action->SetPresentationParticipantIds(
			Context.SourcePresentationId,
			Context.TargetPresentationId
		);
		OutActions.Add(Action);
	}
}

void UDamageCardEffect::GetPreviewArgumentNames(TArray<FName>& OutNames) const
{
	OutNames.Add(DescriptionArgumentName);
}

void UDamageCardEffect::BuildPreviewArguments(
	const FCardEffectPreviewContext& Context,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	if (!IsValid(Context.Source))
	{
		OutArguments.AddUnknown(DescriptionArgumentName, TEXT("Damage preview has no valid Source."));
		return;
	}

	FDamageSpec Spec;
	Spec.Source = Context.Source;
	Spec.Target = Context.Target;
	Spec.DamageKind = DamageKind;
	Spec.BaseAmount = BaseAmount;
	FDamageModifierPipeline::Resolve(Spec);
	OutArguments.AddInteger(DescriptionArgumentName, Spec.ResolvedAmount);
}

void UDamageCardEffect::ValidatePreviewConfiguration(TArray<FText>& OutErrors) const
{
	if (DescriptionArgumentName.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("DamageCardEffect requires a DescriptionArgumentName.")));
	}
	if (BaseAmount < 0)
	{
		OutErrors.Add(FText::FromString(TEXT("DamageCardEffect BaseAmount cannot be negative.")));
	}
	if (HitCount <= 0)
	{
		OutErrors.Add(FText::FromString(TEXT("DamageCardEffect HitCount must be greater than zero.")));
	}
}

void UDamageCardEffect::BuildImmediatePreviewOperations(
	const FCardEffectPreviewContext& Context,
	int32 EffectIndex,
	TArray<FImmediatePreviewOperation>& OutOperations
) const
{
	if (!IsValid(Context.Source)
		|| !IsValid(Context.Target)
		|| DescriptionArgumentName.IsNone()
		|| BaseAmount < 0
		|| HitCount <= 0)
	{
		return;
	}

	FDamageSpec Spec;
	Spec.Source = Context.Source;
	Spec.Target = Context.Target;
	Spec.DamageKind = DamageKind;
	Spec.BaseAmount = BaseAmount;
	FDamageModifierPipeline::Resolve(Spec);

	FImmediatePreviewOperation Operation;
	Operation.EffectIndex = EffectIndex;
	Operation.SemanticArgumentName = DescriptionArgumentName;
	Operation.Type = EImmediatePreviewOperationType::Damage;
	Operation.BaseAmount = BaseAmount;
	Operation.ResolvedAmount = Spec.ResolvedAmount;
	Operation.HitCount = HitCount;
	OutOperations.Add(Operation);
}
