#pragma once

#include "CoreMinimal.h"
#include "CardEffect.h"
#include "SelectExhaustHandCardEffect.generated.h"

// Composable "select one Hand card, then exhaust it" Effect (Wave 1C-B).
//
// BuildActions computes the candidate set as "current Hand cards, excluding the
// played card (Context.Card)", then:
//   - non-empty candidates -> enqueue a USelectionRequestAction whose authored
//     UExhaustSelectedContinuation builds the UExhaustCardAction for the chosen card;
//   - empty candidates -> skip burning entirely (no selection, no exhaust, no fault).
//
// The Effect is the authored composition point: it knows both selection and
// exhaust, bridging them locally, while the primitive Actions stay neutral.
// Ordering after this Effect (e.g. draw) is authored by the owning card's
// Effects[] array, not decided here.
UCLASS(EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API USelectExhaustHandCardEffect : public UCardEffect
{
	GENERATED_BODY()

public:
	virtual void BuildActions(
		const FCardPlayContext& Context,
		TArray<UBattleAction*>& OutActions
	) const override;
	virtual void GetPreviewArgumentNames(TArray<FName>& OutNames) const override;
	virtual void BuildPreviewArguments(
		const FCardEffectPreviewContext& Context,
		FPreviewTextArgumentBuilder& OutArguments
	) const override;
	virtual void ValidatePreviewConfiguration(TArray<FText>& OutErrors) const override;
};
