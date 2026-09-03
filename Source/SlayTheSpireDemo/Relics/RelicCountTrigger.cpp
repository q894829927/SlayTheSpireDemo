#include "RelicCountTrigger.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult URelicCountTrigger::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bValid = ParentResult != EDataValidationResult::Invalid;

	if (RequiredCount <= 0)
	{
		Context.AddError(FText::FromString(TEXT("RelicCountTrigger RequiredCount must be greater than zero.")));
		bValid = false;
	}

	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
