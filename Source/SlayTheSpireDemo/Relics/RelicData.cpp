#include "RelicData.h"

#include "RelicCountTrigger.h"

bool URelicData::TryGetCounterMax(int32& OutCounterMax) const
{
	OutCounterMax = 0;
	const URelicCountTrigger* FoundCountTrigger = nullptr;

	for (const TObjectPtr<UBattleTrigger>& TriggerPtr : Triggers)
	{
		const URelicCountTrigger* CountTrigger = Cast<URelicCountTrigger>(TriggerPtr.Get());
		if (!IsValid(CountTrigger))
		{
			continue;
		}

		if (FoundCountTrigger != nullptr)
		{
			return false;
		}

		FoundCountTrigger = CountTrigger;
	}

	if (FoundCountTrigger == nullptr || FoundCountTrigger->GetRequiredCount() <= 0)
	{
		return false;
	}

	OutCounterMax = FoundCountTrigger->GetRequiredCount();
	return true;
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult URelicData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bValid = ParentResult != EDataValidationResult::Invalid;
	int32 CountTriggerCount = 0;

	if (RelicId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("RelicId must not be None.")));
		bValid = false;
	}

	for (int32 Index = 0; Index < Triggers.Num(); ++Index)
	{
		const UBattleTrigger* Trigger = Triggers[Index].Get();
		if (!IsValid(Trigger))
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Relic contains an invalid Trigger at index %d."),
				Index)));
			bValid = false;
			continue;
		}

		if (IsValid(Cast<URelicCountTrigger>(Trigger)))
		{
			++CountTriggerCount;
		}

		if (Trigger->IsDataValid(Context) == EDataValidationResult::Invalid)
		{
			bValid = false;
		}
	}

	if (CountTriggerCount > 1)
	{
		Context.AddError(FText::FromString(TEXT("Relic may contain at most one RelicCountTrigger because URelicInstance owns one Counter.")));
		bValid = false;
	}

	int32 CounterMax = 0;
	if (bShowCounter && !TryGetCounterMax(CounterMax))
	{
		Context.AddError(FText::FromString(TEXT("bShowCounter requires exactly one valid RelicCountTrigger with RequiredCount greater than zero.")));
		bValid = false;
	}

	return bValid
		? EDataValidationResult::Valid
		: EDataValidationResult::Invalid;
}
#endif
