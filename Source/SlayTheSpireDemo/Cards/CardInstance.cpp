#include "CardInstance.h"

#include "CardData.h"
#include "Effects/CardEffect.h"

void UCardInstance::Initialize(UCardData* InDefinition, int32 InRuntimeId)
{
	Definition = InDefinition;
	RuntimeId = InRuntimeId;
	bUpgraded = false;
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
	return IsValid(Definition.Get())
		&& !bUpgraded
		&& IsValid(Definition->UpgradedVariant.Get());
}

const UCardVariantData* UCardInstance::GetActiveUpgradedVariant() const
{
	if (!bUpgraded || !IsValid(Definition.Get()))
	{
		return nullptr;
	}

	return IsValid(Definition->UpgradedVariant.Get())
		? Definition->UpgradedVariant.Get()
		: nullptr;
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
	if (const UCardVariantData* Variant = GetActiveUpgradedVariant())
	{
		return Variant->DisplayName;
	}
	return IsValid(Definition.Get()) ? Definition->DisplayName : FText::GetEmpty();
}

FText UCardInstance::GetDescriptionFormat() const
{
	if (const UCardVariantData* Variant = GetActiveUpgradedVariant())
	{
		return Variant->Description;
	}
	return IsValid(Definition.Get()) ? Definition->Description : FText::GetEmpty();
}

UTexture2D* UCardInstance::GetCardArt() const
{
	if (const UCardVariantData* Variant = GetActiveUpgradedVariant())
	{
		return Variant->CardArt.Get();
	}
	return IsValid(Definition.Get()) ? Definition->CardArt.Get() : nullptr;
}

ECardType UCardInstance::GetCardType() const
{
	if (const UCardVariantData* Variant = GetActiveUpgradedVariant())
	{
		return Variant->CardType;
	}
	return IsValid(Definition.Get()) ? Definition->CardType : ECardType::Attack;
}

int32 UCardInstance::GetCurrentCost() const
{
	if (const UCardVariantData* Variant = GetActiveUpgradedVariant())
	{
		return FMath::Max(0, Variant->Cost);
	}
	return IsValid(Definition.Get()) ? FMath::Max(0, Definition->BaseCost) : 0;
}

ECardTargetType UCardInstance::GetTargetType() const
{
	if (const UCardVariantData* Variant = GetActiveUpgradedVariant())
	{
		return Variant->TargetType;
	}
	return IsValid(Definition.Get()) ? Definition->TargetType : ECardTargetType::None;
}

ECardDestination UCardInstance::ResolveDestination() const
{
	if (const UCardVariantData* Variant = GetActiveUpgradedVariant())
	{
		return Variant->DefaultDestination;
	}
	return IsValid(Definition.Get()) ? Definition->DefaultDestination : ECardDestination::Discard;
}

const TArray<TObjectPtr<UCardEffect>>& UCardInstance::GetEffects() const
{
	static const TArray<TObjectPtr<UCardEffect>> EmptyEffects;
	if (const UCardVariantData* Variant = GetActiveUpgradedVariant())
	{
		return Variant->Effects;
	}
	return IsValid(Definition.Get()) ? Definition->Effects : EmptyEffects;
}

FString UCardInstance::GetDebugLabel() const
{
	const FName CardId = GetCardId();
	const FString StableId = CardId.IsNone() ? TEXT("UnknownCard") : CardId.ToString();
	return FString::Printf(TEXT("%s#%d"), *StableId, RuntimeId);
}
