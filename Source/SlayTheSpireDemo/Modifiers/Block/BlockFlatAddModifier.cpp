#include "BlockFlatAddModifier.h"

#include "BlockSpec.h"
#include "../../Status/StatusInstance.h"
#include "../../Battle/BattleTextTypes.h"

void UBlockFlatAddModifier::Apply(const UStatusInstance* StatusInstance, FBlockSpec& Spec) const
{
	if (!IsValid(StatusInstance) || StatusInstance->GetAmount() <= 0)
	{
		return;
	}

	const int32 Before = Spec.WorkingAmount;
	int64 Delta = static_cast<int64>(Value);
	if (AmountMode == EModifierAmountMode::ScaleWithAmount)
	{
		Delta *= static_cast<int64>(StatusInstance->GetAmount());
	}

	const int64 RawResult = static_cast<int64>(Spec.WorkingAmount) + Delta;
	const int64 ClampedResult = FMath::Clamp<int64>(
		RawResult,
		static_cast<int64>(0),
		static_cast<int64>(MAX_int32)
	);
	Spec.WorkingAmount = static_cast<int32>(ClampedResult);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[BlockModifier] %s FlatAdd: %d -> %d (Value=%d Amount=%d Mode=%s)"),
		*StatusInstance->GetDebugLabel(),
		Before,
		Spec.WorkingAmount,
		Value,
		StatusInstance->GetAmount(),
		AmountMode == EModifierAmountMode::ScaleWithAmount ? TEXT("ScaleWithAmount") : TEXT("PresenceOnly")
	);
}

void UBlockFlatAddModifier::GetDescriptionArgumentNames(TArray<FName>& OutNames) const
{
	OutNames.Add(DescriptionArgumentName);
}

void UBlockFlatAddModifier::BuildDescriptionArguments(
	const UStatusInstance* StatusInstance,
	FPreviewTextArgumentBuilder& OutArguments
) const
{
	if (!IsValid(StatusInstance) || StatusInstance->GetAmount() <= 0)
	{
		OutArguments.AddUnknown(DescriptionArgumentName, TEXT("Block FlatAdd description has no active StatusInstance."));
		return;
	}

	int64 Delta = static_cast<int64>(Value);
	if (AmountMode == EModifierAmountMode::ScaleWithAmount)
	{
		Delta *= static_cast<int64>(StatusInstance->GetAmount());
	}
	OutArguments.AddInteger(DescriptionArgumentName, Delta);
}

void UBlockFlatAddModifier::ValidateDescriptionConfiguration(TArray<FText>& OutErrors) const
{
	if (DescriptionArgumentName.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("BlockFlatAddModifier requires a DescriptionArgumentName.")));
	}
}
