#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CardTypes.h"
#include "CardInstance.generated.h"

class UCardData;
class UCardEffect;
class UTexture2D;
class UUpgradeCardAction;

UCLASS()
class SLAYTHESPIREDEMO_API UCardInstance : public UObject
{
	GENERATED_BODY()

public:
	// Creation-time upgraded state belongs to the runtime instance, never the
	// shared UCardData definition. In-combat upgrades still go through
	// UUpgradeCardAction / CommitUpgrade().
	void Initialize(UCardData* InDefinition, int32 InRuntimeId, bool bStartUpgraded = false);

	const UCardData* GetDefinition() const;
	int32 GetRuntimeId() const;
	FName GetCardId() const;

	bool IsUpgraded() const;
	bool CanUpgrade() const;

	// Stable metadata never changes for an ordinary upgrade.
	FText GetDisplayName() const;
	UTexture2D* GetCardArt() const;
	ECardType GetCardType() const;
	ECardRarity GetRarity() const;
	ECardColor GetCardColor() const;
	ECardTargetType GetTargetType() const;

	// Ordinary upgrades keep one shared Description / Destination / Effects
	// composition. Only typed authored values such as cost/effect amounts select
	// Base vs Upgraded using this instance's bUpgraded state.
	FText GetDescriptionFormat() const;
	int32 GetCurrentCost() const;
	ECardDestination ResolveDestination() const;
	const TArray<TObjectPtr<UCardEffect>>& GetEffects() const;

	FString GetDebugLabel() const;

private:
	friend class UUpgradeCardAction;

	bool CommitUpgrade();

	UPROPERTY(Transient)
	TObjectPtr<UCardData> Definition = nullptr;

	UPROPERTY(Transient)
	int32 RuntimeId = INDEX_NONE;

	UPROPERTY(Transient)
	bool bUpgraded = false;
};
