#include "CardInstance.h"

#include "CardData.h"

void UCardInstance::Initialize(UCardData* InDefinition, int32 InRuntimeId)
{
	Definition = InDefinition;
	RuntimeId = InRuntimeId;
}

const UCardData* UCardInstance::GetDefinition() const
{
	return Definition.Get();
}

int32 UCardInstance::GetRuntimeId() const
{
	return RuntimeId;
}

FName UCardInstance::GetCardId() const
{
	return IsValid(Definition.Get()) ? Definition->CardId : NAME_None;
}

int32 UCardInstance::GetCurrentCost() const
{
	return IsValid(Definition.Get()) ? FMath::Max(0, Definition->BaseCost) : 0;
}

ECardTargetType UCardInstance::GetTargetType() const
{
	return IsValid(Definition.Get()) ? Definition->TargetType : ECardTargetType::None;
}

ECardDestination UCardInstance::ResolveDestination() const
{
	return IsValid(Definition.Get()) ? Definition->DefaultDestination : ECardDestination::Discard;
}

FString UCardInstance::GetDebugLabel() const
{
	const FName CardId = GetCardId();
	const FString StableId = CardId.IsNone() ? TEXT("UnknownCard") : CardId.ToString();
	return FString::Printf(TEXT("%s#%d"), *StableId, RuntimeId);
}
