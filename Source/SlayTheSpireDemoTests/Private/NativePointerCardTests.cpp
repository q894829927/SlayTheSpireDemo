#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Phase6UIA2NR8TestTypes.h"
#include "NativeHandLayoutTestUtils.h"
#include "UI/BattleHUDViewModel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Phase6UIA1TestFixture.h"

SLAYTHESPIREDEMO_API void CacheG9TestWidgetGeometry(UWidget*, const FGeometry&);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointerCardConfirmTest, "SlayTheSpireDemo.HandInteraction.PointerCard.SelfAndNoneRequests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPointerCardConfirmTest::RunTest(const FString&)
{
	for (bool bQueue : {false, true}) for (ECardTargetType Type : {ECardTargetType::Self, ECardTargetType::None})
	{
		const int32 ExpectedBlock = Type == ECardTargetType::Self ? 8 : 0;
		Phase6UIA1Test::FHUDTestFixture F(Type, 1, 0, ExpectedBlock);
		F.Battle->DebugStartingDeck[0]->CardType = ECardType::Skill;
		F.Battle->bEnableCommittedPresentationRecording = false;
		F.Battle->StartBattle(); F.DrainInitialReady(); F.InitializeViewModel();
		UPhase6UIA2NR8HUDProbe* HUD = NewObject<UPhase6UIA2NR8HUDProbe>(F.World);
		HUD->SetTestWorld(F.World);
		HUD->ConfigureCardSurfaces(NewObject<UHorizontalBox>(HUD), NewObject<UOverlay>(HUD), NewObject<UTextBlock>(HUD), NewObject<UTextBlock>(HUD), NewObject<UTextBlock>(HUD));
		HUD->SetViewModel(F.ViewModel); HUD->SetBufferedPlayerInputEnabled(bQueue);
		TestTrue(TEXT("Select ordinary Skill"), HUD->SelectCard(F.FirstRuntimeId(), false));
		TestEqual(TEXT("Draft reserves no energy"), F.Battle->Energy, 3);
		TestTrue(TEXT("Pointer confirmation forwards legal Self or None through normal Request"), HUD->ConfirmPointerForTesting());
		TestEqual(TEXT("One accepted Request pays cost exactly once"), F.Battle->Energy, 2);
		TestEqual(TEXT("Existing Gameplay effect applies"), F.Player->Block, ExpectedBlock);
		TestEqual(TEXT("Existing Gameplay consumes one card"), F.Battle->GetDeckRuntimeForTesting()->GetHandCount(), 0);
		TestFalse(TEXT("Repeated pointer confirmation cannot pay twice"), HUD->ConfirmPointerForTesting());
		HUD->InvokeNativeDestructForTesting(); HUD->SetViewModel(nullptr);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointerHandPoseTest, "SlayTheSpireDemo.HandInteraction.PointerCard.IdentityCancelAndResize", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPointerHandPoseTest::RunTest(const FString&)
{
	UBattleHandFanPanel* Fan = NewObject<UBattleHandFanPanel>();
	UBattleCardWidget* Card = NewObject<UBattleCardWidget>(Fan);
	FBattleHUDCardView View; View.RuntimeId = 1; View.CardType = ECardType::Skill; View.TargetType = ECardTargetType::Self;
	Card->SetCardView(View); Fan->AddChild(Card); Fan->CommitFrozenOrder(); Fan->TakeWidget();
	UBattleCardWidget* Neighbor = NewObject<UBattleCardWidget>(Fan);
	FBattleHUDCardView NeighborView = View; NeighborView.RuntimeId = 2;
	Neighbor->SetCardView(NeighborView); Fan->AddChild(Neighbor); Fan->CommitFrozenOrder();
	UPanelSlot* Slot = Card->Slot; TSharedPtr<SWidget> Slate = Card->GetCachedWrappedWidget();
	const FGeometry Parent = FGeometry::MakeRoot(FVector2D(900,280), FSlateLayoutTransform(1.5f, FVector2D(60,100)));
	CacheG9TestWidgetGeometry(Fan, Parent);
	const auto Rest = NativeHandLayoutTest::Arrange(Fan, FVector2D(900,280));
	const FVector2D NeighborPointer = Parent.LocalToAbsolute(NativeHandLayoutTest::Position(*NativeHandLayoutTest::Find(Rest, Neighbor)) + FVector2D(75,100));
	Fan->UpdateHoverAffordance(NeighborPointer, INDEX_NONE, true, 1);
	TestEqual(TEXT("Resting neighbor may hover"), Fan->GetHoveredRuntimeId(), 2);
	Fan->SetInputVisualCards({1});
	const FVector2D Pointer(700,380);
	TestTrue(TEXT("Pointer moves an eligible formal card"), Fan->MoveInputVisualTo(1, Pointer));
	FGeometry Visual = Parent;
	TestTrue(TEXT("Source comes from current arrangement, before a card Tick"), Fan->GetCardVisualGeometry(Card, Visual));
	TestTrue(TEXT("Rendered center equals pointer under DPI"), FVector2D(Visual.LocalToAbsolute(Visual.GetLocalSize() * .5f)).Equals(Pointer, .01));
	Fan->UpdateHoverAffordance(NeighborPointer, 1, true, .001f);
	TestEqual(TEXT("Pointer draft preserves resting-strip hit routing"), Fan->GetHoveredRuntimeId(), 2);
	TestTrue(TEXT("Already raised neighbor returns immediately"), Neighbor->GetRenderTransform().Translation.IsNearlyZero() && Neighbor->GetRenderTransform().Scale.Equals(FVector2D(1)));
	TestEqual(TEXT("Neighbor paint layer returns to frozen order"), CastChecked<UBattleHandFanSlot>(Neighbor->Slot)->GetPaintLayer(), 0);
	Fan->GetCardVisualGeometry(Card, Visual);
	TestTrue(TEXT("Hover cannot overwrite a pointer pose"), FVector2D(Visual.LocalToAbsolute(Visual.GetLocalSize() * .5f)).Equals(Pointer, .01));
	CacheG9TestWidgetGeometry(Fan, FGeometry::MakeRoot(FVector2D(500,360), FSlateLayoutTransform(1.2f, FVector2D(120,30))));
	Fan->MoveInputVisualTo(1, Pointer); Fan->GetCardVisualGeometry(Card, Visual);
	TestTrue(TEXT("Viewport change rebases visual only"), FVector2D(Visual.LocalToAbsolute(Visual.GetLocalSize() * .5f)).Equals(Pointer, .01));
	TestTrue(TEXT("Pointer preserves slot and Slate identity"), Card->Slot == Slot && Card->GetCachedWrappedWidget() == Slate);
	Fan->SetInputVisualCards({});
	TestTrue(TEXT("Cancel returns immediately to fan, without Tick"), Card->GetRenderTransform().Translation.IsNearlyZero() && Card->GetRenderTransform().Scale.Equals(FVector2D(1)));
	TestEqual(TEXT("Cancel restores card button hit testing"), Card->GetVisibility(), ESlateVisibility::Visible);
	CacheG9TestWidgetGeometry(Fan, Parent);
	Fan->UpdateHoverAffordance(NeighborPointer, INDEX_NONE, true, 1);
	TestEqual(TEXT("Cancel restores neighbor hover"), Fan->GetHoveredRuntimeId(), 2);
	FBattleHUDCardView Attack = View; Attack.CardType = ECardType::Attack; Attack.TargetType = ECardTargetType::Enemy;
	Card->SetCardView(Attack);
	Fan->UpdateHoverAffordance(NeighborPointer, 1, true, .001f);
	TestEqual(TEXT("Attack aiming preserves resting-strip card switching"), Fan->GetHoveredRuntimeId(), 2);
	TestTrue(TEXT("Attack aiming immediately restores neighbors"), Neighbor->GetRenderTransform().Translation.IsNearlyZero() && Neighbor->GetRenderTransform().Scale.Equals(FVector2D(1)));
	TestTrue(TEXT("Selected attack keeps its own raised aim pose"), Card->GetRenderTransform().Translation.Equals(FVector2D(0,-72)) && Card->GetRenderTransform().Scale.Equals(FVector2D(1.35)));
	Fan->UpdateHoverAffordance(NeighborPointer, INDEX_NONE, true, 1);
	TestEqual(TEXT("Cancel attack aiming restores neighbor hover"), Fan->GetHoveredRuntimeId(), 2);
	Card->SetCardView(View);
	Fan->SetInputVisualCards({1});
	Fan->UpdateHoverAffordance(NeighborPointer, INDEX_NONE, true, 1);
	TestEqual(TEXT("A queued frozen pointer pose alone does not suppress new hover"), Fan->GetHoveredRuntimeId(), 2);
	Card->SetVisibility(ESlateVisibility::Hidden); Card->SetIsEnabled(false); Fan->SetInputVisualCards({1});
	TestFalse(TEXT("A non-Hand hidden owner cannot move"), Fan->MoveInputVisualTo(1, Pointer));
	Fan->SetInputVisualCards({}); TestEqual(TEXT("Cleanup cannot resurrect a hidden owner"), Card->GetVisibility(), ESlateVisibility::Hidden);
	View.CardType = ECardType::Power; TestTrue(TEXT("Power follows pointer"), UBattleHUDWidget::CardFollowsPointer(View));
	View.CardType = ECardType::Attack; View.TargetType = ECardTargetType::None; TestTrue(TEXT("Future untargeted AOE presentation uses same policy"), UBattleHUDWidget::CardFollowsPointer(View));
	View.TargetType = ECardTargetType::Enemy; TestFalse(TEXT("Targeted Attack retains arrow route"), UBattleHUDWidget::CardFollowsPointer(View));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointerCardPlayedSourceTest, "SlayTheSpireDemo.HandInteraction.PointerCard.FirstPoseAndToken", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPointerCardPlayedSourceTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	UPhase6UIA2NR8HUDProbe* HUD = NewObject<UPhase6UIA2NR8HUDProbe>(World);
	HUD->AddToRoot();
	UBattleHUDViewModel* VM = NewObject<UBattleHUDViewModel>(HUD); HUD->SetTestWorld(World); HUD->SetViewModelForTesting(VM);
	UOverlay* Play = NewObject<UOverlay>(HUD);
	HUD->ConfigureCardSurfaces(NewObject<UHorizontalBox>(HUD), Play, NewObject<UTextBlock>(HUD), NewObject<UTextBlock>(HUD), NewObject<UTextBlock>(HUD));
	UBattleHandFanPanel* Fan = NewObject<UBattleHandFanPanel>(HUD); HUD->ConfigureFanForTesting(Fan);
	const FGeometry Root = FGeometry::MakeRoot(FVector2D(1000,700), FSlateLayoutTransform(1.5f, FVector2D(80,60)));
	CacheG9TestWidgetGeometry(HUD->WidgetTree->RootWidget, Root);
	CacheG9TestWidgetGeometry(Play, Root.MakeChild(FVector2D(400,300), FSlateLayoutTransform(FVector2D(300,160))));
	CacheG9TestWidgetGeometry(Fan, Root.MakeChild(FVector2D(900,280), FSlateLayoutTransform(FVector2D(50,420))));
	VM->BattleId = 8; VM->Player.PresentationId = TEXT("Player"); VM->Energy = 3;
	FPresentationCardSnapshot Snapshot; Snapshot.RuntimeId = 9; Snapshot.CardId = TEXT("Skill"); Snapshot.DisplayName = FText::FromString(TEXT("Skill")); Snapshot.Cost = 1; Snapshot.CardType = ECardType::Skill; Snapshot.TargetType = ECardTargetType::None;
	UBattleCardWidget* Card = NewObject<UBattleCardWidget>(Fan); Card->SetCardView(HUD->MakePresentationCardView(Snapshot));
	VM->HandCards.Add(Card->GetCardView()); Fan->AddChild(Card); Fan->CommitFrozenOrder(); Fan->TakeWidget();
	VM->SelectedCardRuntimeId = 9; VM->InteractionState = EBattleHUDInteractionState::ReadyToConfirm; VM->bInputLocked = false;
	const FVector2D Pointer = Root.LocalToAbsolute(FVector2D(270,320)); HUD->PointerForTesting(Pointer);
	const auto Origin = HUD->OriginForTesting(9);
	TestTrue(TEXT("Capture exact selected pointer pose"), Origin.IsSet() && Origin->Center.Equals(FVector2D(.27,320.0/700.0), .0001));
	FPresentationRecord Record; Record.BattleId = 8; Record.ResolutionId = 10; Record.PresentationSequence = 1; Record.Type = EBattlePresentationRecordType::CardPlayed;
	Record.CardPlayed.Card = Snapshot; Record.CardPlayed.SourcePresentationId = TEXT("Player"); Record.CardPlayed.HandIndexBefore = 0; Record.CardPlayed.PlayAreaIndexAfter = 0; Record.CardPlayed.EnergyBefore = 3; Record.CardPlayed.EnergyAfter = 2; Record.CardPlayed.CostPaid = 1;
	FPresentationPlaybackToken Token; Token.BattleId = 8; Token.ResolutionId = 10; Token.PresentationSequence = 1; Token.LocalPlaybackGeneration = 1;
	const FGeometry HandAllocation = Fan->GetCachedGeometry();
	CacheG9TestWidgetGeometry(Fan, FGeometry::MakeRoot(FVector2D::ZeroVector, FSlateLayoutTransform()));
	AddExpectedError(TEXT("[BattleHUD][CardPlayedReject]"), EAutomationExpectedErrorFlags::Contains, 2);
	TestFalse(TEXT("Missing source declines rather than inventing an origin"), HUD->PlayPresentationRecord(Record, Token));
	TestEqual(TEXT("Failed preparation creates no PlayArea child"), Play->GetChildrenCount(), 0);
	TestTrue(TEXT("Failed preparation preserves visible historical source"), Card->IsVisible());
	TestEqual(TEXT("Failed preparation does not spend display energy"), VM->Energy, 3);
	CacheG9TestWidgetGeometry(Fan, HandAllocation);
	TestTrue(TEXT("Begin accepts current frozen source"), HUD->PlayPresentationRecord(Record, Token));
	UBattleCardWidget* Moving = HUD->PlayedCardForTesting();
	if (TestNotNull(TEXT("Moving card"), Moving))
	{
		TestTrue(TEXT("First pose installed before NativeTick"), Moving->GetRenderTransform().Translation.Equals(FVector2D(-230,10), .01));
		TestEqual(TEXT("First pose is opaque, with no flash/fade in"), Moving->GetRenderOpacity(), 1.0f);
		TestEqual(TEXT("Historical slot remains hidden"), Card->GetVisibility(), ESlateVisibility::Hidden);
		HUD->InvokeNativeTickForTesting(.1f); TestTrue(TEXT("Motion advances toward PlayArea"), Moving->GetRenderTransform().Translation.Size() < FVector2D(-230,10).Size());
		FPresentationPlaybackToken Old = Token; ++Old.LocalPlaybackGeneration; HUD->InvokeCancelForTesting(Old);
		TestTrue(TEXT("Old callback cannot cancel new motion"), HUD->IsLocalPresentationActive());
		HUD->InvokeFinishForTesting(Token); TestTrue(TEXT("Finish centers card"), Moving->GetRenderTransform().Translation.IsNearlyZero());
	}
	HUD->InvokeNativeDestructForTesting(); HUD->RemoveFromRoot(); World->DestroyWorld(false);
	return true;
}
#endif
