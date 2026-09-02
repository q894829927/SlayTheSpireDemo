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

	// Card-facing Damage/Block values may carry the immutable authored base that
	// produced the current Gameplay-resolved value. RichText presentation uses
	// this metadata only for comparison styling; it never recomputes Gameplay.
	bool AddIntegerWithAuthoredBase(FName Name, int64 Value, int64 AuthoredBase);

	bool AddNumber(FName Name, double Value);
	bool AddText(FName Name, const FText& Value);
	bool AddPercentMagnitude(FName Name, int32 Numerator, int32 Denominator);

	// Replaces one already-declared semantic argument with a Gameplay-resolved A3
	// operation value. It deliberately cannot create a new argument, so target-
	// specific card-face formatting stays inside the validated authored template.
	// Existing authored-base metadata is preserved for RichText comparison.
	bool OverrideInteger(FName Name, int64 Value);

	void AddUnknown(FName Name, const FString& Error);
	void AddError(const FString& Error);

	bool Contains(FName Name) const;
	const FFormatArgumentValue* FindValue(FName Name) const;
	bool TryGetAuthoredBaseComparison(FName Name, int64& OutCurrentValue, int64& OutAuthoredBase) const;
	bool HasErrors() const;
	const TArray<FString>& GetErrors() const;

private:
	struct FIntegerPresentationMetadata
	{
		int64 CurrentValue = 0;
		int64 AuthoredBase = 0;
	};

	bool AddValue(FName Name, const FFormatArgumentValue& Value);

	// FName is the case-insensitive semantic identity of a gameplay value.
	// Never turn it directly into an FText::Format key: packaged builds do not
	// preserve FName casing, while named FText arguments are case-sensitive.
	TMap<FName, FFormatArgumentValue> ArgumentValues;
	TMap<FName, FIntegerPresentationMetadata> IntegerPresentationMetadata;
	TArray<FString> Errors;
};
