#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StatusInstance.generated.h"

class ACombatant;
class UStatusContainer;
class UStatusData;

UCLASS()
class SLAYTHESPIREDEMO_API UStatusInstance : public UObject
{
	GENERATED_BODY()

public:
	UStatusData* GetDefinition() const;
	FName GetStatusId() const;
	int32 GetAmount() const;
	uint64 GetRuntimeSequence() const;
	ACombatant* GetOwner() const;
	FString GetDebugLabel() const;

private:
	friend class UStatusContainer;

	void Initialize(UStatusData* InDefinition, ACombatant* InOwner, int32 InAmount, uint64 InRuntimeSequence);
	bool AddAmount(int32 AmountToAdd);
	bool ReduceAmount(int32 AmountToRemove);

	UPROPERTY(Transient)
	TObjectPtr<UStatusData> Definition = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACombatant> Owner = nullptr;

	int32 Amount = 0;
	uint64 RuntimeSequence = 0;
};
