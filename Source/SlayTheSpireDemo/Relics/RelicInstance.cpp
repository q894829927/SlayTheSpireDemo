#include "RelicInstance.h"

#include "RelicData.h"

void URelicInstance::Initialize(URelicData* InDefinition, ABattleManager* InBattle, uint64 InRuntimeSequence)
{
	Definition = InDefinition;
	Battle = InBattle;
	RuntimeSequence = InRuntimeSequence;
	Counter = 0;
}

URelicData* URelicInstance::GetDefinition() const
{
	return Definition.Get();
}

FName URelicInstance::GetRelicId() const
{
	return IsValid(Definition.Get()) ? Definition->RelicId : NAME_None;
}

uint64 URelicInstance::GetRuntimeSequence() const
{
	return RuntimeSequence;
}

ABattleManager* URelicInstance::GetBattle() const
{
	return Battle.Get();
}

int32 URelicInstance::GetCounter() const
{
	return Counter;
}

void URelicInstance::SetCounterFromAction(int32 InCounter)
{
	Counter = FMath::Max(0, InCounter);
}

FString URelicInstance::GetDebugLabel() const
{
	const FName RelicId = GetRelicId();
	const FString StableId = RelicId.IsNone() ? TEXT("UnknownRelic") : RelicId.ToString();
	return FString::Printf(TEXT("%s#%llu"), *StableId, static_cast<unsigned long long>(RuntimeSequence));
}
