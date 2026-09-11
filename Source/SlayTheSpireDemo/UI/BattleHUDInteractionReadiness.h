#pragma once

#include "CoreMinimal.h"
#include "../Presentation/PresentationG8Types.h"
#include "../Selection/SelectionTypes.h"

enum class EBattleHUDReadinessAuthoritySource : uint8
{
	Invalid,
	PresentationOwned,
	DirectBaseline
};

enum class EBattleHUDReadinessMode : uint8
{
	NormalPlayerTurn,
	CardReadyToConfirm,
	CardTargetChoice,
	PendingCardSelection,
	Resolving,
	TerminalOrUnavailable
};

// G8-B is deliberately shadow-only. This value describes what the exact
// authority/readiness evaluator believes without mutating ViewModel input state.
struct SLAYTHESPIREDEMO_API FBattleHUDInteractionReadinessShadow
{
	EBattleHUDReadinessAuthoritySource AuthoritySource =
		EBattleHUDReadinessAuthoritySource::Invalid;
	EBattleHUDReadinessMode Mode = EBattleHUDReadinessMode::Resolving;

	FPresentationSessionToken SessionToken;
	FPendingSelectionRequestIdentity PendingSelectionIdentity;
	int64 BattleId = 0;
	int64 StateRevision = 0;
	int32 SelectedCardRuntimeId = INDEX_NONE;

	bool bAuthoritativePendingSelection = false;
	bool bVisiblePendingSelection = false;
	bool bExactReadSurface = false;
	bool bReady = false;
	bool bBaselineReady = false;
	bool bMatchesBaseline = false;
};
