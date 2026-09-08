#pragma once

#include "CoreMinimal.h"
#include "BattleAction.h"
#include "../Selection/SelectionTypes.h"
#include "SelectionRequestAction.generated.h"

class UAuthoredContinuation;
class USelectionResolver;
enum class ESelectionResolveDisposition : uint8;

// BattleAction that owns the suspend/resume lifecycle of one authored player
// choice.
//
// Execute begins the selection through the Gameplay-owned resolver and, when it
// succeeds, deliberately does NOT call Finish — the Queue's existing asynchronous
// Action model keeps this Action as CurrentAction while the resolution awaits
// external input. The authoritative submit path (Wave 1C-B) and Automation both
// call ResolvePendingSelection / CancelPendingSelection to return control.
//
// The Action is the queue-lifecycle owner: it enqueues any continuation batch at
// the Queue front and then Finishes, so the existing HandleActionFinished resume
// path advances the queue. It never pumps the queue itself.
UCLASS()
class SLAYTHESPIREDEMO_API USelectionRequestAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(
		USelectionResolver* InResolver,
		const FSelectionRequest& InRequest,
		UAuthoredContinuation* InContinuation
	);

	virtual void Execute(UBattleActionQueue* Queue) override;

	// Deterministic submit entry points. They revalidate through the resolver,
	// apply the resulting dependent batch at the Queue front, and Finish.
	void ResolvePendingSelection(const FSelectionResult& Result);
	ESelectionResolveDisposition ResolvePendingSelectionWithDisposition(const FSelectionResult& Result);
	void CancelPendingSelection();
	void AbandonPendingSelectionForFault();

	bool IsAwaitingSelection() const
	{
		return bAwaitingSelection;
	}

private:
	UBattleActionQueue* GetOwningQueue() const;

	UPROPERTY(Transient)
	TObjectPtr<USelectionResolver> Resolver = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAuthoredContinuation> Continuation = nullptr;

	UPROPERTY(Transient)
	FSelectionRequest Request;

	UPROPERTY(Transient)
	TObjectPtr<UBattleActionQueue> OwningQueue = nullptr;

	bool bAwaitingSelection = false;
};
