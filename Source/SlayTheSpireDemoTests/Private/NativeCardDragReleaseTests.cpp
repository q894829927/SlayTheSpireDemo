#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA2NR8TestTypes.h"
#include "NativeHandLayoutTestUtils.h"
#include "UI/BattleHUDViewModel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Presentation/BattlePresentationController.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"

SLAYTHESPIREDEMO_API void CacheG9TestWidgetGeometry(UWidget*, const FGeometry&);

namespace NativeCardDragTest
{
	struct FFixture
	{
		Phase6UIA1Test::FHUDTestFixture Gameplay;
		UPhase6UIA2NR8HUDProbe* HUD = nullptr;
		UBattleHandFanPanel* Fan = nullptr;
		UBattlePresentationController* Controller = nullptr;
		FGeometry Root = FGeometry::MakeRoot(FVector2D(1000,700), FSlateLayoutTransform(1.5f, FVector2D(80,40)));
		int32 RuntimeId = INDEX_NONE;
		explicit FFixture(ECardType Type, ECardTargetType Target, bool bQueue, bool bRecorded = false, bool bMandatoryA = false)
			: Gameplay(Target, 1, 0, Target == ECardTargetType::Self ? 8 : 0)
		{
			Gameplay.World->SetGameInstance(NewObject<UGameInstance>(Gameplay.World));
			Gameplay.Battle->DebugStartingDeck[0]->CardType = Type;
			Gameplay.Battle->bEnableCommittedPresentationRecording = bRecorded;
			if (bRecorded)
			{
				UCardData* A = Phase6UIA1Test::CreateCard(Gameplay.World, TEXT("A"), ECardTargetType::None, 0);
				if (bMandatoryA) A->Effects.Add(NewObject<USelectExhaustHandCardEffect>(A));
				Gameplay.Battle->DebugStartingDeck.Add(A);
				Gameplay.Battle->OpeningHandDrawCount = 2;
			}
			Gameplay.Battle->StartBattle(); Gameplay.DrainInitialReady(); Gameplay.InitializeViewModel();
			for (const auto& Card : Gameplay.ViewModel->HandCards) if (Card.CardId == TEXT("HUDCard")) RuntimeId = Card.RuntimeId;
			HUD = NewObject<UPhase6UIA2NR8HUDProbe>(Gameplay.World); HUD->AddToRoot(); HUD->SetTestWorld(Gameplay.World);
			UOverlay* Play = NewObject<UOverlay>(HUD);
			HUD->ConfigureCardSurfaces(NewObject<UHorizontalBox>(HUD), Play, NewObject<UTextBlock>(HUD), NewObject<UTextBlock>(HUD), NewObject<UTextBlock>(HUD));
			Fan = NewObject<UBattleHandFanPanel>(HUD); HUD->ConfigureFanForTesting(Fan); Fan->TakeWidget();
			HUD->SetViewModel(Gameplay.ViewModel);
			if (bRecorded)
			{
				Controller = NewObject<UBattlePresentationController>(Gameplay.World);
				HUD->SetPresentationController(Controller); Controller->Initialize(Gameplay.Battle, Gameplay.ViewModel, HUD);
			}
			HUD->SetBufferedPlayerInputEnabled(bQueue);
			CacheG9TestWidgetGeometry(HUD->WidgetTree->RootWidget, Root);
			CacheG9TestWidgetGeometry(Fan, Root.MakeChild(FVector2D(800,220), FSlateLayoutTransform(FVector2D(100,480))));
			CacheG9TestWidgetGeometry(Play, Root.MakeChild(FVector2D(400,300), FSlateLayoutTransform(FVector2D(300,100))));
		}
		~FFixture()
		{
			if (Controller) Controller->Shutdown();
			HUD->InvokeNativeDestructForTesting(); HUD->SetViewModel(nullptr); HUD->RemoveFromRoot();
		}
		FVector2D Point(double X, double Y) const { return Root.LocalToAbsolute(FVector2D(X,Y)); }
		bool SelectAndPress() { return HUD->SelectCard(RuntimeId, false) && HUD->BeginDragForTesting(RuntimeId, Point(500,620)); }
		int32 Selected() const { return HUD->IsBufferedPlayerInputEnabled() ? HUD->GetBufferedCardDraftRuntimeId() : Gameplay.ViewModel->SelectedCardRuntimeId; }
	};
}
using namespace NativeCardDragTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDragReleaseRegionsTest, "SlayTheSpireDemo.HandInteraction.CardDrag.ReleaseRegionsAndOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDragReleaseRegionsTest::RunTest(const FString&)
{
	for (bool bQueue : {false, true}) for (ECardType Type : {ECardType::Skill, ECardType::Power, ECardType::Attack})
	{
		const ECardTargetType Target = Type == ECardType::Skill ? ECardTargetType::Self : ECardTargetType::None;
		for (bool bOutside : {false, true})
		{
			FFixture F(Type, Target, bQueue);
			if (!TestTrue(TEXT("Normal selection starts the pointer-card gesture"), F.SelectAndPress())) return false;
			UPanelSlot* Slot = F.Fan->GetChildAt(0)->Slot;
			const FVector2D Drop = bOutside ? F.Point(500,330) : F.Point(600,600);
			TestTrue(TEXT("Held movement begins dragging under DPI"), F.HUD->MoveDragForTesting(Drop));
			TestTrue(TEXT("One matching release handled"), F.HUD->ReleaseDragForTesting(Drop));
			TestFalse(TEXT("Old/repeated release cannot submit"), F.HUD->ReleaseDragForTesting(Drop));
			if (!bOutside)
			{
				TestEqual(TEXT("Hand-region release does not debit energy"), F.Gameplay.Battle->Energy, 3);
				TestEqual(TEXT("Hand-region release keeps the same draft"), F.Selected(), F.RuntimeId);
				F.HUD->PointerForTesting(F.Point(500,330));
				TestEqual(TEXT("Moving after release cannot auto-play"), F.Gameplay.Battle->Energy, 3);
				TestTrue(TEXT("Formal slot survives held movement"), F.Fan->GetChildAt(0)->Slot == Slot);
				TestTrue(TEXT("A fresh click still confirms normally"), F.HUD->ConfirmPointerForTesting());
			}
			TestEqual(TEXT("Exactly one cost is paid"), F.Gameplay.Battle->Energy, 2);
			TestEqual(TEXT("Exactly one card resolves"), F.Gameplay.Battle->GetDeckRuntimeForTesting()->GetDiscardCount(), 1);
			TestEqual(TEXT("Normal Self effect applies once"), F.Gameplay.Player->Block, Target == ECardTargetType::Self ? 8 : 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDragReleaseSafetyTest, "SlayTheSpireDemo.HandInteraction.CardDrag.ShortClickAndRetirement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDragReleaseSafetyTest::RunTest(const FString&)
{
	for (int32 Mode = 0; Mode < 9; ++Mode)
	{
		FFixture F(ECardType::Skill, ECardTargetType::Self, true);
		if (!TestTrue(TEXT("Press accepted"), F.SelectAndPress())) return false;
		const FVector2D Outside = F.Point(500,330);
		if (Mode == 0)
		{
			TestFalse(TEXT("Short-click jitter is not dragging"), F.HUD->MoveDragForTesting(F.Point(502,620)));
			TestFalse(TEXT("A short click never auto-plays on release"), F.HUD->ReleaseDragForTesting(Outside));
		}
		else
		{
			F.HUD->MoveDragForTesting(Outside);
			if (Mode == 1) { F.HUD->CancelSelection(); F.HUD->SelectCard(F.RuntimeId, false); }
			else if (Mode == 2) F.HUD->SetViewModel(nullptr);
			else if (Mode == 3) F.HUD->SetBufferedPlayerInputEnabled(false);
			else if (Mode == 4) F.HUD->SetPresentationController(NewObject<UBattlePresentationController>(F.Gameplay.World));
			else if (Mode == 5) F.HUD->MoveDragForTesting(Outside, false);
			else if (Mode == 6) CacheG9TestWidgetGeometry(F.Fan, FGeometry::MakeRoot(FVector2D::ZeroVector, FSlateLayoutTransform()));
			else if (Mode == 7)
			{
				UButton* End = NewObject<UButton>(F.HUD); F.HUD->ConfigureDragControlsForTesting(End, nullptr, nullptr);
				CacheG9TestWidgetGeometry(End, F.Root.MakeChild(FVector2D(100,70), FSlateLayoutTransform(FVector2D(450,300))));
			}
			else
			{
				TestFalse(TEXT("Unrelated pointer cannot consume the release"), F.HUD->ReleaseDragForTesting(Outside, 1));
				F.HUD->CancelSelection();
			}
			F.HUD->ReleaseDragForTesting(Outside);
		}
		TestEqual(TEXT("Unsafe/stale release cannot spend energy"), F.Gameplay.Battle->Energy, 3);
		TestEqual(TEXT("Unsafe/stale release cannot move authoritative Hand"), F.Gameplay.Battle->GetDeckRuntimeForTesting()->GetHandCount(), 1);
	}
	FFixture Attack(ECardType::Attack, ECardTargetType::Enemy, true);
	TestTrue(TEXT("Single-target Attack still selects"), Attack.HUD->SelectCard(Attack.RuntimeId, false));
	TestFalse(TEXT("Single-target Attack never enters pointer dragging"), Attack.HUD->BeginDragForTesting(Attack.RuntimeId, Attack.Point(500,620)));
	TestFalse(TEXT("Attack movement is still aiming only"), Attack.HUD->MoveDragForTesting(Attack.Point(500,330)));
	TestFalse(TEXT("Attack release does not play or retarget"), Attack.HUD->ReleaseDragForTesting(Attack.Point(500,330)));
	TestEqual(TEXT("Attack remains selected for a normal target click"), Attack.Selected(), Attack.RuntimeId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDragReleaseBusyQueueTest, "SlayTheSpireDemo.HandInteraction.CardDrag.BusyQueueAndReentry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDragReleaseBusyQueueTest::RunTest(const FString&)
{
	FFixture F(ECardType::Skill, ECardTargetType::Self, true, true);
	UCardInstance* A = nullptr;
	for (const auto& Card : F.Gameplay.Battle->GetDeckRuntimeForTesting()->GetHandCards()) if (Card->GetCardId() == TEXT("A")) A = Card;
	if (!TestNotNull(TEXT("A exists"), A) || !TestTrue(TEXT("A accepted"), F.Gameplay.Battle->RequestPlayCard(A, nullptr).IsAcceptedForResolution())) return false;
	F.Gameplay.FlushReady();
	if (!TestTrue(TEXT("A still owns Blocking playback"), F.Controller->IsWaitingForCompletionForTesting())) return false;
	if (!TestTrue(TEXT("B pointer gesture accepted while A plays"), F.SelectAndPress())) return false;
	const auto Token = F.Controller->GetActivePlaybackTokenForTesting();
	const FVector2D Outside = F.Point(500,330);
	F.HUD->MoveDragForTesting(Outside);
	int32 ReentryChecks = 0;
	const auto Observer = F.Gameplay.ViewModel->OnNativeChanged.AddLambda([&](EBattleHUDDirtyFlags)
	{ ++ReentryChecks; TestFalse(TEXT("Publication cannot replay a consumed release"), F.HUD->ReleaseDragForTesting(Outside)); });
	TestTrue(TEXT("Release confirms a complete queued B"), F.HUD->ReleaseDragForTesting(Outside));
	TestEqual(TEXT("One B intent, no early gameplay debit"), F.HUD->GetBufferedPlayerInputShadowForTesting().GetConfirmedPlayCount(), 1);
	TestEqual(TEXT("No early B cost"), F.Gameplay.Battle->Energy, 3);
	TestTrue(TEXT("Release does not Skip A"), F.Controller->GetActivePlaybackTokenForTesting() == Token);
	for (int32 Step = 0; Step < 8; ++Step)
	{
		F.Gameplay.FlushReady();
		for (int32 Record = 0; Record < 32 && F.Controller->IsWaitingForCompletionForTesting(); ++Record)
			F.Controller->NotifyPresentationFinished(F.Controller->GetActivePlaybackTokenForTesting());
		FTSTicker::GetCoreTicker().Tick(0.0f);
	}
	F.Gameplay.ViewModel->OnNativeChanged.Remove(Observer);
	TestTrue(TEXT("Synchronous publication exercised"), ReentryChecks > 0);
	TestEqual(TEXT("B pays exactly once after A's history"), F.Gameplay.Battle->Energy, 2);
	TestEqual(TEXT("B effect occurs once"), F.Gameplay.Player->Block, 8);
	TestEqual(TEXT("Exactly A and B resolve"), F.Gameplay.Battle->GetDeckRuntimeForTesting()->GetDiscardCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDragForcedChoiceTest, "SlayTheSpireDemo.HandInteraction.CardDrag.ForcedChoiceRetiresGesture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDragForcedChoiceTest::RunTest(const FString&)
{
	FFixture F(ECardType::Skill, ECardTargetType::Self, true, true, true);
	if (!TestTrue(TEXT("Ordinary B press starts"), F.SelectAndPress())) return false;
	const FVector2D Outside = F.Point(500,330);
	F.HUD->MoveDragForTesting(Outside);
	UCardInstance* A = nullptr;
	for (const auto& Card : F.Gameplay.Battle->GetDeckRuntimeForTesting()->GetHandCards()) if (Card->GetCardId() == TEXT("A")) A = Card;
	if (!TestNotNull(TEXT("A exists"), A) || !TestTrue(TEXT("A reaches mandatory selection"), F.Gameplay.Battle->RequestPlayCard(A, nullptr).IsAcceptedForResolution())) return false;
	F.Gameplay.FlushReady();
	TestTrue(TEXT("Gameplay owns a mandatory choice before visible completion"), F.Gameplay.ViewModel->HasAuthoritativePendingCardSelection());
	F.HUD->ReleaseDragForTesting(Outside);
	TestEqual(TEXT("Old release cannot play B"), F.Gameplay.Battle->Energy, 3);
	TestEqual(TEXT("Old release cannot enqueue B"), F.HUD->GetBufferedPlayerInputShadowForTesting().GetConfirmedPlayCount(), 0);
	TestFalse(TEXT("Forced choice cannot start an ordinary drag"), F.HUD->BeginDragForTesting(F.RuntimeId, F.Point(500,620)));
	return true;
}
#endif
