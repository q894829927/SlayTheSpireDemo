#pragma once

#include "PresentationTypes.h"
#include "PresentationG8Types.h"
#include "PresentationCardReducer.generated.h"

// One historical play occurrence. An absent session is permitted for reduction
// without a Widget; such a token can never authorize a visual job.
USTRUCT()
struct SLAYTHESPIREDEMO_API FPlayedCardPresentationLifecycleToken
{
	GENERATED_BODY()
	UPROPERTY() FPresentationSessionToken SessionToken;
	UPROPERTY() int64 BattleId = 0;
	UPROPERTY() int64 SourceResolutionId = 0;
	UPROPERTY() int64 CardPlayedPresentationSequence = 0;
	UPROPERTY() int64 LocalLifecycleGeneration = 0;
	UPROPERTY() int32 RuntimeId = INDEX_NONE;
	UPROPERTY() FName CardId;
	bool HasHistoricalIdentity() const;
	bool IsValid() const { return HasHistoricalIdentity() && SessionToken.IsValid() && SessionToken.BattleId == BattleId; }
	bool operator==(const FPlayedCardPresentationLifecycleToken& Other) const;
	bool operator!=(const FPlayedCardPresentationLifecycleToken& Other) const { return !(*this == Other); }
};

USTRUCT()
struct SLAYTHESPIREDEMO_API FPlayedCardHistoryEntry
{
	GENERATED_BODY()
	UPROPERTY() FPlayedCardPresentationLifecycleToken Token;
	UPROPERTY() FPresentationCardSnapshot Card;
};

USTRUCT()
struct SLAYTHESPIREDEMO_API FCardPresentationHistoryState
{
	GENERATED_BODY()
	UPROPERTY() TArray<FPlayedCardHistoryEntry> PendingPlays;
	UPROPERTY() int64 NextLifecycleGeneration = 1;
	// Skip/recovery clears correlations without reusing an occurrence generation.
	void ClearCorrelations() { PendingPlays.Reset(); }
};

namespace PresentationCardReducer
{
	SLAYTHESPIREDEMO_API bool IsCardSnapshotValid(const FPresentationCardSnapshot& Card);
	// RichDescription is a transient frozen preview; it is not card identity.
	SLAYTHESPIREDEMO_API bool DoesCardViewMatch(const FBattleHUDCardView& View, const FPresentationCardSnapshot& Card);
	// Atomic on failure; no Gameplay, UI or Controller publication. Passing copies
	// gives preflight the identical production semantics without formal effects.
	SLAYTHESPIREDEMO_API bool TryApplyRecord(FPresentationStateSnapshot& Snapshot,
		FCardPresentationHistoryState& History, const FPresentationRecord& Record,
		const FPresentationSessionToken& Session, FPlayedCardPresentationLifecycleToken* OutLifecycle = nullptr);
}
