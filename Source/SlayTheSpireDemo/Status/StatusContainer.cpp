#include "StatusContainer.h"

#include "StatusData.h"
#include "StatusInstance.h"
#include "../Combat/Combatant.h"

void UStatusContainer::Initialize(ACombatant* InOwner)
{
	Owner = InOwner;
	Statuses.Reset();
}

UStatusInstance* UStatusContainer::ApplyStatus(
	UStatusData* Definition,
	int32 AmountToAdd,
	uint64 CandidateRuntimeSequence,
	bool& bOutCreated
)
{
	bOutCreated = false;

	if (!IsValid(Owner.Get()) || !IsValid(Definition) || Definition->StatusId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Status] ApplyStatus rejected: invalid Owner, Definition or StatusId."));
		return nullptr;
	}

	if (AmountToAdd <= 0 || CandidateRuntimeSequence == 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Status] ApplyStatus rejected: Status=%s AmountToAdd=%d CandidateSequence=%llu."),
			*Definition->StatusId.ToString(),
			AmountToAdd,
			static_cast<unsigned long long>(CandidateRuntimeSequence)
		);
		return nullptr;
	}

	if (UStatusInstance* Existing = FindMutableStatusById(Definition->StatusId))
	{
		if (Existing->GetDefinition() != Definition)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[Status] Duplicate StatusId '%s' uses different definitions; merging into the existing runtime status."),
				*Definition->StatusId.ToString()
			);
		}

		const int32 OldAmount = Existing->GetAmount();
		if (!Existing->AddAmount(AmountToAdd))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Status] Failed to merge %s."), *Existing->GetDebugLabel());
			return nullptr;
		}

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Status] Merged %s on %s: Amount %d + %d = %d. CandidateSequence=%llu intentionally unused."),
			*Existing->GetDebugLabel(),
			*GetNameSafe(Owner.Get()),
			OldAmount,
			AmountToAdd,
			Existing->GetAmount(),
			static_cast<unsigned long long>(CandidateRuntimeSequence)
		);
		LogState(TEXT("AfterMerge"));
		return Existing;
	}

	UStatusInstance* NewInstance = NewObject<UStatusInstance>(this);
	if (!IsValid(NewInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("[Status] Failed to create runtime status for %s."), *Definition->StatusId.ToString());
		return nullptr;
	}

	NewInstance->Initialize(Definition, Owner.Get(), AmountToAdd, CandidateRuntimeSequence);
	Statuses.Add(NewInstance);
	bOutCreated = true;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Status] Created %s on %s Amount=%d."),
		*NewInstance->GetDebugLabel(),
		*GetNameSafe(Owner.Get()),
		NewInstance->GetAmount()
	);
	LogState(TEXT("AfterCreate"));
	return NewInstance;
}

const UStatusInstance* UStatusContainer::FindStatusById(FName StatusId) const
{
	return FindMutableStatusById(StatusId);
}

UStatusInstance* UStatusContainer::FindMutableStatusById(FName StatusId) const
{
	if (StatusId.IsNone())
	{
		return nullptr;
	}

	for (const TObjectPtr<UStatusInstance>& InstancePtr : Statuses)
	{
		UStatusInstance* Instance = InstancePtr.Get();
		if (IsValid(Instance) && Instance->GetStatusId() == StatusId)
		{
			return Instance;
		}
	}

	return nullptr;
}

bool UStatusContainer::RemoveStatusById(FName StatusId)
{
	const int32 Index = Statuses.IndexOfByPredicate(
		[StatusId](const TObjectPtr<UStatusInstance>& InstancePtr)
		{
			return IsValid(InstancePtr.Get()) && InstancePtr->GetStatusId() == StatusId;
		}
	);

	if (Index == INDEX_NONE)
	{
		return false;
	}

	const FString RemovedLabel = Statuses[Index]->GetDebugLabel();
	Statuses.RemoveAt(Index);
	UE_LOG(LogTemp, Log, TEXT("[Status] Removed %s from %s."), *RemovedLabel, *GetNameSafe(Owner.Get()));
	LogState(TEXT("AfterRemove"));
	return true;
}

const TArray<TObjectPtr<UStatusInstance>>& UStatusContainer::GetStatuses() const
{
	return Statuses;
}

FString UStatusContainer::DescribeStatuses() const
{
	TArray<FString> Parts;
	Parts.Reserve(Statuses.Num());

	for (const TObjectPtr<UStatusInstance>& InstancePtr : Statuses)
	{
		const UStatusInstance* Instance = InstancePtr.Get();
		if (IsValid(Instance))
		{
			Parts.Add(FString::Printf(TEXT("%s Amount=%d"), *Instance->GetDebugLabel(), Instance->GetAmount()));
		}
	}

	return FString::Join(Parts, TEXT(", "));
}

void UStatusContainer::LogState(const TCHAR* Context) const
{
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Status] %s %s Statuses=[%s]"),
		*GetNameSafe(Owner.Get()),
		Context,
		*DescribeStatuses()
	);
}
