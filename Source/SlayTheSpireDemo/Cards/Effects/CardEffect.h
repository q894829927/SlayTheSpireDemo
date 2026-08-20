#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CardEffect.generated.h"

class UBattleAction;
struct FCardPlayContext;
struct FCardEffectPreviewContext;
class FPreviewTextArgumentBuilder;

UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UCardEffect : public UObject
{
	GENERATED_BODY()

public:
	virtual void BuildActions(
		const FCardPlayContext& Context,
		TArray<UBattleAction*>& OutActions
	) const PURE_VIRTUAL(UCardEffect::BuildActions, );

	// Read-only player-facing facts. Every concrete Effect must explicitly expose
	// its deterministic numeric values so future Effects cannot silently fall back
	// to stale hand-authored numbers.
	virtual void GetPreviewArgumentNames(TArray<FName>& OutNames) const
		PURE_VIRTUAL(UCardEffect::GetPreviewArgumentNames, );
	virtual void BuildPreviewArguments(
		const FCardEffectPreviewContext& Context,
		FPreviewTextArgumentBuilder& OutArguments
	) const PURE_VIRTUAL(UCardEffect::BuildPreviewArguments, );
	virtual void ValidatePreviewConfiguration(TArray<FText>& OutErrors) const
		PURE_VIRTUAL(UCardEffect::ValidatePreviewConfiguration, );
};
