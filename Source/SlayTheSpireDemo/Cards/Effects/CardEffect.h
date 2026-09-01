#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../../Battle/BattleImmediatePreview.h"
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

	// A3 target-specific numeric preview contribution. Unsupported effects are
	// intentionally absent from Operations rather than fabricating an outcome.
	// This read-only hook must never call BuildActions or mutate Gameplay state.
	virtual void BuildImmediatePreviewOperations(
		const FCardEffectPreviewContext& Context,
		int32 EffectIndex,
		TArray<FImmediatePreviewOperation>& OutOperations
	) const
	{
	}
};
