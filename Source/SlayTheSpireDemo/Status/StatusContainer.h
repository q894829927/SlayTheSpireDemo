#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
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

	UStatusInstance* ApplyStatus(
		UStatusData* Definition,
		int32 AmountToAdd,
		uint64 CandidateRuntimeSequence,
		bool& bOutCreated
	);

	const UStatusInstance* FindStatusById(FName StatusId) const;
	bool RemoveStatusById(FName StatusId);

	const TArray<TObjectPtr<UStatusInstance>>& GetStatuses() const;
	FString DescribeStatuses() const;
	void LogState(const TCHAR* Context) const;

private:
	UStatusInstance* FindMutableStatusById(FName StatusId) const;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Owner = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStatusInstance>> Statuses;
};
