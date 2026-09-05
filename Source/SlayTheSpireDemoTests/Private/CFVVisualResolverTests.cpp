#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Texture2D.h"
#include "UI/CardFaceStyleSet.h"

namespace CFVVisualResolver
{
	FCardFaceTextureRegion MakeRegion(
		UTexture2D* Texture,
		const FVector2D& Position = FVector2D(1.0, 2.0),
		const FVector2D& Size = FVector2D(10.0, 20.0))
	{
		FCardFaceTextureRegion Region;
		Region.Texture = Texture;
		Region.Placement.Position = Position;
		Region.Placement.Size = Size;
		return Region;
	}

	UTexture2D* MakeTexture()
	{
		return NewObject<UTexture2D>(GetTransientPackage());
	}
}

using namespace CFVVisualResolver;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCFVVisualResolverTest,
	"SlayTheSpireDemo.CFV.VisualResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCFVVisualResolverTest::RunTest(const FString& Parameters)
{
	FCardFaceStyleConfig Config;

	UTexture2D* CommonAttackFrame = MakeTexture();
	UTexture2D* CommonSkillFrame = MakeTexture();
	UTexture2D* CommonPowerFrame = MakeTexture();
	UTexture2D* CommonBanner = MakeTexture();
	UTexture2D* UncommonBanner = MakeTexture();
	UTexture2D* RareBanner = MakeTexture();

	Config.CommonStyle.AttackFrame = MakeRegion(CommonAttackFrame);
	Config.CommonStyle.SkillFrame = MakeRegion(CommonSkillFrame);
	Config.CommonStyle.PowerFrame = MakeRegion(CommonPowerFrame);
	Config.CommonStyle.Banner = MakeRegion(CommonBanner);
	Config.UncommonStyle.Banner = MakeRegion(UncommonBanner);
	Config.RareStyle.Banner = MakeRegion(RareBanner);

	const ECardColor Colors[] = {
		ECardColor::Red,
		ECardColor::Green,
		ECardColor::Blue,
		ECardColor::Purple,
		ECardColor::Colorless,
		ECardColor::Curse
	};

	TMap<ECardColor, UTexture2D*> ExpectedAttackBackgrounds;
	TMap<ECardColor, UTexture2D*> ExpectedOrbs;
	for (ECardColor Color : Colors)
	{
		FCardColorVisualStyle Style;
		UTexture2D* AttackBackground = MakeTexture();
		UTexture2D* Orb = MakeTexture();
		Style.AttackBackground = MakeRegion(AttackBackground);
		Style.SkillBackground = MakeRegion(MakeTexture());
		Style.PowerBackground = MakeRegion(MakeTexture());
		Style.CostOrb = MakeRegion(Orb);
		Config.ColorStyles.Add(Color, Style);
		ExpectedAttackBackgrounds.Add(Color, AttackBackground);
		ExpectedOrbs.Add(Color, Orb);
	}

	for (ECardColor Color : Colors)
	{
		const FResolvedCardFaceStyle Resolved = ResolveCardFaceStyle(
			Color,
			ECardType::Attack,
			ECardRarity::Common,
			Config);
		TestTrue(
			*FString::Printf(TEXT("Color exact-key background selection %d"), static_cast<int32>(Color)),
			Resolved.BackgroundRegion.Texture.Get() == ExpectedAttackBackgrounds.FindRef(Color));
		TestTrue(
			*FString::Printf(TEXT("Color exact-key orb selection %d"), static_cast<int32>(Color)),
			Resolved.CostOrbRegion.Texture.Get() == ExpectedOrbs.FindRef(Color));
	}

	UTexture2D* RedAttack = Config.ColorStyles.FindChecked(ECardColor::Red).AttackBackground.Texture.Get();
	UTexture2D* RedSkill = Config.ColorStyles.FindChecked(ECardColor::Red).SkillBackground.Texture.Get();
	UTexture2D* RedPower = Config.ColorStyles.FindChecked(ECardColor::Red).PowerBackground.Texture.Get();

	struct FShapeCase
	{
		ECardType CardType;
		ECardFaceVisualShape Shape;
		UTexture2D* Background;
		UTexture2D* Frame;
	};

	const FShapeCase ShapeCases[] = {
		{ ECardType::Attack, ECardFaceVisualShape::Attack, RedAttack, CommonAttackFrame },
		{ ECardType::Skill, ECardFaceVisualShape::Skill, RedSkill, CommonSkillFrame },
		{ ECardType::Power, ECardFaceVisualShape::Power, RedPower, CommonPowerFrame },
		{ ECardType::Status, ECardFaceVisualShape::Skill, RedSkill, CommonSkillFrame },
		{ ECardType::Curse, ECardFaceVisualShape::Skill, RedSkill, CommonSkillFrame }
	};

	for (const FShapeCase& ShapeCase : ShapeCases)
	{
		const FResolvedCardFaceStyle Resolved = ResolveCardFaceStyle(
			ECardColor::Red,
			ShapeCase.CardType,
			ECardRarity::Common,
			Config);
		TestTrue(TEXT("CardType maps to expected visual shape"), Resolved.VisualShape == ShapeCase.Shape);
		TestTrue(TEXT("Visual shape selects expected background"), Resolved.BackgroundRegion.Texture.Get() == ShapeCase.Background);
		TestTrue(TEXT("Visual shape selects expected frame"), Resolved.FrameRegion.Texture.Get() == ShapeCase.Frame);
	}

	struct FRarityCase
	{
		ECardRarity Rarity;
		UTexture2D* Banner;
	};

	const FRarityCase RarityCases[] = {
		{ ECardRarity::Basic, CommonBanner },
		{ ECardRarity::Common, CommonBanner },
		{ ECardRarity::Special, CommonBanner },
		{ ECardRarity::Curse, CommonBanner },
		{ ECardRarity::Uncommon, UncommonBanner },
		{ ECardRarity::Rare, RareBanner }
	};

	for (const FRarityCase& RarityCase : RarityCases)
	{
		const FResolvedCardFaceStyle Resolved = ResolveCardFaceStyle(
			ECardColor::Red,
			ECardType::Attack,
			RarityCase.Rarity,
			Config);
		TestTrue(TEXT("Semantic rarity selects expected visual rarity style"), Resolved.BannerRegion.Texture.Get() == RarityCase.Banner);
	}

	FCardFaceStyleConfig MissingColorConfig = Config;
	MissingColorConfig.ColorStyles.Empty();
	MissingColorConfig.ColorStyles.Add(ECardColor::Red, Config.ColorStyles.FindChecked(ECardColor::Red));
	const FResolvedCardFaceStyle MissingGreen = ResolveCardFaceStyle(
		ECardColor::Green,
		ECardType::Attack,
		ECardRarity::Common,
		MissingColorConfig);
	TestNull(TEXT("Missing Green has no background and does not fall back to Red"), MissingGreen.BackgroundRegion.Texture.Get());
	TestNull(TEXT("Missing Green has no cost orb and does not fall back to Red"), MissingGreen.CostOrbRegion.Texture.Get());
	TestTrue(TEXT("Missing color still keeps shared rarity frame"), MissingGreen.FrameRegion.Texture.Get() == CommonAttackFrame);
	TestTrue(TEXT("Missing color still keeps shared rarity banner"), MissingGreen.BannerRegion.Texture.Get() == CommonBanner);

	const FResolvedCardFaceStyle InvalidType = ResolveCardFaceStyle(
		ECardColor::Red,
		static_cast<ECardType>(0xFE),
		ECardRarity::Common,
		Config);
	TestTrue(TEXT("Invalid direct CardType defensively falls back to Skill visual shape"), InvalidType.VisualShape == ECardFaceVisualShape::Skill);

	const FResolvedCardFaceStyle InvalidRarity = ResolveCardFaceStyle(
		ECardColor::Red,
		ECardType::Attack,
		static_cast<ECardRarity>(0xFE),
		Config);
	TestTrue(TEXT("Invalid direct rarity defensively falls back to Common visual style"), InvalidRarity.BannerRegion.Texture.Get() == CommonBanner);

	return true;
}

#endif
