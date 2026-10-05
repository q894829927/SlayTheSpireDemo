#pragma once

#include "CoreMinimal.h"
#include "PresentationG8Types.h"
#include "../Battle/PlayerTurnAuthority.h"
#include "../Cards/CardTypes.h"

class UCardInstance;

struct SLAYTHESPIREDEMO_API FBufferedCardWindowIdentity
{
	int64 SourceResolutionId = 0;
	int64 SourcePresentationSequence = 0;
	int64 LocalWindowGeneration = 0;
	uint64 ChronologyGeneration = 0;

	bool IsValid() const
	{
		return SourceResolutionId > 0 && SourcePresentationSequence > 0
			&& LocalWindowGeneration > 0 && ChronologyGeneration != 0;
	}
};

struct SLAYTHESPIREDEMO_API FBufferedCardIntent
{
	FPresentationSessionToken SessionToken;
	int64 BattleId = 0;
	int64 ExpectedReadyRevision = 0;
	FBufferedCardWindowIdentity CaptureWindow;
	int32 RuntimeId = INDEX_NONE;
	// Input observation only. Never used to reconstruct historical display.
	TWeakObjectPtr<UCardInstance> Card;
};

struct SLAYTHESPIREDEMO_API FBufferedEndTurnIntent
{
	FPlayerTurnAuthorityToken Turn;
	int64 CaptureStateRevision = 0; // provenance, never a replay credential
	uint64 LocalIntentGeneration = 0;
	TOptional<FPresentationSessionToken> PresentationFence;
};

// A confirmed command credential, not Gameplay or an energy reservation.
// CaptureStateRevision describes its source; ordinary historical advancement
// does not invalidate it. Runtime bindings are resolved only at submission.
struct SLAYTHESPIREDEMO_API FQueuedCardPlayIntent
{
	FPlayerTurnAuthorityToken Turn;
	TOptional<FPresentationSessionToken> PresentationFence;
	uint64 BindingGeneration = 0;
	uint64 InputSequence = 0;
	int64 CaptureStateRevision = 0;
	int32 RuntimeId = INDEX_NONE;
	FName CardId;
	FName TargetPresentationId;
	ECardTargetType TargetType = ECardTargetType::None;
};
