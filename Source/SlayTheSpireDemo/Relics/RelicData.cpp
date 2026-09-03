#include "RelicData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult URelicData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bValid = ParentResult != EDataValidationResult::Invalid;

	if (RelicId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("RelicId must not be None.")));
		bValid = false;
	}

	if (bShowCounter && CounterDisplayMax <= 0)
	{
		Context.AddError(FText::FromString(TEXT("CounterDisplayMax must be greater than zero when bShowCounter is enabled.")));
		bValid = false;
	}

	return bValid
		? EDataValidationResult::Valid
		: EDataValidationResult::Invalid;
}
#endif
