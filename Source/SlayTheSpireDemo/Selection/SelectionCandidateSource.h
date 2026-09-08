#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SelectionTypes.h"
#include "SelectionCandidateSource.generated.h"

class UDeckRuntime;

enum class ESelectionCandidateBuildStatus : uint8
{
	Success,
	NoCandidates,
	InvalidRuntimeDependency
};

// Runtime, single-selection dependency. Never stored on a shared Effect.
UCLASS(Abstract)
class SLAYTHESPIREDEMO_API USelectionCandidateSource : public UObject
{
	GENERATED_BODY()
public:
	virtual ESelectionCandidateBuildStatus BuildCandidates(TArray<FSelectionCandidate>& OutCandidates) const;
};

UCLASS()
class SLAYTHESPIREDEMO_API UCurrentHandSelectionSource : public USelectionCandidateSource
{
	GENERATED_BODY()
public:
	void Initialize(UDeckRuntime* InDeck);
	virtual ESelectionCandidateBuildStatus BuildCandidates(TArray<FSelectionCandidate>& OutCandidates) const override;
private:
	UPROPERTY(Transient)
	TObjectPtr<UDeckRuntime> Deck;
};
