#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "../Cards/CardTypes.h"
#include "CardFaceStyleSet.generated.h"

class UTexture2D;

/** Canonical placement inside the card's 300 x 420 texture-design space. */
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FCardFaceLayerPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FVector2D Size = FVector2D::ZeroVector;
};

/** A cropped atlas texture paired with the placement needed to reconstruct it. */
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FCardFaceTextureRegion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	TObjectPtr<UTexture2D> Texture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardFaceLayerPlacement Placement;
};

/** Color-specific resources. Color does not own layout. */
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FCardColorVisualStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardFaceTextureRegion AttackBackground;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardFaceTextureRegion SkillBackground;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardFaceTextureRegion PowerBackground;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardFaceTextureRegion CostOrb;
};

/** Shared rarity resources. */
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FCardRarityVisualStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardFaceTextureRegion Banner;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardFaceTextureRegion AttackFrame;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardFaceTextureRegion SkillFrame;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardFaceTextureRegion PowerFrame;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	TObjectPtr<UTexture2D> TypeLeft = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	TObjectPtr<UTexture2D> TypeCenter = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	TObjectPtr<UTexture2D> TypeRight = nullptr;
};

/**
 * Presentation-only authored configuration.
 *
 * Text/name/cost/description/type-plate geometry remains fixed in the Widget
 * Designer for the current CFV pass. Only cropped atlas layers carry placement
 * because their trim offsets differ by selected texture.
 */
USTRUCT(BlueprintType)
struct SLAYTHESPIREDEMO_API FCardFaceStyleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	TMap<ECardColor, FCardColorVisualStyle> ColorStyles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardRarityVisualStyle CommonStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardRarityVisualStyle UncommonStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardRarityVisualStyle RareStyle;
};

/** Narrow authored presentation DataAsset. It is not Gameplay state or a registry. */
UCLASS(BlueprintType)
class SLAYTHESPIREDEMO_API UCardFaceStyleSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card Face")
	FCardFaceStyleConfig Config;
};

/** Presentation-only shape derived from semantic CardType. */
enum class ECardFaceVisualShape : uint8
{
	Attack,
	Skill,
	Power
};

/** Fully resolved static card-face resource selection. */
struct SLAYTHESPIREDEMO_API FResolvedCardFaceStyle
{
	ECardFaceVisualShape VisualShape = ECardFaceVisualShape::Skill;
	FCardFaceTextureRegion BackgroundRegion;
	FCardFaceTextureRegion FrameRegion;
	FCardFaceTextureRegion BannerRegion;
	FCardFaceTextureRegion CostOrbRegion;
	UTexture2D* TypeLeft = nullptr;
	UTexture2D* TypeCenter = nullptr;
	UTexture2D* TypeRight = nullptr;
};

/** Pure Presentation resolver. No Widget, Gameplay, asset loading or TMap iteration fallback. */
SLAYTHESPIREDEMO_API FResolvedCardFaceStyle ResolveCardFaceStyle(
	ECardColor CardColor,
	ECardType CardType,
	ECardRarity Rarity,
	const FCardFaceStyleConfig& Config);
