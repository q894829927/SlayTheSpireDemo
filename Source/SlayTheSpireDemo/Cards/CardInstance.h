#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CardTypes.h"
#include "CardInstance.generated.h"

class UCardData;
class UCardEffect;
class UCardVariantData;
class UTexture2D;
class UUpgradeCardAction;

UCLASS()
class SLAYTHESPIREDEMO_API UCardInstance : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UCardData* InDefinition, int32 InRuntimeId);

	const UCardData* GetDefinition() const;
	int32 GetRuntimeId() const;
	FName GetCardId() const;

	bool IsUpgraded() const;
	bool CanUpgrade() const;

	FText GetDisplayName() const;
	FText GetDescriptionFormat() const;
	UTexture2D* GetCardArt() const;
	ECardType GetCardType() const;
	int32 GetCurrentCost() const;
	ECardTargetType GetTargetType() const;
	ECardDestination ResolveDestination() const;
	const TArray<TObjectPtr<UCardEffect>>& GetEffects() const;

	FString GetDebugLabel() const;

private:
	friend class UUpgradeCardAction;

	const UCardVariantData* GetActiveUpgradedVariant() const;
	bool CommitUpgrade();

	UPROPERTY(Transient)
	TObjectPtr<UCardData> Definition = nullptr;

	UPROPERTY(Transient)
	int32 RuntimeId = INDEX_NONE;

	UPROPERTY(Transient)
	bool bUpgraded = false;
};
