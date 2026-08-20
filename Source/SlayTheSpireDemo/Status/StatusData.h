#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../Events/BattleTrigger.h"
#include "../Modifiers/Block/BlockModifier.h"
#include "../Modifiers/Damage/DamageModifier.h"
#include "StatusData.generated.h"

USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FStatusAtlasRegion
{
	GENERATED_BODY()

	// When false the HUD should not sample the shared status atlas for this status.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Presentation")
	bool bUseAtlasIcon = false;

	// Normalized atlas-space origin consumed by M_StatusAtlas.UVOffset.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Presentation")
	FVector2D UVOffset = FVector2D::ZeroVector;

	// Normalized packed-region size consumed by M_StatusAtlas.UVScale.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Presentation")
	FVector2D UVScale = FVector2D::UnitVector;

	// Normalized position of the trimmed image inside its original logical canvas.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Presentation")
	FVector2D TrimOffset = FVector2D::ZeroVector;

	// Normalized trimmed-image size relative to the original logical canvas.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Presentation")
	FVector2D TrimScale = FVector2D::UnitVector;
};

UCLASS(BlueprintType)
class SLAYTHESPIREDEMO_API UStatusData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Identity")
	FName StatusId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Identity")
	FText DisplayName;

	// Serialized name intentionally remains Description for existing .uasset
	// compatibility. The authored value is an FText::Format pattern.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Presentation", meta = (MultiLine = "true", DisplayName = "Description Format"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status|Presentation")
	FStatusAtlasRegion IconRegion;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Status|Modifiers|Damage")
	TArray<TObjectPtr<UDamageModifier>> DamageModifiers;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Status|Modifiers|Block")
	TArray<TObjectPtr<UBlockModifier>> BlockModifiers;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Status|Triggers")
	TArray<TObjectPtr<UBattleTrigger>> Triggers;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
