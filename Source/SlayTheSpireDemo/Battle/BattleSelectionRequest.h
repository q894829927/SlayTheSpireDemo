#pragma once

#include "CoreMinimal.h"
#include "../Selection/SelectionTypes.h"

class ABattleManager;

// UI-safe read view for the currently pending exact-N card selection. RuntimeIds
// are stable input identities only; authoritative candidate UObject pointers
// remain inside Gameplay's SelectionResolver.
struct SLAYTHESPIREDEMO_API FPendingCardSelectionReadView
{
	FPendingSelectionRequestIdentity RequestIdentity;
	FName SelectionSource = NAME_None;
	TArray<int32> CandidateRuntimeIds;
	int32 RequiredCount = 0;
	bool bCanCancel = false;
};

// Formal Gameplay boundary used by the Native HUD/ViewModel while an existing
// BattleAction resolution is intentionally waiting for player card selection.
// The UI never receives candidate UObject pointers and never touches the queue.
namespace BattleSelectionRequest
{
	SLAYTHESPIREDEMO_API bool TryBuildPendingCardSelectionReadView(
		const ABattleManager* Battle,
		FPendingCardSelectionReadView& OutView
	);

	// Exact-N submit surface. RuntimeIds are validated for count, uniqueness and
	// candidate membership, then authoritative CardInstances are rebuilt in the
	// request's stable candidate order before the SelectionResult is submitted.
	// The expected exact request identity is matched before any candidate data is
	// interpreted so a stale UI callback cannot resolve a later identical request.
	SLAYTHESPIREDEMO_API bool SubmitPendingCardSelection(
		ABattleManager* Battle,
		const FPendingSelectionRequestIdentity& ExpectedIdentity,
		const TArray<int32>& CardRuntimeIds
	);

	SLAYTHESPIREDEMO_API bool SubmitPendingSelectionCancel(
		ABattleManager* Battle,
		const FPendingSelectionRequestIdentity& ExpectedIdentity
	);

#if WITH_DEV_AUTOMATION_TESTS
	// Compatibility helpers for older generic Selection tests. Production UI code
	// must always carry the exact read-view identity explicitly.
	SLAYTHESPIREDEMO_API bool SubmitPendingCardSelection(
		ABattleManager* Battle,
		const TArray<int32>& CardRuntimeIds
	);
	SLAYTHESPIREDEMO_API bool SubmitPendingSelectionCancel(ABattleManager* Battle);
#endif
}
