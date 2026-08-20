#include "BattleTextTypes.h"

bool FPreviewTextArgumentBuilder::AddInteger(FName Name, int64 Value)
{
	return AddValue(Name, FFormatArgumentValue(Value));
}

bool FPreviewTextArgumentBuilder::AddNumber(FName Name, double Value)
{
	return AddValue(Name, FFormatArgumentValue(Value));
}

bool FPreviewTextArgumentBuilder::AddText(FName Name, const FText& Value)
{
	return AddValue(Name, FFormatArgumentValue(Value));
}

bool FPreviewTextArgumentBuilder::AddPercentMagnitude(FName Name, int32 Numerator, int32 Denominator)
{
	if (Numerator < 0 || Denominator <= 0)
	{
		AddUnknown(
			Name,
			FString::Printf(TEXT("Invalid ratio for '%s': %d/%d."), *Name.ToString(), Numerator, Denominator)
		);
		return false;
	}

	const double Percent =
		FMath::Abs(static_cast<double>(Numerator) - static_cast<double>(Denominator))
		* 100.0
		/ static_cast<double>(Denominator);

	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 0;
	Options.MaximumFractionalDigits = 2;
	return AddText(Name, FText::AsNumber(Percent, &Options));
}

void FPreviewTextArgumentBuilder::AddUnknown(FName Name, const FString& Error)
{
	AddError(Error);
	if (Name.IsNone())
	{
		return;
	}

	const FString Key = Name.ToString();
	ArgumentNames.Add(Name);
	Arguments.Add(Key, FFormatArgumentValue(FText::FromString(TEXT("?"))));
}

void FPreviewTextArgumentBuilder::AddError(const FString& Error)
{
	Errors.Add(Error);
}

bool FPreviewTextArgumentBuilder::Contains(FName Name) const
{
	return !Name.IsNone() && ArgumentNames.Contains(Name);
}

bool FPreviewTextArgumentBuilder::HasErrors() const
{
	return Errors.Num() > 0;
}

const TArray<FString>& FPreviewTextArgumentBuilder::GetErrors() const
{
	return Errors;
}

const FFormatNamedArguments& FPreviewTextArgumentBuilder::GetArguments() const
{
	return Arguments;
}

bool FPreviewTextArgumentBuilder::AddValue(FName Name, const FFormatArgumentValue& Value)
{
	if (Name.IsNone())
	{
		AddError(TEXT("A preview text argument has no name."));
		return false;
	}

	if (ArgumentNames.Contains(Name))
	{
		AddUnknown(
			Name,
			FString::Printf(TEXT("Duplicate preview text argument '%s'."), *Name.ToString())
		);
		return false;
	}

	ArgumentNames.Add(Name);
	Arguments.Add(Name.ToString(), Value);
	return true;
}
