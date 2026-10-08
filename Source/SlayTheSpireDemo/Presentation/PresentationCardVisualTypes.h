#pragma once
#include "PresentationCardReducer.h"
#include "PresentationCardVisualTypes.generated.h"

USTRUCT()
struct SLAYTHESPIREDEMO_API FDetachedCardVisualToken
{
	GENERATED_BODY()
	UPROPERTY() FPlayedCardPresentationLifecycleToken Lifecycle;
	UPROPERTY() int64 LocalVisualGeneration = 0;
	bool IsValid() const { return Lifecycle.IsValid() && LocalVisualGeneration > 0; }
	bool operator==(const FDetachedCardVisualToken& Other) const
	{ return Lifecycle == Other.Lifecycle && LocalVisualGeneration == Other.LocalVisualGeneration; }
};

// One preparation receipt, not a Blocking playback credential or history fact.
USTRUCT()
struct SLAYTHESPIREDEMO_API FDetachedCardDestinationToken
{
	GENERATED_BODY()
	UPROPERTY() FDetachedCardVisualToken Visual;
	UPROPERTY() int64 ResolutionId = 0;
	UPROPERTY() int64 PresentationSequence = 0;
	UPROPERTY() int64 PreparationGeneration = 0;
	bool IsValid() const
	{ return Visual.IsValid() && ResolutionId > 0 && PresentationSequence > 0 && PreparationGeneration > 0; }
	bool operator==(const FDetachedCardDestinationToken& Other) const
	{ return Visual == Other.Visual && ResolutionId == Other.ResolutionId
		&& PresentationSequence == Other.PresentationSequence && PreparationGeneration == Other.PreparationGeneration; }
};
