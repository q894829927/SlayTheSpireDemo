#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StatusMutationTypes.h"
#include "StatusContainer.generated.h"

class ACombatant;
class UStatusData;
class UStatusInstance;

UCLASS()
class SLAYTHESPIREDEMO_API UStatusContainer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ACombatant* InOwner);

	FStatusMutationResult ApplyStatusCommit(
		UStatusData* Definition,
		int32 AmountToAdd,
		uint64 CandidateRuntimeSequence
	);

	FStatusMutationResult ReduceStatusCommit(UStatusInstance* ExpectedInstance, int32 AmountToRemove);
	FStatusMutationResult RemoveStatusCommit(UStatusInstance* ExpectedInstance);

	// Compatibility wrappers retained for pre-A2D callers/tests. New Gameplay
	// producers should consume the explicit mutation result APIs above.
	UStatusInstance* ApplyStatus(
		UStatusData* Definition,
		int32 AmountToAdd,
		uint64 CandidateRuntimeSequence,
		bool& bOutCreated
	);

	bool ReduceStatus(UStatusInstance* ExpectedInstance, int32 AmountToRemove);
	bool ContainsStatusInstance(const UStatusInstance* Instance) const;

	const UStatusInstance* FindStatusById(FName StatusId) const;
	bool RemoveStatusById(FName StatusId);

	const TArray<TObjectPtr<UStatusInstance>>& GetStatuses() const;
	FString DescribeStatuses() const;
	void LogState(const TCHAR* Context) const;

private:
	UStatusInstance* FindMutableStatusById(FName StatusId) const;
	int32 FindExactStatusIndex(const UStatusInstance* ExpectedInstance) const;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Owner = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStatusInstance>> Statuses;
};
