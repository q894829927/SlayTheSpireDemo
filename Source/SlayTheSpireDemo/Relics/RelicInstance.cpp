#include "RelicInstance.h"

#include "RelicData.h"

void URelicInstance::Initialize(URelicData* InDefinition, ABattleManager* InBattle, uint64 InRuntimeSequence)
{
	Definition = InDefinition;
	Battle = InBattle;
	RuntimeSequence = InRuntimeSequence;
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

FString URelicInstance::GetDebugLabel() const
{
	const FName RelicId = GetRelicId();
	const FString StableId = RelicId.IsNone() ? TEXT("UnknownRelic") : RelicId.ToString();
	return FString::Printf(TEXT("%s#%llu"), *StableId, static_cast<unsigned long long>(RuntimeSequence));
}
