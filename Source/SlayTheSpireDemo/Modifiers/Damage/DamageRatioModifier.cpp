#include "DamageRatioModifier.h"

#include "DamageSpec.h"
#include "../../Status/StatusInstance.h"
#include "../../Battle/BattleTextTypes.h"

void UDamageRatioModifier::Apply(const UStatusInstance* StatusInstance, FDamageSpec& Spec) const
{
	if (!IsValid(StatusInstance) || StatusInstance->GetAmount() <= 0)
	{
		return;
	}

	if (Phase != EDamageModifierPhase::SourceMultiplier && Phase != EDamageModifierPhase::TargetMultiplier)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[DamageModifier] %s Ratio skipped: invalid Phase=%s."),
			*StatusInstance->GetDebugLabel(),
			DamageModifierPhaseToString(Phase)
		);
		return;
	}

	if (Numerator < 0 || Denominator <= 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[DamageModifier] %s Ratio skipped: Numerator=%d Denominator=%d."),
			*StatusInstance->GetDebugLabel(),
			Numerator,
			Denominator
		);
		return;
	}

	const int32 Before = Spec.WorkingAmount;
	const int32 ApplicationCount = AmountMode == EModifierAmountMode::ScaleWithAmount
		? StatusInstance->GetAmount()
		: 1;

	for (int32 ApplicationIndex = 0; ApplicationIndex < ApplicationCount; ++ApplicationIndex)
	{
		const int64 RawResult =
			static_cast<int64>(Spec.WorkingAmount)
			* static_cast<int64>(Numerator)
			/ static_cast<int64>(Denominator);

		const int64 ClampedResult = FMath::Clamp<int64>(
			RawResult,
			static_cast<int64>(0),
			static_cast<int64>(MAX_int32)
		);
		Spec.WorkingAmount = static_cast<int32>(ClampedResult);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[DamageModifier] %s Ratio %s: %d -> %d (Numerator=%d Denominator=%d Amount=%d Mode=%s Applications=%d)"),
		*StatusInstance->GetDebugLabel(),
		DamageModifierPhaseToString(Phase),
		Before,
		Spec.WorkingAmount,
		Numerator,
		Denominator,
		StatusInstance->GetAmount(),
		AmountMode == EModifierAmountMode::ScaleWithAmount ? TEXT("ScaleWithAmount") : TEXT("PresenceOnly"),
		ApplicationCount
	);
}

void UDamageRatioModifier::GetDescriptionArgumentNames(TArray<FName>& OutNames) const
{
	OutNames.Add(DescriptionArgumentName);
}

void UDamageRatioModifier::BuildDescriptionArguments(
	const UStatusInstance* StatusInstance,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	if (!IsValid(StatusInstance) || StatusInstance->GetAmount() <= 0)
	{
		OutArguments.AddUnknown(DescriptionArgumentName, TEXT("Damage Ratio description has no active StatusInstance."));
		return;
	}

	// This is the configured per-application ratio. ScaleWithAmount remains
	// represented by this percentage plus the reserved {Amount}; it must not
	// claim an exact cumulative percentage because gameplay floors each step.
	OutArguments.AddPercentMagnitude(DescriptionArgumentName, Numerator, Denominator);
}

void UDamageRatioModifier::ValidateDescriptionConfiguration(TArray<FText>& OutErrors) const
{
	if (DescriptionArgumentName.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("DamageRatioModifier requires a DescriptionArgumentName.")));
	}
	if (Numerator < 0 || Denominator <= 0)
	{
		OutErrors.Add(FText::FromString(TEXT("DamageRatioModifier requires Numerator >= 0 and Denominator > 0.")));
	}
}
