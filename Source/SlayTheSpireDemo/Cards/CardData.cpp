#include "CardData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "../Battle/BattleTextResolver.h"

namespace
{
	bool IsAuthoredCardRarityValid(ECardRarity Rarity)
	{
		switch (Rarity)
		{
		case ECardRarity::Basic:
		case ECardRarity::Common:
		case ECardRarity::Uncommon:
		case ECardRarity::Rare:
		case ECardRarity::Special:
		case ECardRarity::Curse:
			return true;
		default:
			return false;
		}
	}

	bool IsAuthoredCardColorValid(ECardColor CardColor)
	{
		switch (CardColor)
		{
		case ECardColor::Red:
		case ECardColor::Green:
		case ECardColor::Blue:
		case ECardColor::Purple:
		case ECardColor::Colorless:
		case ECardColor::Curse:
			return true;
		default:
			return false;
		}
	}
}

EDataValidationResult UCardData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bIsValid = ParentResult != EDataValidationResult::Invalid;

	if (!IsAuthoredCardRarityValid(Rarity))
	{
		Context.AddError(FText::FromString(TEXT("Card Rarity contains an invalid enum value.")));
		bIsValid = false;
	}
	if (!IsAuthoredCardColorValid(CardColor))
	{
		Context.AddError(FText::FromString(TEXT("Card CardColor contains an invalid enum value.")));
		bIsValid = false;
	}
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
