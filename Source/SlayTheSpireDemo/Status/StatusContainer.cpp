#include "StatusContainer.h"

#include "StatusData.h"
#include "StatusInstance.h"
#include "../Combat/Combatant.h"

void UStatusContainer::Initialize(ACombatant* InOwner)
{
	Owner = InOwner;
	Statuses.Reset();
}

FStatusMutationResult UStatusContainer::ApplyStatusCommit(
	UStatusData* Definition,
	int32 AmountToAdd,
	uint64 CandidateRuntimeSequence
)
{
	FStatusMutationResult Result;

	if (!IsValid(Owner.Get()) || !IsValid(Definition) || Definition->StatusId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Status] ApplyStatus rejected: invalid Owner, Definition or StatusId."));
		return Result;
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
		return Result;
	}

	if (UStatusInstance* Existing = FindMutableStatusById(Definition->StatusId))
	{
		UStatusData* EffectiveDefinition = Existing->GetDefinition();
		if (!IsValid(EffectiveDefinition)
			|| Existing->GetOwner() != Owner.Get()
			|| Existing->GetStatusId().IsNone()
			|| Existing->GetRuntimeSequence() == 0
			|| Existing->GetAmount() <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Status] ApplyStatus rejected: existing runtime status is structurally invalid."));
			return Result;
		}

		if (EffectiveDefinition != Definition)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[Status] Duplicate StatusId '%s' uses different definitions; merging into the existing runtime status."),
				*Definition->StatusId.ToString()
			);
		}

		Result.StatusId = Existing->GetStatusId();
		Result.RuntimeSequence = Existing->GetRuntimeSequence();
		Result.AmountBefore = Existing->GetAmount();
		Result.AmountAfter = Result.AmountBefore;
		Result.EffectiveInstance = Existing;
		Result.EffectiveDefinition = EffectiveDefinition;

		if (!Existing->AddAmount(AmountToAdd))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Status] Failed to merge %s."), *Existing->GetDebugLabel());
			return Result;
		}

		Result.AmountAfter = Existing->GetAmount();
		if (Result.AmountAfter == Result.AmountBefore)
		{
			Result.Outcome = EStatusMutationOutcome::NoOp;
			UE_LOG(
				LogTemp,
				Log,
				TEXT("[Status] Merge no-op for %s on %s: Amount remains %d. CandidateSequence=%llu intentionally unused."),
				*Existing->GetDebugLabel(),
				*GetNameSafe(Owner.Get()),
				Result.AmountAfter,
				static_cast<unsigned long long>(CandidateRuntimeSequence)
			);
			return Result;
		}

		Result.Outcome = EStatusMutationOutcome::Committed;
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Status] Merged %s on %s: Amount %d + %d = %d. CandidateSequence=%llu intentionally unused."),
			*Existing->GetDebugLabel(),
			*GetNameSafe(Owner.Get()),
			Result.AmountBefore,
			AmountToAdd,
			Result.AmountAfter,
			static_cast<unsigned long long>(CandidateRuntimeSequence)
		);
		LogState(TEXT("AfterMerge"));
		return Result;
	}

	UStatusInstance* NewInstance = NewObject<UStatusInstance>(this);
	if (!IsValid(NewInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("[Status] Failed to create runtime status for %s."), *Definition->StatusId.ToString());
		return Result;
	}

	NewInstance->Initialize(Definition, Owner.Get(), AmountToAdd, CandidateRuntimeSequence);
	if (NewInstance->GetAmount() <= 0
		|| NewInstance->GetRuntimeSequence() == 0
		|| NewInstance->GetStatusId().IsNone()
		|| NewInstance->GetOwner() != Owner.Get())
	{
		UE_LOG(LogTemp, Error, TEXT("[Status] Newly created runtime status failed post-initialize validation."));
		return Result;
	}

	Statuses.Add(NewInstance);

	Result.Outcome = EStatusMutationOutcome::Committed;
	Result.StatusId = NewInstance->GetStatusId();
	Result.RuntimeSequence = NewInstance->GetRuntimeSequence();
	Result.AmountBefore = 0;
	Result.AmountAfter = NewInstance->GetAmount();
	Result.bCreated = true;
	Result.bRemoved = false;
	Result.EffectiveInstance = NewInstance;
	Result.EffectiveDefinition = NewInstance->GetDefinition();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Status] Created %s on %s Amount=%d."),
		*NewInstance->GetDebugLabel(),
		*GetNameSafe(Owner.Get()),
		NewInstance->GetAmount()
	);
	LogState(TEXT("AfterCreate"));
	return Result;
}

FStatusMutationResult UStatusContainer::ReduceStatusCommit(
	UStatusInstance* ExpectedInstance,
	int32 AmountToRemove
)
{
	FStatusMutationResult Result;

	if (!IsValid(Owner.Get()) || !IsValid(ExpectedInstance) || AmountToRemove <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Status] ReduceStatus rejected: invalid Owner, ExpectedInstance or AmountToRemove=%d."), AmountToRemove);
		return Result;
	}

	if (ExpectedInstance->GetOwner() != Owner.Get())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Status] ReduceStatus rejected: ExpectedInstance belongs to a different owner."));
		return Result;
	}

	const int32 Index = FindExactStatusIndex(ExpectedInstance);
	if (Index == INDEX_NONE)
	{
		Result.Outcome = EStatusMutationOutcome::NoOp;
		Result.StatusId = ExpectedInstance->GetStatusId();
		Result.RuntimeSequence = ExpectedInstance->GetRuntimeSequence();
		Result.AmountBefore = FMath::Max(ExpectedInstance->GetAmount(), 0);
		Result.AmountAfter = Result.AmountBefore;
		Result.EffectiveInstance = ExpectedInstance;
		Result.EffectiveDefinition = ExpectedInstance->GetDefinition();
		return Result;
	}

	UStatusData* EffectiveDefinition = ExpectedInstance->GetDefinition();
	const int32 OldAmount = ExpectedInstance->GetAmount();
	if (!IsValid(EffectiveDefinition)
		|| ExpectedInstance->GetStatusId().IsNone()
		|| ExpectedInstance->GetRuntimeSequence() == 0
		|| OldAmount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Status] ReduceStatus rejected: exact runtime instance is structurally invalid."));
		return Result;
	}

	Result.StatusId = ExpectedInstance->GetStatusId();
	Result.RuntimeSequence = ExpectedInstance->GetRuntimeSequence();
	Result.AmountBefore = OldAmount;
	Result.AmountAfter = OldAmount;
	Result.EffectiveInstance = ExpectedInstance;
	Result.EffectiveDefinition = EffectiveDefinition;

	const FString Label = ExpectedInstance->GetDebugLabel();
	if (OldAmount <= AmountToRemove)
	{
		Statuses.RemoveAt(Index);
		Result.Outcome = EStatusMutationOutcome::Committed;
		Result.AmountAfter = 0;
		Result.bRemoved = true;
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Status] Reduced %s on %s: Amount %d - %d => removed exact instance."),
			*Label,
			*GetNameSafe(Owner.Get()),
			OldAmount,
			AmountToRemove
		);
		LogState(TEXT("AfterReduceRemove"));
		return Result;
	}

	if (!ExpectedInstance->ReduceAmount(AmountToRemove))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Status] ReduceStatus failed for %s."), *Label);
		return Result;
	}

	Result.AmountAfter = ExpectedInstance->GetAmount();
	if (Result.AmountAfter == Result.AmountBefore)
	{
		Result.Outcome = EStatusMutationOutcome::NoOp;
		return Result;
	}

	Result.Outcome = EStatusMutationOutcome::Committed;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Status] Reduced %s on %s: Amount %d - %d = %d."),
		*Label,
		*GetNameSafe(Owner.Get()),
		OldAmount,
		AmountToRemove,
		Result.AmountAfter
	);
	LogState(TEXT("AfterReduce"));
	return Result;
}

FStatusMutationResult UStatusContainer::RemoveStatusCommit(UStatusInstance* ExpectedInstance)
{
	FStatusMutationResult Result;

	if (!IsValid(Owner.Get()) || !IsValid(ExpectedInstance))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Status] RemoveStatus rejected: invalid Owner or ExpectedInstance."));
		return Result;
	}

	if (ExpectedInstance->GetOwner() != Owner.Get())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Status] RemoveStatus rejected: ExpectedInstance belongs to a different owner."));
		return Result;
	}

	const int32 Index = FindExactStatusIndex(ExpectedInstance);
	if (Index == INDEX_NONE)
	{
		Result.Outcome = EStatusMutationOutcome::NoOp;
		Result.StatusId = ExpectedInstance->GetStatusId();
		Result.RuntimeSequence = ExpectedInstance->GetRuntimeSequence();
		Result.AmountBefore = FMath::Max(ExpectedInstance->GetAmount(), 0);
		Result.AmountAfter = Result.AmountBefore;
		Result.EffectiveInstance = ExpectedInstance;
		Result.EffectiveDefinition = ExpectedInstance->GetDefinition();
		return Result;
	}

	UStatusData* EffectiveDefinition = ExpectedInstance->GetDefinition();
	const int32 OldAmount = ExpectedInstance->GetAmount();
	if (!IsValid(EffectiveDefinition)
		|| ExpectedInstance->GetStatusId().IsNone()
		|| ExpectedInstance->GetRuntimeSequence() == 0
		|| OldAmount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Status] RemoveStatus rejected: exact runtime instance is structurally invalid."));
		return Result;
	}

	Result.Outcome = EStatusMutationOutcome::Committed;
	Result.StatusId = ExpectedInstance->GetStatusId();
	Result.RuntimeSequence = ExpectedInstance->GetRuntimeSequence();
	Result.AmountBefore = OldAmount;
	Result.AmountAfter = 0;
	Result.bRemoved = true;
	Result.EffectiveInstance = ExpectedInstance;
	Result.EffectiveDefinition = EffectiveDefinition;

	const FString RemovedLabel = ExpectedInstance->GetDebugLabel();
	Statuses.RemoveAt(Index);
	UE_LOG(LogTemp, Log, TEXT("[Status] Removed %s from %s by exact instance."), *RemovedLabel, *GetNameSafe(Owner.Get()));
	LogState(TEXT("AfterRemove"));
	return Result;
}

UStatusInstance* UStatusContainer::ApplyStatus(
	UStatusData* Definition,
	int32 AmountToAdd,
	uint64 CandidateRuntimeSequence,
	bool& bOutCreated
)
{
	const FStatusMutationResult Result = ApplyStatusCommit(
		Definition,
		AmountToAdd,
		CandidateRuntimeSequence
	);
	bOutCreated = Result.IsCommitted() && Result.bCreated;
	return Result.Outcome == EStatusMutationOutcome::Invalid
		? nullptr
		: Result.EffectiveInstance;
}

bool UStatusContainer::ReduceStatus(UStatusInstance* ExpectedInstance, int32 AmountToRemove)
{
	return ReduceStatusCommit(ExpectedInstance, AmountToRemove).IsCommitted();
}

bool UStatusContainer::ContainsStatusInstance(const UStatusInstance* Instance) const
{
	if (!IsValid(Instance))
	{
		return false;
	}

	return FindExactStatusIndex(Instance) != INDEX_NONE;
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

int32 UStatusContainer::FindExactStatusIndex(const UStatusInstance* ExpectedInstance) const
{
	if (!IsValid(ExpectedInstance))
	{
		return INDEX_NONE;
	}

	return Statuses.IndexOfByPredicate(
		[ExpectedInstance](const TObjectPtr<UStatusInstance>& InstancePtr)
		{
			return InstancePtr.Get() == ExpectedInstance;
		}
	);
}

bool UStatusContainer::RemoveStatusById(FName StatusId)
{
	UStatusInstance* ExpectedInstance = FindMutableStatusById(StatusId);
	if (!IsValid(ExpectedInstance))
	{
		return false;
	}

	return RemoveStatusCommit(ExpectedInstance).IsCommitted();
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
