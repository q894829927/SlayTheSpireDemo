#pragma once

#include "CoreMinimal.h"
#include "../Battle/BattleRequestTypes.h"
#include "../Presentation/PresentationG8Types.h"

class ABattleManager;

enum class EBufferedPlayerIntentKind : uint8
{
	None,
	CardSelection,
	EndTurn
};

enum class EBufferedIntentShadowEvaluation : uint8
{
	Stale,
	Waiting,
	Ready
};

struct SLAYTHESPIREDEMO_API FBufferedCardWindowIdentity
{
	int64 SourceResolutionId = 0;
	int64 SourcePresentationSequence = 0;
	uint64 LocalWindowGeneration = 0;

	bool IsValid() const
	{
		return SourceResolutionId > 0
			&& SourcePresentationSequence > 0
			&& LocalWindowGeneration > 0;
	}

	friend bool operator==(
		const FBufferedCardWindowIdentity& A,
		const FBufferedCardWindowIdentity& B)
	{
		return A.SourceResolutionId == B.SourceResolutionId
			&& A.SourcePresentationSequence == B.SourcePresentationSequence
			&& A.LocalWindowGeneration == B.LocalWindowGeneration;
	}
};

// Presentation-lag identity only. ExpectedReadyRevision must already be a
// committed/sealed target; this type never grants permission to predict a future
// Gameplay revision.
struct SLAYTHESPIREDEMO_API FBufferedCardIntent
{
	FPresentationSessionToken SessionToken;
	int64 BattleId = 0;
	int64 ExpectedReadyRevision = 0;
	FBufferedCardWindowIdentity CaptureWindow;
	int32 RuntimeId = INDEX_NONE;

	bool IsValid() const
	{
		return SessionToken.IsValid()
			&& BattleId > 0
			&& ExpectedReadyRevision > 0
			&& CaptureWindow.IsValid()
			&& RuntimeId != INDEX_NONE;
	}
};

// Gameplay-turn identity only. CaptureStateRevision is diagnostic provenance;
// it is never a range/replay credential.
struct SLAYTHESPIREDEMO_API FBufferedEndTurnIntent
{
	FPlayerTurnAuthorityToken Turn;
	int64 CaptureStateRevision = 0;
	uint64 LocalIntentGeneration = 0;

	bool IsValid() const
	{
		return Turn.IsValid() && LocalIntentGeneration > 0;
	}
};

// Single pending-intent owner. G9-A exercised this type in shadow mode; G9-B
// activates the same arbitration contract for production Native HUD input.
class SLAYTHESPIREDEMO_API FBufferedPlayerIntentShadowState
{
public:
	EBufferedPlayerIntentKind GetKind() const { return Kind; }
	bool HasCardSelection() const { return Kind == EBufferedPlayerIntentKind::CardSelection; }
	bool HasEndTurn() const { return Kind == EBufferedPlayerIntentKind::EndTurn; }
	bool IsEmpty() const { return Kind == EBufferedPlayerIntentKind::None; }
	const FBufferedCardIntent* GetCardSelection() const
	{
		return HasCardSelection() ? &CardIntent : nullptr;
	}
	const FBufferedEndTurnIntent* GetEndTurn() const
	{
		return HasEndTurn() ? &EndTurnIntent : nullptr;
	}

	bool StoreCardSelection(const FBufferedCardIntent& Intent);
	bool StoreEndTurn(FBufferedEndTurnIntent Intent);
	bool TryTakeCardSelection(FBufferedCardIntent& OutIntent);
	bool TryTakeEndTurn(FBufferedEndTurnIntent& OutIntent);
	void Clear();

private:
	EBufferedPlayerIntentKind Kind = EBufferedPlayerIntentKind::None;
	FBufferedCardIntent CardIntent;
	FBufferedEndTurnIntent EndTurnIntent;
	uint64 NextIntentGeneration = 1;
};

// Exact EndTurn authority helpers. They do not themselves call
// RequestEndPlayerTurn(); G9-B production code consumes them through the single
// buffered-player-intent owner.
SLAYTHESPIREDEMO_API bool TryCaptureBufferedEndTurnShadow(
	const ABattleManager* Battle,
	FBufferedEndTurnIntent& OutIntent
);

SLAYTHESPIREDEMO_API EBufferedIntentShadowEvaluation EvaluateBufferedEndTurnShadow(
	const ABattleManager* Battle,
	const FBufferedEndTurnIntent& Intent
);
