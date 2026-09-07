#pragma once

#include "CoreMinimal.h"

class ABattleManager;

// UI-safe read view for the currently pending exact-N card selection. RuntimeIds
// are stable input identities only; authoritative candidate UObject pointers
// remain inside Gameplay's SelectionResolver.
struct SLAYTHESPIREDEMO_API FPendingCardSelectionReadView
{
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
	bool TryBuildPendingCardSelectionReadView(
		const ABattleManager* Battle,
		FPendingCardSelectionReadView& OutView
	);

	// Exact-N submit surface. RuntimeIds are validated for count, uniqueness and
	// candidate membership, then authoritative CardInstances are rebuilt in the
	// request's stable candidate order before the SelectionResult is submitted.
	bool SubmitPendingCardSelection(
		ABattleManager* Battle,
		const TArray<int32>& CardRuntimeIds
	);

	// Compatibility wrapper for the sealed single-card path.
	bool SubmitPendingCardSelection(
		ABattleManager* Battle,
		int32 CardRuntimeId
	);

	bool SubmitPendingSelectionCancel(ABattleManager* Battle);
}
