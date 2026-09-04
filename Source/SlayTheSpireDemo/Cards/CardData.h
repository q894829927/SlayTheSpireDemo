#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "CardTypes.h"
#include "Effects/CardEffect.h"
#include "CardData.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SLAYTHESPIREDEMO_API UCardVariantData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Variant|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Variant|Presentation", meta = (MultiLine = "true", DisplayName = "Description Format"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Variant|Presentation")
	TObjectPtr<UTexture2D> CardArt = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Variant|Rules")
	ECardType CardType = ECardType::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Variant|Rules")
	ECardTargetType TargetType = ECardTargetType::Enemy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Variant|Rules", meta = (ClampMin = "0"))
	int32 Cost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Variant|Rules")
	ECardDestination DefaultDestination = ECardDestination::Discard;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Card|Variant|Effects")
	TArray<TObjectPtr<UCardEffect>> Effects;
};

UCLASS(BlueprintType)
class SLAYTHESPIREDEMO_API UCardData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Identity")
	FName CardId = NAME_None;

	// Existing serialized fields are the Base configuration. Keeping their names
	// preserves current .uasset compatibility while ordinary upgrades simply
	// switch the runtime instance to UpgradedVariant.
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Rules")
	ECardDestination DefaultDestination = ECardDestination::Discard;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Card|Effects")
	TArray<TObjectPtr<UCardEffect>> Effects;

	// Optional second authored configuration for the normal one-time upgrade.
	// CardId remains owned by UCardData so Base and Plus are one card identity.
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Card|Upgrade")
	TObjectPtr<UCardVariantData> UpgradedVariant = nullptr;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
