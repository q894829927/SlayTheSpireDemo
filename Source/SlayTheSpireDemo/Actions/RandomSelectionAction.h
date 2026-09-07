#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "../Selection/SelectionTypes.h"
#include "RandomSelectionAction.generated.h"

class UAuthoredContinuation;

// Domain-neutral random-index provider. The selection Action knows only the
// current remaining candidate count; the provider owns deterministic RNG state.
DECLARE_DELEGATE_RetVal_TwoParams(
	bool,
	FSelectionRandomIndexChooser,
	int32,
	int32&
);

// Synchronous producer of a resolved exact-N SelectionResult.
//
// Unlike USelectionRequestAction this Action never opens a pending player choice.
// It chooses unique candidate membership without replacement through the injected
// count->index provider, canonicalizes the chosen set back to original candidate
// order, then invokes the same authored Continuation contract used by Player mode.
// It deliberately knows nothing about cards, Hand, Exhaust or CardId.
UCLASS()
class SLAYTHESPIREDEMO_API URandomSelectionAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(
		const FSelectionRequest& InRequest,
		UAuthoredContinuation* InContinuation,
		const FSelectionRandomIndexChooser& InRandomIndexChooser
	);

	virtual void Execute(UBattleActionQueue* Queue) override;

private:
	UPROPERTY(Transient)
	FSelectionRequest Request;

	UPROPERTY(Transient)
	TObjectPtr<UAuthoredContinuation> Continuation = nullptr;

	FSelectionRandomIndexChooser RandomIndexChooser;
};
