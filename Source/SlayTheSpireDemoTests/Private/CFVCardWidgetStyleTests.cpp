#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CFVCardWidgetStyleTestTypes.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/CardFaceStyleSet.h"

namespace CFVCardWidgetStyle
{
	struct FProbeSurface
	{
		TObjectPtr<UCFVCardWidgetStyleProbe> Card;
		TObjectPtr<UTextBlock> Name;
		TObjectPtr<UTextBlock> Cost;
		TObjectPtr<URichTextBlock> Description;
		TObjectPtr<UTextBlock> Type;
		TObjectPtr<UImage> Art;
		TObjectPtr<UImage> Background;
		TObjectPtr<UImage> Frame;
		TObjectPtr<UImage> Banner;
		TObjectPtr<UImage> CostOrb;
	};

	UTexture2D* MakeTexture()
	{
		return NewObject<UTexture2D>(GetTransientPackage());
	}

	FCardFaceTextureRegion MakeRegion(
		UTexture2D* Texture,
		const FVector2D& Position,
		const FVector2D& Size)
	{
		FCardFaceTextureRegion Region;
		Region.Texture = Texture;
		Region.Placement.Position = Position;
		Region.Placement.Size = Size;
		return Region;
	}

	UObject* GetImageResource(const UImage* Image)
	{
		return IsValid(Image) ? Image->GetBrush().GetResourceObject() : nullptr;
	}

	UCanvasPanelSlot* GetCanvasSlot(UImage* Image)
	{
		return IsValid(Image) ? Cast<UCanvasPanelSlot>(Image->Slot) : nullptr;
	}

	FProbeSurface MakeProbe(bool bWithDecorative)
	{
		FProbeSurface Surface;
		Surface.Card = NewObject<UCFVCardWidgetStyleProbe>(GetTransientPackage());
		UButton* Button = NewObject<UButton>(Surface.Card);
		Surface.Name = NewObject<UTextBlock>(Surface.Card);
		Surface.Cost = NewObject<UTextBlock>(Surface.Card);
		Surface.Description = NewObject<URichTextBlock>(Surface.Card);
		Surface.Type = NewObject<UTextBlock>(Surface.Card);
		Surface.Art = NewObject<UImage>(Surface.Card);
		Surface.Card->ConfigureCore(
			Button,
			Surface.Name,
			Surface.Cost,
			Surface.Description,
			Surface.Type,
			Surface.Art);

		if (!bWithDecorative)
		{
			return Surface;
		}

		UCanvasPanel* TextureCanvas = NewObject<UCanvasPanel>(Surface.Card);
		Surface.Background = NewObject<UImage>(Surface.Card);
		Surface.Frame = NewObject<UImage>(Surface.Card);
		Surface.Banner = NewObject<UImage>(Surface.Card);
		Surface.CostOrb = NewObject<UImage>(Surface.Card);
		TextureCanvas->AddChildToCanvas(Surface.Background);
		TextureCanvas->AddChildToCanvas(Surface.Frame);
		TextureCanvas->AddChildToCanvas(Surface.Banner);
		TextureCanvas->AddChildToCanvas(Surface.CostOrb);

		Surface.Card->ConfigureDecorative(
			Surface.Background,
			Surface.Frame,
			Surface.Banner,
			Surface.CostOrb);
		return Surface;
	}

	FBattleHUDCardView MakeView(
		ECardColor Color,
		ECardType Type,
		ECardRarity Rarity,
		UTexture2D* Art,
		bool bUpgraded = false)
	{
		FBattleHUDCardView View;
		View.RuntimeId = 42;
		View.CardId = TEXT("CFVWidgetProbe");
		View.DisplayName = FText::FromString(TEXT("Probe"));
		View.bUpgraded = bUpgraded;
		View.Cost = 1;
		View.CardType = Type;
		View.Rarity = Rarity;
		View.CardColor = Color;
		View.Description = FText::FromString(TEXT("Probe description"));
		View.CardArt = Art;
		return View;
	}
}

using namespace CFVCardWidgetStyle;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCFVCardWidgetStyleTest,
	"SlayTheSpireDemo.CFV.WidgetStyle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCFVCardWidgetStyleTest::RunTest(const FString& Parameters)
{
	UCardFaceStyleSet* StyleSet = NewObject<UCardFaceStyleSet>(GetTransientPackage());
	if (!TestNotNull(TEXT("Transient StyleSet exists"), StyleSet))
	{
		return false;
	}

	UTexture2D* RedAttackBackground = MakeTexture();
	UTexture2D* RedSkillBackground = MakeTexture();
	UTexture2D* RedPowerBackground = MakeTexture();
	UTexture2D* RedOrb = MakeTexture();
	FCardColorVisualStyle RedStyle;
	RedStyle.AttackBackground = MakeRegion(RedAttackBackground, FVector2D(0.0, 1.0), FVector2D(302.0, 419.0));
	RedStyle.SkillBackground = MakeRegion(RedSkillBackground, FVector2D(0.0, 1.0), FVector2D(299.0, 419.0));
	RedStyle.PowerBackground = MakeRegion(RedPowerBackground, FVector2D(0.0, 1.0), FVector2D(299.0, 419.0));
	RedStyle.CostOrb = MakeRegion(RedOrb, FVector2D(-19.0, -17.0), FVector2D(72.0, 71.0));
	StyleSet->Config.ColorStyles.Add(ECardColor::Red, RedStyle);

	UTexture2D* GreenSkillBackground = MakeTexture();
	UTexture2D* GreenOrb = MakeTexture();
	FCardColorVisualStyle GreenStyle;
	GreenStyle.SkillBackground = MakeRegion(GreenSkillBackground, FVector2D(3.0, 4.0), FVector2D(290.0, 410.0));
	GreenStyle.CostOrb = MakeRegion(GreenOrb, FVector2D(-18.0, -16.0), FVector2D(70.0, 70.0));
	StyleSet->Config.ColorStyles.Add(ECardColor::Green, GreenStyle);

	UTexture2D* CommonAttackFrame = MakeTexture();
	UTexture2D* CommonSkillFrame = MakeTexture();
	UTexture2D* CommonPowerFrame = MakeTexture();
	UTexture2D* CommonBanner = MakeTexture();
	StyleSet->Config.CommonStyle.AttackFrame = MakeRegion(CommonAttackFrame, FVector2D(19.0, 62.0), FVector2D(262.0, 185.0));
	StyleSet->Config.CommonStyle.SkillFrame = MakeRegion(CommonSkillFrame, FVector2D(18.0, 61.0), FVector2D(263.0, 183.0));
	StyleSet->Config.CommonStyle.PowerFrame = MakeRegion(CommonPowerFrame, FVector2D(15.0, 6.0), FVector2D(269.0, 238.0));
	StyleSet->Config.CommonStyle.Banner = MakeRegion(CommonBanner, FVector2D(-12.0, 11.0), FVector2D(324.0, 77.0));

	UTexture2D* RareAttackFrame = MakeTexture();
	UTexture2D* RareBanner = MakeTexture();
	StyleSet->Config.RareStyle.AttackFrame = MakeRegion(RareAttackFrame, FVector2D(19.0, 62.0), FVector2D(262.0, 185.0));
	StyleSet->Config.RareStyle.Banner = MakeRegion(RareBanner, FVector2D(-12.0, 11.0), FVector2D(324.0, 77.0));

	FProbeSurface Probe = MakeProbe(true);
	if (!TestNotNull(TEXT("Widget style probe exists"), Probe.Card.Get()))
	{
		return false;
	}
	Probe.Card->SetStyleSetForTesting(StyleSet);

	UTexture2D* ArtA = MakeTexture();
	const FBattleHUDCardView RedRareAttack = MakeView(
		ECardColor::Red,
		ECardType::Attack,
		ECardRarity::Rare,
		ArtA);
	Probe.Card->SetCardView(RedRareAttack);

	TestTrue(TEXT("Red Rare Attack resolves Red attack background"), GetImageResource(Probe.Background) == RedAttackBackground);
	TestTrue(TEXT("Red Rare Attack resolves Rare attack frame"), GetImageResource(Probe.Frame) == RareAttackFrame);
	TestTrue(TEXT("Red Rare Attack resolves Rare banner"), GetImageResource(Probe.Banner) == RareBanner);
	TestTrue(TEXT("Red Rare Attack resolves Red orb"), GetImageResource(Probe.CostOrb) == RedOrb);
	TestTrue(TEXT("Frozen CardArt remains independent of StyleSet"), GetImageResource(Probe.Art) == ArtA);
	TestEqual(TEXT("Background is non-hit-testable when visible"), Probe.Background->GetVisibility(), ESlateVisibility::HitTestInvisible);

	UCanvasPanelSlot* AttackFrameSlot = GetCanvasSlot(Probe.Frame);
	if (TestNotNull(TEXT("Frame has a Canvas slot"), AttackFrameSlot))
	{
		TestEqual(TEXT("Attack frame position applied in 300x420 texture space"), AttackFrameSlot->GetPosition(), FVector2D(19.0, 62.0));
		TestEqual(TEXT("Attack frame size applied in 300x420 texture space"), AttackFrameSlot->GetSize(), FVector2D(262.0, 185.0));
	}

	UTexture2D* ArtB = MakeTexture();
	const FBattleHUDCardView GreenBasicSkill = MakeView(
		ECardColor::Green,
		ECardType::Skill,
		ECardRarity::Basic,
		ArtB);
	Probe.Card->SetCardView(GreenBasicSkill);
	TestTrue(TEXT("Same Widget switches to exact Green skill background"), GetImageResource(Probe.Background) == GreenSkillBackground);
	TestTrue(TEXT("Basic rarity collapses to Common skill frame"), GetImageResource(Probe.Frame) == CommonSkillFrame);
	TestTrue(TEXT("Basic rarity collapses to Common banner"), GetImageResource(Probe.Banner) == CommonBanner);
	TestTrue(TEXT("Same Widget updates CostOrb"), GetImageResource(Probe.CostOrb) == GreenOrb);
	TestTrue(TEXT("Same Widget updates frozen CardArt"), GetImageResource(Probe.Art) == ArtB);

	UCanvasPanelSlot* GreenBackgroundSlot = GetCanvasSlot(Probe.Background);
	if (TestNotNull(TEXT("Background has a Canvas slot"), GreenBackgroundSlot))
	{
		TestEqual(TEXT("New texture placement replaces old position"), GreenBackgroundSlot->GetPosition(), FVector2D(3.0, 4.0));
		TestEqual(TEXT("New texture placement replaces old size"), GreenBackgroundSlot->GetSize(), FVector2D(290.0, 410.0));
	}

	const FBattleHUDCardView MissingBlue = MakeView(
		ECardColor::Blue,
		ECardType::Skill,
		ECardRarity::Basic,
		ArtB);
	Probe.Card->SetCardView(MissingBlue);
	TestNull(TEXT("Unconfigured Blue clears old background resource"), GetImageResource(Probe.Background));
	TestNull(TEXT("Unconfigured Blue clears old orb resource"), GetImageResource(Probe.CostOrb));
	TestEqual(TEXT("Unconfigured Blue hides background"), Probe.Background->GetVisibility(), ESlateVisibility::Hidden);
	TestEqual(TEXT("Unconfigured Blue hides orb"), Probe.CostOrb->GetVisibility(), ESlateVisibility::Hidden);
	TestTrue(TEXT("Unconfigured Blue retains shared rarity frame"), GetImageResource(Probe.Frame) == CommonSkillFrame);
	TestTrue(TEXT("Unconfigured Blue retains shared rarity banner"), GetImageResource(Probe.Banner) == CommonBanner);
	if (UCanvasPanelSlot* ClearedBackgroundSlot = GetCanvasSlot(Probe.Background))
	{
		TestEqual(TEXT("Cleared background placement position has no stale value"), ClearedBackgroundSlot->GetPosition(), FVector2D::ZeroVector);
		TestEqual(TEXT("Cleared background placement size has no stale value"), ClearedBackgroundSlot->GetSize(), FVector2D::ZeroVector);
	}

	const FBattleHUDCardView RedPower = MakeView(
		ECardColor::Red,
		ECardType::Power,
		ECardRarity::Common,
		ArtA);
	Probe.Card->SetCardView(RedPower);
	TestTrue(TEXT("Same Widget restores Red power background after missing color"), GetImageResource(Probe.Background) == RedPowerBackground);
	TestTrue(TEXT("Same Widget restores Common power frame"), GetImageResource(Probe.Frame) == CommonPowerFrame);
	TestEqual(TEXT("Restored background returns to non-hit-testable visibility"), Probe.Background->GetVisibility(), ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* PowerFrameSlot = GetCanvasSlot(Probe.Frame))
	{
		TestEqual(TEXT("Power frame position replaces previous frame placement"), PowerFrameSlot->GetPosition(), FVector2D(15.0, 6.0));
		TestEqual(TEXT("Power frame size replaces previous frame placement"), PowerFrameSlot->GetSize(), FVector2D(269.0, 238.0));
	}

	FBattleHUDCardView UpgradedRedPower = RedPower;
	UpgradedRedPower.bUpgraded = true;
	Probe.Card->SetCardView(UpgradedRedPower);
	TestTrue(TEXT("Upgrade state does not change background selection"), GetImageResource(Probe.Background) == RedPowerBackground);
	TestTrue(TEXT("Upgrade state does not change frame selection"), GetImageResource(Probe.Frame) == CommonPowerFrame);
	TestTrue(TEXT("Upgrade presentation still appends plus"), Probe.Name->GetText().ToString() == TEXT("Probe+"));

	Probe.Card->SetStyleSetForTesting(nullptr);
	Probe.Card->SetCardView(RedRareAttack);
	TestTrue(TEXT("Null StyleSet keeps core name"), Probe.Name->GetText().ToString() == TEXT("Probe"));
	TestTrue(TEXT("Null StyleSet keeps core CardArt"), GetImageResource(Probe.Art) == ArtA);
	TestNull(TEXT("Null StyleSet clears background"), GetImageResource(Probe.Background));
	TestNull(TEXT("Null StyleSet clears frame"), GetImageResource(Probe.Frame));
	TestNull(TEXT("Null StyleSet clears banner"), GetImageResource(Probe.Banner));
	TestNull(TEXT("Null StyleSet clears orb"), GetImageResource(Probe.CostOrb));

	// SetCardView-before-style and explicit refresh converge to the same resolved state.
	Probe.Card->SetStyleSetForTesting(StyleSet);
	Probe.Card->RefreshForTesting();
	TestTrue(TEXT("Explicit post-DTO refresh restores background deterministically"), GetImageResource(Probe.Background) == RedAttackBackground);
	TestTrue(TEXT("Explicit post-DTO refresh restores frame deterministically"), GetImageResource(Probe.Frame) == RareAttackFrame);

	FProbeSurface CoreOnlyProbe = MakeProbe(false);
	CoreOnlyProbe.Card->SetStyleSetForTesting(StyleSet);
	CoreOnlyProbe.Card->SetCardView(RedRareAttack);
	TestTrue(TEXT("Missing decorative controls do not block core name"), CoreOnlyProbe.Name->GetText().ToString() == TEXT("Probe"));
	TestTrue(TEXT("Missing decorative controls do not block frozen CardArt"), GetImageResource(CoreOnlyProbe.Art) == ArtA);

	return true;
}

#endif