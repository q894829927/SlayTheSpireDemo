#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "CardTypes.h"
#include "Effects/CardEffect.h"
#include "CardData.generated.h"

// Load-compatibility shim for any asset that was saved during the brief
// UCardVariantData implementation window. Ordinary card authoring no longer
// references or exposes this class. Keep the original reflected class name so
// those serialized subobjects can still be resolved when an old asset is loaded.
// Do not mark this UCLASS(Deprecated): Unreal requires deprecated reflected
// classes to use the UDEPRECATED_ prefix, which would change the reflected name
// and defeat this compatibility purpose.
UCLASS()
class SLAYTHESPIREDEMO_API UCardVariantData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	FText Description;

	UPROPERTY()
	TObjectPtr<UTexture2D> CardArt = nullptr;

	UPROPERTY()
	ECardType CardType = ECardType::Attack;

	UPROPERTY()
	ECardTargetType TargetType = ECardTargetType::Enemy;

	UPROPERTY()
	int32 Cost = 1;

	UPROPERTY()
	ECardDestination DefaultDestination = ECardDestination::Discard;

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UCardEffect>> Effects;
};

UCLASS(BlueprintType)
class SLAYTHESPIREDEMO_API UCardData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Identity")
	FName CardId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Identity")
	FText DisplayName;

	// Serialized name intentionally remains Description for existing .uasset
	// compatibility. The authored value is an FText::Format pattern.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Presentation", meta = (MultiLine = "true", DisplayName = "Description Format"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Presentation")
	TObjectPtr<UTexture2D> CardArt = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Rules")
	ECardType CardType = ECardType::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Rules")
	ECardTargetType TargetType = ECardTargetType::Enemy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Rules", meta = (ClampMin = "0"))
	int32 BaseCost = 1;

	// STS-style typed upgrade value. No sentinel/fallback semantics: unchanged
	// cost is authored explicitly to the same value as BaseCost.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Rules", meta = (ClampMin = "0"))
	int32 UpgradedCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Rules")
	ECardDestination DefaultDestination = ECardDestination::Discard;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Card|Effects")
	TArray<TObjectPtr<UCardEffect>> Effects;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
