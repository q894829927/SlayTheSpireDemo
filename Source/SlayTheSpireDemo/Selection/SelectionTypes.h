#pragma once

#include "CoreMinimal.h"
#include "SelectionTypes.generated.h"

// A single selectable runtime object. The Selection primitive is intentionally
// neutral: it does not know what a candidate is (a card, a combatant, ...) and
// must not interpret its concrete semantics. A candidate carries a stable
// deterministic ordering key (RuntimeSequence) and an optional presentation
// identity key; neither is authoritative Gameplay identity.
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FSelectionCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	TObjectPtr<UObject> RuntimeObject = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	int32 RuntimeSequence = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	FName SelectionKey = NAME_None;
};

UENUM(BlueprintType)
enum class ESelectionStatus : uint8
{
	Pending,
	Resolved,
	Cancelled,
	Invalid
};

// Cancellation is a consumer-authored rule, not a Presentation decision.
// Allowed preserves the generic primitive's legal cancel path. Forbidden is
// used by mandatory choices such as Burning Pact's "Exhaust 1 card" step.
UENUM(BlueprintType)
enum class ESelectionCancelPolicy : uint8
{
	Allowed,
	Forbidden
};

// G8-B exact identity for one authoritative player-facing pending Selection.
// SelectionBoundaryRevision is advanced before the pending request is exposed,
// so back-to-back requests with identical source/count/candidates still differ.
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FPendingSelectionRequestIdentity
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	int64 SelectionBoundaryRevision = 0;

	bool IsValid() const
	{
		return BattleId > 0 && SelectionBoundaryRevision > 0;
	}

	bool operator==(const FPendingSelectionRequestIdentity& Other) const
	{
		return BattleId == Other.BattleId
			&& SelectionBoundaryRevision == Other.SelectionBoundaryRevision;
	}

	bool operator!=(const FPendingSelectionRequestIdentity& Other) const
	{
		return !(*this == Other);
	}
};

// Immutable-by-contract player-choice result. SelectedObjects are the exact
// runtime objects the player chose; they are validated against the pending
// request before any continuation is built. A Cancelled or Invalid result must
// not mutate Gameplay and must not request a ResolutionFault.
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FSelectionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	ESelectionStatus Status = ESelectionStatus::Pending;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	TArray<TObjectPtr<UObject>> SelectedObjects;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	FName Reason = NAME_None;
};

// A Gameplay-authored selection request. It is created before Presentation
// interaction begins and owns only the candidate set plus the selection count
// constraints and cancellation policy. It holds no pending mutable state; that
// belongs to the resolver that owns the active selection.
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FSelectionRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	FName SelectionSource = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	int32 MinCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	int32 MaxCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	ESelectionCancelPolicy CancelPolicy = ESelectionCancelPolicy::Allowed;

	UPROPERTY(BlueprintReadOnly, Category = "Selection")
	TArray<FSelectionCandidate> Candidates;
};
