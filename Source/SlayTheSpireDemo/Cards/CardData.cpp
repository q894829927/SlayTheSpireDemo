#include "CardData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "../Battle/BattleTextResolver.h"

EDataValidationResult UCardData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bIsValid = ParentResult != EDataValidationResult::Invalid;

	if (BaseCost < 0)
	{
		Context.AddError(FText::FromString(TEXT("Card BaseCost cannot be negative.")));
		bIsValid = false;
	}
	if (UpgradedCost < 0)
	{
		Context.AddError(FText::FromString(TEXT("Card UpgradedCost cannot be negative.")));
		bIsValid = false;
	}

	TArray<FText> Errors;
	if (!FBattleTextResolver::ValidateCardDefinition(this, Errors))
	{
		for (const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		bIsValid = false;
	}

	return bIsValid
		? EDataValidationResult::Valid
		: EDataValidationResult::Invalid;
}
#endif
