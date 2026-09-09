#pragma once

#include "CoreMinimal.h"
#include "SelectionPresentationMetadata.generated.h"

// Explicit committed Presentation grouping. Group identity is only a visual
// correlation aid; it never changes authoritative Gameplay ordering.
UENUM(BlueprintType)
enum class EPresentationGroupKind : uint8
{
	None UMETA(DisplayName = "None"),
	SelectionDestination UMETA(DisplayName = "Selection Destination")
};

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FPresentationGroupTag
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Group")
	EPresentationGroupKind Kind = EPresentationGroupKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Group")
	int64 GroupId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Group")
	int32 ExpectedMemberCount = 0;

	bool IsValid() const
	{
		return Kind != EPresentationGroupKind::None
			&& GroupId > 0
			&& ExpectedMemberCount >= 0;
	}
};

// Envelope-level declaration. Expected count alone is deliberately insufficient:
// the canonical selected identities are frozen so G2 can later reject omissions,
// substitutions and duplicates without consulting mutable Gameplay/UI state.
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FPresentationGroupDeclaration
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Group")
	FPresentationGroupTag Group;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Group")
	TArray<int32> CanonicalSelectedRuntimeIds;
};

UENUM(BlueprintType)
enum class ESelectionPresentationOutcomeMode : uint8
{
	None UMETA(DisplayName = "None"),
	RecordedResolution UMETA(DisplayName = "Recorded Resolution"),
	DirectStateRevision UMETA(DisplayName = "Direct State Revision")
};

// Immutable receipt that correlates one exact interactive Selection boundary to
// its accepted post-choice outcome. UI-local SelectionGeneration is intentionally
// absent: Gameplay/Presentation correlation is scoped only by Battle + boundary.
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FSelectionPresentationOutcomeReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Selection")
	int64 BattleId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Selection")
	int64 SelectionBoundaryRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Selection")
	ESelectionPresentationOutcomeMode Mode = ESelectionPresentationOutcomeMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Selection")
	int64 ResolutionId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Battle Presentation|Selection")
	int64 StateRevision = 0;

	bool IsValid() const
	{
		if (BattleId <= 0 || SelectionBoundaryRevision <= 0)
		{
			return false;
		}
		if (Mode == ESelectionPresentationOutcomeMode::RecordedResolution)
		{
			return ResolutionId > 0 && StateRevision == 0;
		}
		if (Mode == ESelectionPresentationOutcomeMode::DirectStateRevision)
		{
			return ResolutionId == 0 && StateRevision > SelectionBoundaryRevision;
		}
		return false;
	}
};

// Action-local, explicitly propagated context. Only USelectionRequestAction sets
// this on direct continuation Actions. Writer propagation alone never copies it,
// so trigger/reaction Actions remain ungrouped by construction.
struct SLAYTHESPIREDEMO_API FSelectionPresentationActionContext
{
	FPresentationGroupTag Group;
	TArray<int32> CanonicalSelectedRuntimeIds;

	bool IsValid() const
	{
		return Group.IsValid()
			&& Group.Kind == EPresentationGroupKind::SelectionDestination
			&& Group.ExpectedMemberCount == CanonicalSelectedRuntimeIds.Num();
	}
};
