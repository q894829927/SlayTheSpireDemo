#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "UI/BattleHandFanPanel.h"
#include "UI/BattleTargetingArrowWidget.h"
#include "UI/BattleCardWidget.h"
#include "UI/BattleHUDWidget.h"
#include "NativeHandLayoutTestUtils.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandFanFormalSlotTest, "SlayTheSpireDemo.HandInteraction.FanSlots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHandFanFormalSlotTest::RunTest(const FString& Parameters)
{
	UBattleHandFanPanel* Panel = NewObject<UBattleHandFanPanel>();
	TArray<UBattleCardWidget*> Cards;
	for (int32 I = 0; I < 5; ++I)
	{
		UBattleCardWidget* Card = NewObject<UBattleCardWidget>(Panel);
		FBattleHUDCardView View;
		View.RuntimeId = 100 + I;
		Card->SetCardView(View);
		Panel->AddChild(Card);
		Cards.Add(Card);
	}
	Cards[2]->SetVisibility(ESlateVisibility::Hidden);
	Cards[2]->SetIsEnabled(false);
	Panel->CommitFrozenOrder();
	const auto Wide = NativeHandLayoutTest::Arrange(Panel, FVector2D(900, 280));
	const auto Narrow = NativeHandLayoutTest::Arrange(Panel, FVector2D(500, 280));
	TestEqual(TEXT("Hidden selected slot remains structural"), Panel->GetChildrenCount(), 5);
	for (int32 I = 0; I < 5; ++I)
	{
		TestTrue(TEXT("Fan preserves exact frozen index and object"), Panel->GetChildAt(I) == Cards[I]);
		const FGeometry* Geometry = NativeHandLayoutTest::Find(Wide, Cards[I]);
		const FGeometry* Opposite = NativeHandLayoutTest::Find(Wide, Cards[4-I]);
		if (!TestNotNull(TEXT("First Slate pass arranges the card"), Geometry) || !TestNotNull(TEXT("Opposite card"), Opposite)) return false;
		TestTrue(TEXT("First layout uses full configured size"), FVector2D(Geometry->GetLocalSize()) == FVector2D(150, 210));
		TestTrue(TEXT("Fan is bottom-centered and symmetric"), FMath::IsNearlyEqual(
			NativeHandLayoutTest::Position(*Geometry).X + NativeHandLayoutTest::Position(*Opposite).X, 750.0));
		TestEqual(TEXT("Frozen rank is explicit"), CastChecked<UBattleHandFanSlot>(Cards[I]->Slot)->GetFrozenIndex(), I);
	}
	TestTrue(TEXT("Allotted width changes arrangement without Tick"),
		NativeHandLayoutTest::Position(*NativeHandLayoutTest::Find(Wide, Cards[0])) != NativeHandLayoutTest::Position(*NativeHandLayoutTest::Find(Narrow, Cards[0])));
	TestEqual(TEXT("Layout cannot restore hidden ownership"), Cards[2]->GetVisibility(), ESlateVisibility::Hidden);
	Panel->RemoveChild(Cards[0]);
	Panel->CommitFrozenOrder();
	TestTrue(TEXT("Removal retains remaining instance"), Panel->GetChildAt(0) == Cards[1]);
	TestEqual(TEXT("One-card fan is upright"), UBattleHandFanPanel::GetFanAngle(0, 1), 0.0f);
	TestTrue(TEXT("Narrow fan fits available width"), FMath::Abs(UBattleHandFanPanel::GetFanOffset(9, 10, 500.0f).X) <= 145.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandFanGeometryProtectionTest, "SlayTheSpireDemo.HandInteraction.GeometryProtection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHandFanGeometryProtectionTest::RunTest(const FString& Parameters)
{
	using namespace NativeHandLayoutTest;
	UBattleHandFanPanel* Panel = NewObject<UBattleHandFanPanel>();
	TArray<UBattleCardWidget*> Cards;
	for (int32 I = 0; I < 3; ++I)
	{
		UBattleCardWidget* Card = NewObject<UBattleCardWidget>(Panel);
		FBattleHUDCardView View; View.RuntimeId = 100 + I; Card->SetCardView(View);
		Panel->AddChild(Card); Cards.Add(Card);
	}
	Panel->CommitFrozenOrder();
	FPresentationPlaybackToken Token;
	Token.BattleId = 1; Token.ResolutionId = 2; Token.PresentationSequence = 3; Token.LocalPlaybackGeneration = 4;
	TestTrue(TEXT("Moving card accepts exact geometry lease"), Panel->ProtectCardGeometry(Cards[0], Token));
	const auto Initial = Arrange(Panel, FVector2D(900, 280));
	const auto Resized = Arrange(Panel, FVector2D(500, 360));
	TestTrue(TEXT("Only moving card preserves frozen base position through resize"), Position(*Find(Initial, Cards[0])) == Position(*Find(Resized, Cards[0])));
	TestTrue(TEXT("Other cards follow new allotted size"), Position(*Find(Initial, Cards[1])) != Position(*Find(Resized, Cards[1])));
	FPresentationPlaybackToken Stale = Token; ++Stale.LocalPlaybackGeneration;
	TestFalse(TEXT("Stale token cannot release protected geometry"), Panel->ReleaseCardGeometry(Cards[0], Stale));
	TestTrue(TEXT("Exact token releases geometry"), Panel->ReleaseCardGeometry(Cards[0], Token));
	const auto Released = Arrange(Panel, FVector2D(500, 360));
	TestTrue(TEXT("Release immediately permits new arrangement without Tick"), Position(*Find(Released, Cards[0])) != Position(*Find(Initial, Cards[0])));
	UPanelSlot* SurvivorSlot = Cards[1]->Slot;
	TWeakPtr<SWidget> SurvivorSlate = Cards[1]->GetCachedWrappedWidget();
	Panel->ShiftChild(0, Cards[2]); Panel->CommitFrozenOrder();
	const auto Reordered = Arrange(Panel, FVector2D(900, 280));
	TestTrue(TEXT("Slate uses frozen rank after UMG reorder"), Position(*Find(Reordered, Cards[2])).X < Position(*Find(Reordered, Cards[0])).X);
	TestTrue(TEXT("Reorder retains slot and live Slate identity"), Cards[1]->Slot == SurvivorSlot && Cards[1]->GetCachedWrappedWidget() == SurvivorSlate.Pin());
	CastChecked<UBattleHandFanSlot>(Cards[0]->Slot)->SetPaintLayer(1);
	const auto Raised = Arrange(Panel, FVector2D(900, 280));
	TestTrue(TEXT("Raised card is drawn after the frozen-order peers"), Raised[Raised.Num()-1].Key == Cards[0]);
	const FVector2D BeforeHover = Position(*Find(Raised, Cards[0]));
	Panel->UpdateHoverAffordance(FVector2D::ZeroVector, INDEX_NONE, true, 1);
	TestTrue(TEXT("Hover never changes base geometry"), Position(*Find(Arrange(Panel, FVector2D(900, 280)), Cards[0])) == BeforeHover);
	TestTrue(TEXT("A new exact lease starts after release"), Panel->ProtectCardGeometry(Cards[0], Stale));
	TestFalse(TEXT("Old completion cannot release newer lease"), Panel->ReleaseCardGeometry(Cards[0], Token));
	Panel->RemoveChild(Cards[0]); Panel->CommitFrozenOrder();
	TestFalse(TEXT("Removed owner cannot affect a new slot"), Panel->ReleaseCardGeometry(Cards[0], Stale));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetArrowEligibilityTest, "SlayTheSpireDemo.HandInteraction.TargetArrowPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTargetArrowEligibilityTest::RunTest(const FString& Parameters)
{
	FBattleHUDCardView Card;
	Card.RuntimeId = 12;
	Card.CardType = ECardType::Attack;
	Card.TargetType = ECardTargetType::Enemy;
	auto Eligible = [&]() { return UBattleTargetingArrowWidget::ShouldShow(Card, EBattleHUDInteractionState::ChoosingTarget, false, false); };
	TestTrue(TEXT("Single-enemy Attack can aim"), Eligible());
	Card.CardType = ECardType::Skill;
	TestFalse(TEXT("Enemy-target skill has no attack arrow"), Eligible());
	Card.CardType = ECardType::Attack;
	Card.TargetType = ECardTargetType::None;
	TestFalse(TEXT("Untargeted/all-enemy attack has no arrow"), Eligible());
	Card.TargetType = ECardTargetType::Self;
	TestFalse(TEXT("Self targeting has no arrow"), Eligible());
	Card.TargetType = ECardTargetType::Enemy;
	TestFalse(TEXT("Idle hover cannot start aiming"), UBattleTargetingArrowWidget::ShouldShow(Card, EBattleHUDInteractionState::Idle, false, false));
	TestFalse(TEXT("Locked state removes aiming"), UBattleTargetingArrowWidget::ShouldShow(Card, EBattleHUDInteractionState::ChoosingTarget, true, false));
	TestFalse(TEXT("Consume selection cannot become attack aiming"), UBattleTargetingArrowWidget::ShouldShow(Card, EBattleHUDInteractionState::ChoosingTarget, false, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandFanProductionAssetTest, "SlayTheSpireDemo.HandInteraction.NativeAssetIntegration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHandFanProductionAssetTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) return false;
	World->SetGameInstance(NewObject<UGameInstance>(World));
	UClass* HUDClass = LoadClass<UBattleHUDWidget>(nullptr, TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C"));
	UBattleHUDWidget* HUD = HUDClass ? CreateWidget<UBattleHUDWidget>(World, HUDClass) : nullptr;
	if (TestNotNull(TEXT("Production Native HUD instantiates"), HUD))
	{
		TestNotNull(TEXT("Production Hand installs fan panel"), Cast<UBattleHandFanPanel>(HUD->GetWidgetFromName(TEXT("FanHand"))));
		int32 ArrowCount = 0;
		UBattleTargetingArrowWidget* Arrow = nullptr;
		HUD->WidgetTree->ForEachWidget([&](UWidget* Widget) { if (auto* Found = Cast<UBattleTargetingArrowWidget>(Widget)) { ++ArrowCount; Arrow = Found; } });
		TestEqual(TEXT("One private targeting surface"), ArrowCount, 1);
		if (Arrow)
		{
			UCanvasPanel* Canvas = Cast<UCanvasPanel>(Arrow->GetRootWidget());
			if (!TestNotNull(TEXT("Native arrow constructs without LocalPlayer"), Canvas)) { World->DestroyWorld(false); return false; }
			TestEqual(TEXT("Bounded private images"), Canvas->GetChildrenCount(), 21);
			Arrow->SetAim(FVector2D(500, 600), FVector2D(900, 250), false);
			UImage* Head = CastChecked<UImage>(Canvas->GetChildAt(20));
			TestEqual(TEXT("No target is gray"), Head->GetColorAndOpacity().R, Head->GetColorAndOpacity().G);
			Arrow->SetAim(FVector2D(500, 600), FVector2D(900, 250), true);
			TestTrue(TEXT("Legal target is red"), Head->GetColorAndOpacity().R > Head->GetColorAndOpacity().G * 5.0f);
			for (UWidget* Child : Canvas->GetAllChildren())
				TestTrue(TEXT("Arrow never steals target input"), Child->GetVisibility() == ESlateVisibility::HitTestInvisible || Child->GetVisibility() == ESlateVisibility::Collapsed);
		}
	}
	TestNotNull(TEXT("Imported arrow texture exists"), LoadObject<UTexture2D>(nullptr, TEXT("/Game/SlayTheSpireDemo/UI/Textures/Targeting/T_reticleArrow.T_reticleArrow")));
	TestNotNull(TEXT("Imported segment texture exists"), LoadObject<UTexture2D>(nullptr, TEXT("/Game/SlayTheSpireDemo/UI/Textures/Targeting/T_reticleBlock.T_reticleBlock")));
	World->DestroyWorld(false);
	return true;
}
#endif
