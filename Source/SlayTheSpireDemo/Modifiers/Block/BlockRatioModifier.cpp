#include "BlockRatioModifier.h"

#include "BlockSpec.h"
#include "../../Status/StatusInstance.h"

void UBlockRatioModifier::Apply(const UStatusInstance* StatusInstance, FBlockSpec& Spec) const
{
	if (!IsValid(StatusInstance) || StatusInstance->GetAmount() <= 0)
	{
		return;
	}

	if (Numerator < 0 || Denominator <= 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BlockModifier] %s Ratio skipped: Numerator=%d Denominator=%d."),
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
		TEXT("[BlockModifier] %s Ratio Multiplier: %d -> %d (Numerator=%d Denominator=%d Amount=%d Mode=%s Applications=%d)"),
		*StatusInstance->GetDebugLabel(),
		Before,
		Spec.WorkingAmount,
		Numerator,
		Denominator,
		StatusInstance->GetAmount(),
		AmountMode == EModifierAmountMode::ScaleWithAmount ? TEXT("ScaleWithAmount") : TEXT("PresenceOnly"),
		ApplicationCount
	);
}
