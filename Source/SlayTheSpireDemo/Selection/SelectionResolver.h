#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SelectionTypes.h"
#include "SelectionResolver.generated.h"

class UAuthoredContinuation;
class UBattleAction;
class UBattleActionQueue;

DECLARE_DELEGATE_RetVal_OneParam(UBattleActionQueue*, FSelectionResolverQueueAccess, const USelectionResolver*);

// Gameplay-authoritative owner of the single active pending selection.
//
// Selection state belongs to Gameplay. Presentation only submits a
// FSelectionResult; it never stores pending selection, decides valid
// candidates, mutates cards/zones, or executes a continuation directly.
//
// The resolver is deliberately narrow: one active request at a time, no
// persistent registry, no universal result bus. Continuation execution is the
// queue's job; the resolver only validates and returns the batch to enqueue.
UCLASS()
class SLAYTHESPIREDEMO_API USelectionResolver : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(FSelectionResolverQueueAccess InQueueAccess);

	bool HasPendingSelection() const;
	const FSelectionRequest* GetPendingRequest() const;

	// Begins a selection. Fails if a selection is already pending or the request
	// is malformed (empty candidate set, inverted min/max bounds).
	bool BeginSelection(
		const FSelectionRequest& Request,
		const UAuthoredContinuation* Continuation
	);

	// Validates a submitted result against the pending request. On success the
	// requested continuation builds the dependent batch into OutActions and the
	// pending selection is cleared. Returns false for cancelled/invalid results
	// (pending selection is cleared but no Actions are produced).
	bool TryResolveSelection(
		const FSelectionResult& Result,
		TArray<UBattleAction*>& OutActions
	);

	// Cancels the pending selection without mutation or fault.
	void CancelSelection();

private:
	bool ValidateRequest(const FSelectionRequest& Request, FString& OutReason) const;
	bool ValidateResult(const FSelectionResult& Result, FString& OutReason) const;

	UPROPERTY(Transient)
	FSelectionRequest PendingRequest;

	UPROPERTY(Transient)
	TObjectPtr<const UAuthoredContinuation> PendingContinuation = nullptr;

	bool bHasPendingSelection = false;
	FSelectionResolverQueueAccess QueueAccess;
};
