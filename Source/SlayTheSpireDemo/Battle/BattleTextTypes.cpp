#include "BattleTextTypes.h"

bool FPreviewTextArgumentBuilder::AddInteger(FName Name, int64 Value)
{
	const bool bAdded = AddValue(Name, FFormatArgumentValue(Value));
	if (bAdded)
	{
		IntegerPresentationMetadata.Remove(Name);
	}
	return bAdded;
}

bool FPreviewTextArgumentBuilder::AddIntegerWithAuthoredBase(FName Name, int64 Value, int64 AuthoredBase)
{
	if (!AddValue(Name, FFormatArgumentValue(Value)))
	{
		return false;
	}

	FIntegerPresentationMetadata& Metadata = IntegerPresentationMetadata.Add(Name);
	Metadata.CurrentValue = Value;
	Metadata.AuthoredBase = AuthoredBase;
	return true;
}

bool FPreviewTextArgumentBuilder::AddNumber(FName Name, double Value)
{
	const bool bAdded = AddValue(Name, FFormatArgumentValue(Value));
	if (bAdded)
	{
		IntegerPresentationMetadata.Remove(Name);
	}
	return bAdded;
}

bool FPreviewTextArgumentBuilder::AddText(FName Name, const FText& Value)
{
	const bool bAdded = AddValue(Name, FFormatArgumentValue(Value));
	if (bAdded)
	{
		IntegerPresentationMetadata.Remove(Name);
	}
	return bAdded;
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

bool FPreviewTextArgumentBuilder::OverrideInteger(FName Name, int64 Value)
{
	if (Name.IsNone())
	{
		AddError(TEXT("A target-specific preview override has no semantic name."));
		return false;
	}

	FFormatArgumentValue* Existing = ArgumentValues.Find(Name);
	if (Existing == nullptr)
	{
		AddError(FString::Printf(
			TEXT("Target-specific preview cannot override unknown argument '%s'."),
			*Name.ToString()));
		return false;
	}

	*Existing = FFormatArgumentValue(Value);
	if (FIntegerPresentationMetadata* Metadata = IntegerPresentationMetadata.Find(Name))
	{
		Metadata->CurrentValue = Value;
	}
	return true;
}

void FPreviewTextArgumentBuilder::AddUnknown(FName Name, const FString& Error)
{
	AddError(Error);
	if (Name.IsNone())
	{
		return;
	}

	IntegerPresentationMetadata.Remove(Name);
	ArgumentValues.Add(Name, FFormatArgumentValue(FText::FromString(TEXT("?"))));
}

void FPreviewTextArgumentBuilder::AddError(const FString& Error)
{
	Errors.Add(Error);
}

bool FPreviewTextArgumentBuilder::Contains(FName Name) const
{
	return !Name.IsNone() && ArgumentValues.Contains(Name);
}

const FFormatArgumentValue* FPreviewTextArgumentBuilder::FindValue(FName Name) const
{
	return Name.IsNone() ? nullptr : ArgumentValues.Find(Name);
}

bool FPreviewTextArgumentBuilder::TryGetAuthoredBaseComparison(
	FName Name,
	int64& OutCurrentValue,
	int64& OutAuthoredBase
) const
{
	OutCurrentValue = 0;
	OutAuthoredBase = 0;
	if (Name.IsNone())
	{
		return false;
	}

	const FIntegerPresentationMetadata* Metadata = IntegerPresentationMetadata.Find(Name);
	if (Metadata == nullptr)
	{
		return false;
	}

	OutCurrentValue = Metadata->CurrentValue;
	OutAuthoredBase = Metadata->AuthoredBase;
	return true;
}

bool FPreviewTextArgumentBuilder::HasErrors() const
{
	return Errors.Num() > 0;
}

const TArray<FString>& FPreviewTextArgumentBuilder::GetErrors() const
{
	return Errors;
}

bool FPreviewTextArgumentBuilder::AddValue(FName Name, const FFormatArgumentValue& Value)
{
	if (Name.IsNone())
	{
		AddError(TEXT("A preview text argument has no name."));
		return false;
	}

	if (ArgumentValues.Contains(Name))
	{
		AddUnknown(
			Name,
			FString::Printf(TEXT("Duplicate preview text argument '%s'."), *Name.ToString())
		);
		return false;
	}

	ArgumentValues.Add(Name, Value);
	return true;
}
