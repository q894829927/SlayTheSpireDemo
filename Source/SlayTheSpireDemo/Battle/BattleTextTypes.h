#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

class ACombatant;
class UCardInstance;

// Read-only context used by CardEffect definitions to expose the numeric facts
// needed by a player-facing description. Target is deliberately null for
// Enemy-target cards: the card face shows the source-side baseline and does not
// claim a target-specific result.
struct SLAYTHESPIREDEMO_API FCardEffectPreviewContext
{
	const UCardInstance* Card = nullptr;
	ACombatant* Source = nullptr;
	ACombatant* Target = nullptr;
};

// Collects named FText::Format arguments and records configuration failures.
// It never mutates gameplay and replaces invalid/ambiguous values with "?".
class SLAYTHESPIREDEMO_API FPreviewTextArgumentBuilder
{
public:
	bool AddInteger(FName Name, int64 Value);
	bool AddNumber(FName Name, double Value);
	bool AddText(FName Name, const FText& Value);
	bool AddPercentMagnitude(FName Name, int32 Numerator, int32 Denominator);
	void AddUnknown(FName Name, const FString& Error);
	void AddError(const FString& Error);

	bool Contains(FName Name) const;
	bool HasErrors() const;
	const TArray<FString>& GetErrors() const;
	const FFormatNamedArguments& GetArguments() const;

private:
	bool AddValue(FName Name, const FFormatArgumentValue& Value);

	FFormatNamedArguments Arguments;
	TSet<FName> ArgumentNames;
	TArray<FString> Errors;
};
