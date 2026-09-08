#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "UObject/Object.h"
#include "BattleActionQueue.generated.h"

class UBattleAction;
struct FPresentationRecordWriter;

DECLARE_MULTICAST_DELEGATE(FOnBattleActionQueueEmpty);
DECLARE_MULTICAST_DELEGATE(FOnBattleActionQueueResolutionIdle);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnBattleActionQueueResolutionFaulted,
	const FString&,
	int32,
	UBattleAction*
);

UCLASS()
class SLAYTHESPIREDEMO_API UBattleActionQueue : public UObject
{
	GENERATED_BODY()

public:
	bool AddToBack(UBattleAction* Action);
	bool AddToFront(UBattleAction* Action);

	// On a healthy Queue, an empty batch is always a legal no-op success,
	// including during QueueEmpty observer notification. Non-empty batches remain
	// subject to the normal atomic validation and QueueEmpty non-reentrancy guard.
	bool AddBatchToBackPreserveOrder(const TArray<UBattleAction*>& Actions);
	bool AddBatchToFrontPreserveOrder(const TArray<UBattleAction*>& Actions);
	bool StartProcessing();

	// On an explicitly validated synchronous interactive boundary, a read-only
	// snapshot operation may observe the already-committed Gameplay state without
	// treating the still-current boundary Action or its pending tail as normal
	// command-busy state. The scope never pumps, mutates or advances the Queue.
	bool RunReadSnapshotAtCurrentActionBoundary(
		const UBattleAction* BoundaryAction,
		TFunctionRef<bool()> ReadOperation
	);
	bool IsCurrentAction(const UBattleAction* Action) const;

	// After a Presentation segment is sealed at the current interactive boundary,
	// already-authored tail Actions must inherit the next segment writer before
	// the current Action finishes. Validation is atomic before any writer changes.
	bool RebindPendingPresentationRecordWriter(
		const UBattleAction* BoundaryAction,
		const FPresentationRecordWriter& Writer
	);

	// QueueEmpty observers may inspect the completed boundary, but authoritative
	// macro progression must not synchronously start a new resolution from inside
	// the multicast broadcast. The one authoritative continuation registered here
	// runs only after every QueueEmpty listener has returned. If it enqueues and
	// starts another batch, the existing PumpQueue call continues that work without
	// nesting another QueueEmpty broadcast inside the previous one.
	// Registration is rejected when the Queue is faulted/fault-requested, when no
	// QueueEmpty broadcast is active, when the callable is unbound, or when another
	// continuation is already registered for the same boundary.
	bool DeferUntilAfterQueueEmptyBroadcast(TFunction<void()>&& Continuation);

	bool RequestResolutionFault(const FString& Reason);
	bool IsResolutionFaulted() const;
	bool IsBusy() const;
	int32 GetPendingCount() const;
	int32 GetExecutedCountInResolution() const;
	const FString& GetResolutionFaultReason() const;
	UBattleAction* GetLastExecutedAction() const;

#if WITH_DEV_AUTOMATION_TESTS
	void SetMaxActionsPerResolutionForTesting(int32 InMaxActions);
#endif

	FOnBattleActionQueueEmpty OnQueueEmpty;

	// Internal resolution-settled signal. This is deliberately later than
	// OnQueueEmpty: it is emitted only after the complete PumpQueue frame has
	// exited and no deferred authoritative continuation remains. The Queue does
	// not discover or call a concrete owner through its UObject Outer. Its owner
	// may subscribe explicitly, while Widgets must not treat this Queue-level
	// signal as the public battle read-state boundary.
	FOnBattleActionQueueResolutionIdle OnResolutionIdle;
	FOnBattleActionQueueResolutionFaulted OnResolutionFaulted;

private:
	static constexpr int32 DefaultMaxActionsPerResolution = 10000;

	bool ValidateBatchForInsertion(const TArray<UBattleAction*>& Actions, FString& OutReason) const;
	void PumpQueue();
	void HandleActionFinished(UBattleAction* FinishedAction);
	void EnterResolutionFaultAtSafePoint();
	void BroadcastResolutionIdleIfSettled();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBattleAction>> PendingActions;

	UPROPERTY(Transient)
	TObjectPtr<UBattleAction> CurrentAction = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBattleAction> LastExecutedAction = nullptr;

	bool bIsPumping = false;
	bool bIsBroadcastingQueueEmpty = false;
	bool bIsExecutingPostQueueEmptyContinuation = false;
	bool bHasDeferredQueueEmptyContinuation = false;
	bool bResolutionFaultRequested = false;
	bool bResolutionFaulted = false;
	bool bInteractiveSnapshotReadScope = false;
	int32 ExecutedCountInResolution = 0;
	int32 MaxActionsPerResolution = DefaultMaxActionsPerResolution;
	FString ResolutionFaultReason;
	TFunction<void()> DeferredQueueEmptyContinuation;
};
