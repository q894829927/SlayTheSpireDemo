#include "RelicData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult URelicData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	if (RelicId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("RelicId must not be None.")));
		return EDataValidationResult::Invalid;
	}

	return ParentResult == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif
