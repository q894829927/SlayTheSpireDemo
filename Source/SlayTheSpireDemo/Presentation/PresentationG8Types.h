#pragma once

#include "CoreMinimal.h"
#include "PresentationG8Types.generated.h"

// G8 Presentation-owned authority identity. This is deliberately distinct from
// per-playback FPresentationPlaybackToken: a session survives ordinary
// Skip/reconcile but becomes stale when the Controller/binding authority changes.
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FPresentationSessionToken
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	int64 ControllerEpoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	int64 PresentationSessionGeneration = 0;

	bool IsValid() const
	{
		return BattleId > 0
			&& ControllerEpoch > 0
			&& PresentationSessionGeneration > 0;
	}

	bool operator==(const FPresentationSessionToken& Other) const
	{
		return BattleId == Other.BattleId
			&& ControllerEpoch == Other.ControllerEpoch
			&& PresentationSessionGeneration == Other.PresentationSessionGeneration;
	}

	bool operator!=(const FPresentationSessionToken& Other) const
	{
		return !(*this == Other);
	}
};

// Exact identity for one detached DamageNumber. The local generation makes a
// re-prepared visual for the same committed Record a different cosmetic object.
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FDetachedDamageToken
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	FPresentationSessionToken SessionToken;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	int64 SourceResolutionId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	int64 PresentationSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	int64 LocalDamageVisualGeneration = 0;

	bool IsValid() const
	{
		return SessionToken.IsValid()
			&& SourceResolutionId > 0
			&& PresentationSequence > 0
			&& LocalDamageVisualGeneration > 0;
	}

	bool operator==(const FDetachedDamageToken& Other) const
	{
		return SessionToken == Other.SessionToken
			&& SourceResolutionId == Other.SourceResolutionId
			&& PresentationSequence == Other.PresentationSequence
			&& LocalDamageVisualGeneration == Other.LocalDamageVisualGeneration;
	}

	bool operator!=(const FDetachedDamageToken& Other) const
	{
		return !(*this == Other);
	}
};

// Frozen presentation-only inputs for one DamageNumber. Formal HP/Block state is
// intentionally absent: the Controller reducer owns that state transition.
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FDetachedDamageVisualSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	int32 IncomingDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	FName TargetPresentationId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	FVector2D FrozenHostLocalStartPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|G8")
	float VisualDuration = 0.0f;

	bool IsValid() const
	{
		return IncomingDamage > 0
			&& !TargetPresentationId.IsNone()
			&& FMath::IsFinite(FrozenHostLocalStartPosition.X)
			&& FMath::IsFinite(FrozenHostLocalStartPosition.Y)
			&& FMath::IsFinite(VisualDuration)
			&& VisualDuration > 0.0f;
	}
};
