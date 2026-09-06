#pragma once

#include "CoreMinimal.h"
#include "Actions/BattleAction.h"
#include "Selection/AuthoredContinuation.h"
#include "Selection/SelectionTypes.h"
#include "CardExpansionWave1CTestTypes.generated.h"

// Concrete (non-abstract) neutral object used as a Selection candidate. Bare
// UObject is abstract and cannot be NewObject'd.
UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UWave1CTestCandidateObject : public UObject
{
	GENERATED_BODY()
};

// Stateless marker Action used to observe whether an authored Continuation's
// dependent batch actually ran. Carries no mutable result surface.
UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UWave1CTestMarkerAction : public UBattleAction
{
	GENERATED_BODY()

public:
	void Initialize(int32* InExecutionCounter, FName InTag);
	virtual void Execute(UBattleActionQueue* Queue) override;

	FName Tag = NAME_None;

private:
	int32* ExecutionCounter = nullptr;
};

// Concrete authored Continuation definition object for the primitive test. It is
// immutable/stateless: it holds only a non-owned counter pointer and a fixed
// marker tag, never resolution-local state.
UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UWave1CTestContinuation : public UAuthoredContinuation
{
	GENERATED_BODY()

public:
	void Configure(int32* InExecutionCounter, FName InTag, int32 InMarkersPerResult);
	virtual bool BuildNextActions(
		const FSelectionResult& Result,
		UBattleActionQueue* Queue,
		TArray<UBattleAction*>& OutActions
	) const override;

	FName Tag = NAME_None;
	int32 MarkersPerResult = 1;

private:
	int32* ExecutionCounter = nullptr;
};
