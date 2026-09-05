#include "CardFaceStyleSet.h"

namespace
{
	ECardFaceVisualShape ResolveVisualShape(ECardType CardType)
	{
		switch (CardType)
		{
		case ECardType::Attack:
			return ECardFaceVisualShape::Attack;
		case ECardType::Power:
			return ECardFaceVisualShape::Power;
		case ECardType::Skill:
		case ECardType::Status:
		case ECardType::Curse:
		default:
			return ECardFaceVisualShape::Skill;
		}
	}

	const FCardRarityVisualStyle& ResolveRarityStyle(
		ECardRarity Rarity,
		const FCardFaceStyleConfig& Config)
	{
		switch (Rarity)
		{
		case ECardRarity::Uncommon:
			return Config.UncommonStyle;
		case ECardRarity::Rare:
			return Config.RareStyle;
		case ECardRarity::Basic:
		case ECardRarity::Common:
		case ECardRarity::Special:
		case ECardRarity::Curse:
		default:
			return Config.CommonStyle;
		}
	}

	FCardFaceTextureRegion ResolveBackground(
		const FCardColorVisualStyle& ColorStyle,
		ECardFaceVisualShape Shape)
	{
		switch (Shape)
		{
		case ECardFaceVisualShape::Attack:
			return ColorStyle.AttackBackground;
		case ECardFaceVisualShape::Power:
			return ColorStyle.PowerBackground;
		case ECardFaceVisualShape::Skill:
		default:
			return ColorStyle.SkillBackground;
		}
	}

	FCardFaceTextureRegion ResolveFrame(
		const FCardRarityVisualStyle& RarityStyle,
		ECardFaceVisualShape Shape)
	{
		switch (Shape)
		{
		case ECardFaceVisualShape::Attack:
			return RarityStyle.AttackFrame;
		case ECardFaceVisualShape::Power:
			return RarityStyle.PowerFrame;
		case ECardFaceVisualShape::Skill:
		default:
			return RarityStyle.SkillFrame;
		}
	}
}

FResolvedCardFaceStyle ResolveCardFaceStyle(
	ECardColor CardColor,
	ECardType CardType,
	ECardRarity Rarity,
	const FCardFaceStyleConfig& Config)
{
	FResolvedCardFaceStyle Resolved;
	Resolved.VisualShape = ResolveVisualShape(CardType);

	if (const FCardColorVisualStyle* ColorStyle = Config.ColorStyles.Find(CardColor))
	{
		Resolved.BackgroundRegion = ResolveBackground(*ColorStyle, Resolved.VisualShape);
		Resolved.CostOrbRegion = ColorStyle->CostOrb;
	}

	const FCardRarityVisualStyle& RarityStyle = ResolveRarityStyle(Rarity, Config);
	Resolved.FrameRegion = ResolveFrame(RarityStyle, Resolved.VisualShape);
	Resolved.BannerRegion = RarityStyle.Banner;
	Resolved.TypeLeft = RarityStyle.TypeLeft.Get();
	Resolved.TypeCenter = RarityStyle.TypeCenter.Get();
	Resolved.TypeRight = RarityStyle.TypeRight.Get();

	return Resolved;
}
