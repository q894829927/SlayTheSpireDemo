#include "CardInstance.h"

#include "CardData.h"
#include "Effects/CardEffect.h"

void UCardInstance::Initialize(UCardData* InDefinition, int32 InRuntimeId, bool bStartUpgraded)
{
	Definition = InDefinition;
	RuntimeId = InRuntimeId;
	bUpgraded = bStartUpgraded;
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

bool UCardInstance::IsUpgraded() const
{
	return bUpgraded;
}

bool UCardInstance::CanUpgrade() const
{
	return IsValid(Definition.Get()) && !bUpgraded;
}

bool UCardInstance::CommitUpgrade()
{
	if (!CanUpgrade())
	{
		return false;
	}

	bUpgraded = true;
	return true;
}

FText UCardInstance::GetDisplayName() const
{
	return IsValid(Definition.Get()) ? Definition->DisplayName : FText::GetEmpty();
}

UTexture2D* UCardInstance::GetCardArt() const
{
	return IsValid(Definition.Get()) ? Definition->CardArt.Get() : nullptr;
}

ECardType UCardInstance::GetCardType() const
{
	return IsValid(Definition.Get()) ? Definition->CardType : ECardType::Attack;
}

ECardTargetType UCardInstance::GetTargetType() const
{
	return IsValid(Definition.Get()) ? Definition->TargetType : ECardTargetType::None;
}

FText UCardInstance::GetDescriptionFormat() const
{
	return IsValid(Definition.Get()) ? Definition->Description : FText::GetEmpty();
}

int32 UCardInstance::GetCurrentCost() const
{
	if (!IsValid(Definition.Get()))
	{
		return 0;
	}
	return FMath::Max(0, bUpgraded ? Definition->UpgradedCost : Definition->BaseCost);
}

ECardDestination UCardInstance::ResolveDestination() const
{
	return IsValid(Definition.Get())
		? Definition->DefaultDestination
		: ECardDestination::Discard;
}

const TArray<TObjectPtr<UCardEffect>>& UCardInstance::GetEffects() const
{
	static const TArray<TObjectPtr<UCardEffect>> EmptyEffects;
	return IsValid(Definition.Get()) ? Definition->Effects : EmptyEffects;
}

FString UCardInstance::GetDebugLabel() const
{
	const FName CardId = GetCardId();
	const FString StableId = CardId.IsNone() ? TEXT("UnknownCard") : CardId.ToString();
	return FString::Printf(TEXT("%s#%d"), *StableId, RuntimeId);
}
