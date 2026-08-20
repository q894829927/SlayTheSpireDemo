#include "CardData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "../Battle/BattleTextResolver.h"

EDataValidationResult UCardData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	TArray<FText> Errors;
	if (!FBattleTextResolver::ValidateCardDefinition(this, Errors))
	{
		for (const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		return EDataValidationResult::Invalid;
	}

	return ParentResult == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif
