#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "UI/BattleHandFanPanel.h"
#include "UI/BattleTargetingArrowWidget.h"
#include "UI/BattleCardWidget.h"
#include "UI/BattleHUDWidget.h"
#include "Components/CanvasPanelSlot.h"
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
	Panel->LayoutCards();
	TestEqual(TEXT("Hidden selected slot remains structural"), Panel->GetChildrenCount(), 5);
	for (int32 I = 0; I < 5; ++I)
	{
		TestTrue(TEXT("Fan preserves exact frozen index and object"), Panel->GetChildAt(I) == Cards[I]);
		const auto* CanvasSlot = CastChecked<UCanvasPanelSlot>(Cards[I]->Slot);
		const FVector2D Opposite = CastChecked<UCanvasPanelSlot>(Cards[4 - I]->Slot)->GetPosition();
		TestTrue(TEXT("Fan is symmetric"), FMath::IsNearlyEqual(CanvasSlot->GetPosition().X, -Opposite.X));
	}
	TestEqual(TEXT("Layout cannot restore hidden ownership"), Cards[2]->GetVisibility(), ESlateVisibility::Hidden);
	Panel->RemoveChild(Cards[0]);
	TestTrue(TEXT("Removal retains remaining instance"), Panel->GetChildAt(0) == Cards[1]);
	TestEqual(TEXT("One-card fan is upright"), UBattleHandFanPanel::GetFanAngle(0, 1), 0.0f);
	TestTrue(TEXT("Narrow fan fits available width"), FMath::Abs(UBattleHandFanPanel::GetFanOffset(9, 10, 500.0f).X) <= 145.0f);
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
