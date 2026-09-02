#include "BattleTextResolver.h"

#include "BattleImmediatePreview.h"
#include "BattleTextTypes.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"
#include "../Cards/Effects/CardEffect.h"
#include "../Combat/Combatant.h"
#include "../Modifiers/Block/BlockModifier.h"
#include "../Modifiers/Damage/DamageModifier.h"
#include "../Status/StatusData.h"
#include "../Status/StatusInstance.h"

namespace
{
	const TCHAR* IncreasedPreviewStyle = TEXT("PreviewIncrease");
	const TCHAR* DecreasedPreviewStyle = TEXT("PreviewDecrease");

	void ExtractNamedArgumentStrings(const FText& Format, TArray<FString>& OutNames)
	{
		OutNames.Reset();
		const FString Pattern = Format.ToString();
		for (int32 Index = 0; Index < Pattern.Len(); ++Index)
		{
			if (Pattern[Index] != TCHAR('{'))
			{
				continue;
			}

			if (Index + 1 < Pattern.Len() && Pattern[Index + 1] == TCHAR('{'))
			{
				++Index;
				continue;
			}

			const int32 CloseIndex = Pattern.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + 1);
			if (CloseIndex == INDEX_NONE)
			{
				break;
			}

			FString NameString = Pattern.Mid(Index + 1, CloseIndex - Index - 1).TrimStartAndEnd();
			int32 ModifierIndex = INDEX_NONE;
			if (NameString.FindChar(TCHAR('|'), ModifierIndex) || NameString.FindChar(TCHAR(':'), ModifierIndex))
			{
				NameString.LeftInline(ModifierIndex);
				NameString.TrimStartAndEndInline();
			}

			if (!NameString.IsEmpty())
			{
				const bool bAlreadyPresent = OutNames.ContainsByPredicate(
					[&NameString](const FString& ExistingName)
					{
						return ExistingName.Equals(NameString, ESearchCase::CaseSensitive);
					}
				);
				if (!bAlreadyPresent)
				{
					OutNames.Add(MoveTemp(NameString));
				}
			}
			Index = CloseIndex;
		}
	}

	void ExtractNamedArguments(const FText& Format, TSet<FName>& OutNames)
	{
		OutNames.Reset();
		TArray<FString> ExactNames;
		ExtractNamedArgumentStrings(Format, ExactNames);
		for (const FString& ExactName : ExactNames)
		{
			OutNames.Add(FName(*ExactName));
		}
	}

	FText FormatDescription(
		const FText& Format,
		FPreviewTextArgumentBuilder& Builder,
		const FString& DebugOwner,
		const TMap<FName, FText>* RichOverrides = nullptr
	)
	{
		if (Format.IsEmpty())
		{
			return FText::GetEmpty();
		}

		TArray<FString> RequiredKeys;
		ExtractNamedArgumentStrings(Format, RequiredKeys);
		FFormatNamedArguments ExactArguments;
		for (const FString& RequiredKey : RequiredKeys)
		{
			const FName SemanticName(*RequiredKey);
			if (!Builder.Contains(SemanticName))
			{
				Builder.AddUnknown(
					SemanticName,
					FString::Printf(TEXT("%s description references unknown argument '%s'."), *DebugOwner, *RequiredKey)
				);
			}

			if (RichOverrides != nullptr)
			{
				if (const FText* RichValue = RichOverrides->Find(SemanticName))
				{
					// RichText styling is attached to the semantic argument itself, not
					// discovered by searching the already-formatted sentence for digits.
					ExactArguments.Add(RequiredKey, FFormatArgumentValue(*RichValue));
					continue;
				}
			}

			if (const FFormatArgumentValue* Value = Builder.FindValue(SemanticName))
			{
				// Preserve the exact spelling from the active (possibly localized)
				// format pattern. FText named argument lookup is case-sensitive.
				ExactArguments.Add(RequiredKey, *Value);
			}
		}

		for (const FString& Error : Builder.GetErrors())
		{
			UE_LOG(LogTemp, Error, TEXT("[BattleText] %s"), *Error);
		}

		return FText::Format(Format, ExactArguments);
	}

	bool BuildCardDescriptionArguments(
		const UCardInstance* Card,
		ACombatant* Source,
		FPreviewTextArgumentBuilder& Builder)
	{
		if (!IsValid(Card))
		{
			return false;
		}

		const UCardData* Definition = Card->GetDefinition();
		if (!IsValid(Definition))
		{
			return false;
		}

		Builder.AddInteger(TEXT("Cost"), Card->GetCurrentCost());

		FCardEffectPreviewContext Context;
		Context.Card = Card;
		Context.Source = Source;
		Context.Target = Definition->TargetType == ECardTargetType::Self ? Source : nullptr;

		for (const TObjectPtr<UCardEffect>& EffectPtr : Definition->Effects)
		{
			const UCardEffect* Effect = EffectPtr.Get();
			if (!IsValid(Effect))
			{
				Builder.AddError(FString::Printf(TEXT("Card %s contains an invalid Effect."), *Card->GetDebugLabel()));
				continue;
			}
			Effect->BuildPreviewArguments(Context, Builder);
		}
		return true;
	}

	void BuildRichPreviewOverrides(
		const TArray<FImmediatePreviewOperation>& Operations,
		TMap<FName, FText>& OutOverrides)
	{
		OutOverrides.Reset();
		for (const FImmediatePreviewOperation& Operation : Operations)
		{
			const TCHAR* StyleName = nullptr;
			if (Operation.ResolvedAmount > Operation.BaseAmount)
			{
				StyleName = IncreasedPreviewStyle;
			}
			else if (Operation.ResolvedAmount < Operation.BaseAmount)
			{
				StyleName = DecreasedPreviewStyle;
			}

			if (StyleName == nullptr || Operation.SemanticArgumentName.IsNone())
			{
				continue;
			}

			const FString ResolvedNumber = FText::AsNumber(Operation.ResolvedAmount).ToString();
			OutOverrides.Add(
				Operation.SemanticArgumentName,
				FText::FromString(FString::Printf(TEXT("<%s>%s</>"), StyleName, *ResolvedNumber))
			);
		}
	}

	void AddValidationError(TArray<FText>& OutErrors, const FString& Error)
	{
		OutErrors.Add(FText::FromString(Error));
	}

	void ValidateRequiredArguments(
		const FText& Format,
		const TArray<FName>& DeclaredNames,
		const TSet<FName>& OptionalNames,
		const FString& DebugOwner,
		TArray<FText>& OutErrors
	)
	{
		TSet<FName> FormatNames;
		ExtractNamedArguments(Format, FormatNames);
		TSet<FName> UniqueDeclared;

		for (const FName Name : DeclaredNames)
		{
			if (Name.IsNone())
			{
				AddValidationError(OutErrors, FString::Printf(TEXT("%s has an unnamed dynamic description value."), *DebugOwner));
				continue;
			}

			if (UniqueDeclared.Contains(Name))
			{
				AddValidationError(
					OutErrors,
					FString::Printf(TEXT("%s declares duplicate description argument '%s'."), *DebugOwner, *Name.ToString())
				);
				continue;
			}

			if (OptionalNames.Contains(Name))
			{
				AddValidationError(
					OutErrors,
					FString::Printf(TEXT("%s declares reserved description argument '%s'."), *DebugOwner, *Name.ToString())
				);
				continue;
			}

			UniqueDeclared.Add(Name);
			if (!FormatNames.Contains(Name))
			{
				AddValidationError(
					OutErrors,
					FString::Printf(TEXT("%s description does not use dynamic argument '%s'."), *DebugOwner, *Name.ToString())
				);
			}
		}

		for (const FName FormatName : FormatNames)
		{
			if (!UniqueDeclared.Contains(FormatName) && !OptionalNames.Contains(FormatName))
			{
				AddValidationError(
					OutErrors,
					FString::Printf(TEXT("%s description references unknown argument '%s'."), *DebugOwner, *FormatName.ToString())
				);
			}
		}
	}
}

FText FBattleTextResolver::ResolveCardDescription(const UCardInstance* Card, ACombatant* Source)
{
	if (!IsValid(Card) || !IsValid(Card->GetDefinition()))
	{
		return FText::GetEmpty();
	}

	FPreviewTextArgumentBuilder Builder;
	if (!BuildCardDescriptionArguments(Card, Source, Builder))
	{
		return FText::GetEmpty();
	}
	return FormatDescription(Card->GetDefinition()->Description, Builder, Card->GetDebugLabel());
}

FText FBattleTextResolver::ResolveCardDescriptionForImmediatePreview(
	const UCardInstance* Card,
	ACombatant* Source,
	const TArray<FImmediatePreviewOperation>& Operations)
{
	if (!IsValid(Card) || !IsValid(Card->GetDefinition()))
	{
		return FText::GetEmpty();
	}

	FPreviewTextArgumentBuilder Builder;
	if (!BuildCardDescriptionArguments(Card, Source, Builder))
	{
		return FText::GetEmpty();
	}

	for (const FImmediatePreviewOperation& Operation : Operations)
	{
		Builder.OverrideInteger(Operation.SemanticArgumentName, Operation.ResolvedAmount);
	}

	return FormatDescription(Card->GetDefinition()->Description, Builder, Card->GetDebugLabel());
}

FText FBattleTextResolver::ResolveCardRichDescriptionForImmediatePreview(
	const UCardInstance* Card,
	ACombatant* Source,
	const TArray<FImmediatePreviewOperation>& Operations)
{
	if (!IsValid(Card) || !IsValid(Card->GetDefinition()))
	{
		return FText::GetEmpty();
	}

	FPreviewTextArgumentBuilder Builder;
	if (!BuildCardDescriptionArguments(Card, Source, Builder))
	{
		return FText::GetEmpty();
	}

	for (const FImmediatePreviewOperation& Operation : Operations)
	{
		Builder.OverrideInteger(Operation.SemanticArgumentName, Operation.ResolvedAmount);
	}

	TMap<FName, FText> RichOverrides;
	BuildRichPreviewOverrides(Operations, RichOverrides);
	return FormatDescription(
		Card->GetDefinition()->Description,
		Builder,
		Card->GetDebugLabel(),
		&RichOverrides);
}

FText FBattleTextResolver::ResolveStatusDescription(const UStatusInstance* StatusInstance)
{
	if (!IsValid(StatusInstance))
	{
		return FText::GetEmpty();
	}

	const UStatusData* Definition = StatusInstance->GetDefinition();
	if (!IsValid(Definition))
	{
		return FText::GetEmpty();
	}

	FPreviewTextArgumentBuilder Builder;
	Builder.AddInteger(TEXT("Amount"), StatusInstance->GetAmount());

	for (const TObjectPtr<UDamageModifier>& ModifierPtr : Definition->DamageModifiers)
	{
		if (const UDamageModifier* Modifier = ModifierPtr.Get())
		{
			Modifier->BuildDescriptionArguments(StatusInstance, Builder);
		}
	}

	for (const TObjectPtr<UBlockModifier>& ModifierPtr : Definition->BlockModifiers)
	{
		if (const UBlockModifier* Modifier = ModifierPtr.Get())
		{
			Modifier->BuildDescriptionArguments(StatusInstance, Builder);
		}
	}

	return FormatDescription(Definition->Description, Builder, StatusInstance->GetDebugLabel());
}

bool FBattleTextResolver::ValidateCardDefinition(const UCardData* Definition, TArray<FText>& OutErrors)
{
	OutErrors.Reset();
	if (!IsValid(Definition))
	{
		AddValidationError(OutErrors, TEXT("Card definition is invalid."));
		return false;
	}

	TArray<FName> DeclaredNames;
	for (const TObjectPtr<UCardEffect>& EffectPtr : Definition->Effects)
	{
		const UCardEffect* Effect = EffectPtr.Get();
		if (!IsValid(Effect))
		{
			AddValidationError(OutErrors, FString::Printf(TEXT("Card %s contains an invalid Effect."), *Definition->CardId.ToString()));
			continue;
		}
		Effect->GetPreviewArgumentNames(DeclaredNames);
		Effect->ValidatePreviewConfiguration(OutErrors);
	}

	TSet<FName> OptionalNames;
	OptionalNames.Add(TEXT("Cost"));
	ValidateRequiredArguments(
		Definition->Description,
		DeclaredNames,
		OptionalNames,
		FString::Printf(TEXT("Card %s"), *Definition->CardId.ToString()),
		OutErrors
	);
	return OutErrors.Num() == 0;
}

bool FBattleTextResolver::ValidateStatusDefinition(const UStatusData* Definition, TArray<FText>& OutErrors)
{
	OutErrors.Reset();
	if (!IsValid(Definition))
	{
		AddValidationError(OutErrors, TEXT("Status definition is invalid."));
		return false;
	}

	TArray<FName> DeclaredNames;
	for (const TObjectPtr<UDamageModifier>& ModifierPtr : Definition->DamageModifiers)
	{
		const UDamageModifier* Modifier = ModifierPtr.Get();
		if (!IsValid(Modifier))
		{
			AddValidationError(OutErrors, FString::Printf(TEXT("Status %s contains an invalid DamageModifier."), *Definition->StatusId.ToString()));
			continue;
		}
		Modifier->GetDescriptionArgumentNames(DeclaredNames);
		Modifier->ValidateDescriptionConfiguration(OutErrors);
	}

	for (const TObjectPtr<UBlockModifier>& ModifierPtr : Definition->BlockModifiers)
	{
		const UBlockModifier* Modifier = ModifierPtr.Get();
		if (!IsValid(Modifier))
		{
			AddValidationError(OutErrors, FString::Printf(TEXT("Status %s contains an invalid BlockModifier."), *Definition->StatusId.ToString()));
			continue;
		}
		Modifier->GetDescriptionArgumentNames(DeclaredNames);
		Modifier->ValidateDescriptionConfiguration(OutErrors);
	}

	TSet<FName> OptionalNames;
	OptionalNames.Add(TEXT("Amount"));
	ValidateRequiredArguments(
		Definition->Description,
		DeclaredNames,
		OptionalNames,
		FString::Printf(TEXT("Status %s"), *Definition->StatusId.ToString()),
		OutErrors
	);
	return OutErrors.Num() == 0;
}
