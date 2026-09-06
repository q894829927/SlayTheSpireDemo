#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SelectionTypes.h"
#include "AuthoredContinuation.generated.h"

class UBattleAction;
class UBattleActionQueue;

// Locked resolution-local authored Continuation contract (Ironclad plan 2.6).
//
// A Continuation owns only the responsibility: "given the preceding typed
// Result, build the downstream Action batch for the current resolution." It is
// an immutable / stateless definition object. It must never:
//   - store LastResult / PendingCard / CurrentTarget / any resolution-local
//     mutable state;
//   - mutate Gameplay or drive the queue directly;
//   - be a BattleEvent, Dispatcher listener, persistent Registry or value bag.
//
// Dynamic state lives only in the executing Action, the typed Result and the
// local invocation data. The queue insertion ordering (reactions before
// continuation) is the caller's responsibility, not the Continuation's.
UCLASS(Abstract, Blueprintable)
class SLAYTHESPIREDEMO_API UAuthoredContinuation : public UObject
{
	GENERATED_BODY()

public:
	// Builds the dependent Action batch for a resolved selection. Returns false
	// when the Result cannot satisfy this continuation (e.g. cancelled/invalid
	// selection); the caller then clears pending state without enqueueing.
	// The queue is provided read-only for allocation (NewObject outer) only.
	virtual bool BuildNextActions(
		const FSelectionResult& Result,
		UBattleActionQueue* Queue,
		TArray<UBattleAction*>& OutActions
	) const
	{
		(void)Result;
		(void)Queue;
		(void)OutActions;
		return false;
	}
};
