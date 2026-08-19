#include "StatusInstance.h"

#include "StatusData.h"

void UStatusInstance::Initialize(UStatusData* InDefinition, ACombatant* InOwner, int32 InAmount, uint64 InRuntimeSequence)
{
	Definition = InDefinition;
	Owner = InOwner;
	Amount = InAmount;
	RuntimeSequence = InRuntimeSequence;
}

bool UStatusInstance::AddAmount(int32 AmountToAdd)
{
	if (AmountToAdd <= 0 || Amount <= 0)
	{
		return false;
	}

	const int64 NewAmount = static_cast<int64>(Amount) + static_cast<int64>(AmountToAdd);
	Amount = static_cast<int32>(FMath::Clamp<int64>(NewAmount, 1, MAX_int32));
	return true;
}

bool UStatusInstance::ReduceAmount(int32 AmountToRemove)
{
	if (AmountToRemove <= 0 || Amount <= AmountToRemove)
	{
		return false;
	}

	Amount -= AmountToRemove;
	return true;
}

UStatusData* UStatusInstance::GetDefinition() const
{
	return Definition.Get();
}

FName UStatusInstance::GetStatusId() const
{
	return IsValid(Definition.Get()) ? Definition->StatusId : NAME_None;
}

int32 UStatusInstance::GetAmount() const
{
	return Amount;
}

uint64 UStatusInstance::GetRuntimeSequence() const
{
	return RuntimeSequence;
}

ACombatant* UStatusInstance::GetOwner() const
{
	return Owner.Get();
}

FString UStatusInstance::GetDebugLabel() const
{
	const FName StatusId = GetStatusId();
	const FString StableId = StatusId.IsNone() ? TEXT("UnknownStatus") : StatusId.ToString();
	return FString::Printf(TEXT("%s#%llu"), *StableId, static_cast<unsigned long long>(RuntimeSequence));
}
