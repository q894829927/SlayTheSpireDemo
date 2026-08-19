#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "CardTypes.h"
#include "Effects/CardEffect.h"
#include "CardData.generated.h"

UCLASS(BlueprintType)
class SLAYTHESPIREDEMO_API UCardData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Identity")
	FName CardId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card|Presentation")
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
};
