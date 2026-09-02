#include "RelicContainer.h"

#include "RelicData.h"
#include "RelicInstance.h"
#include "../Battle/BattleManager.h"

void URelicContainer::Initialize(ABattleManager* InBattle)
{
	Battle = InBattle;
	Relics.Reset();
}

void URelicContainer::Reset()
{
	Relics.Reset();
	Battle = nullptr;
}

FRelicAddResult URelicContainer::AddRelic(URelicData* Definition)
{
	FRelicAddResult Result;
	if (!IsValid(Battle.Get()) || !IsValid(Definition) || Definition->RelicId.IsNone())
	{
		return Result;
	}

	if (URelicInstance* Existing = FindMutableRelicById(Definition->RelicId))
	{
		Result.Outcome = ERelicAddOutcome::Duplicate;
		Result.Instance = Existing;
		return Result;
	}

	const uint64 RuntimeSequence = Battle->AllocateRuntimeSequence();
	if (RuntimeSequence == 0)
	{
		return Result;
	}

	URelicInstance* Instance = NewObject<URelicInstance>(this);
	if (!IsValid(Instance))
	{
		return Result;
	}

	Instance->Initialize(Definition, Battle.Get(), RuntimeSequence);
	Relics.Add(Instance);

	Result.Outcome = ERelicAddOutcome::Added;
	Result.Instance = Instance;
	return Result;
}

const URelicInstance* URelicContainer::FindRelicById(FName RelicId) const
{
	return FindMutableRelicById(RelicId);
}

bool URelicContainer::ContainsRelic(FName RelicId) const
{
	return FindMutableRelicById(RelicId) != nullptr;
}

bool URelicContainer::ContainsRelicInstance(const URelicInstance* Instance) const
{
	if (!IsValid(Instance))
	{
		return false;
	}

	for (const TObjectPtr<URelicInstance>& Candidate : Relics)
	{
		if (Candidate.Get() == Instance)
		{
			return true;
		}
	}
	return false;
}

const TArray<TObjectPtr<URelicInstance>>& URelicContainer::GetRelics() const
{
	return Relics;
}

ABattleManager* URelicContainer::GetBattle() const
{
	return Battle.Get();
}

URelicInstance* URelicContainer::FindMutableRelicById(FName RelicId) const
{
	if (RelicId.IsNone())
	{
		return nullptr;
	}

	for (const TObjectPtr<URelicInstance>& Instance : Relics)
	{
		if (IsValid(Instance.Get()) && Instance->GetRelicId() == RelicId)
		{
			return Instance.Get();
		}
	}
	return nullptr;
}
