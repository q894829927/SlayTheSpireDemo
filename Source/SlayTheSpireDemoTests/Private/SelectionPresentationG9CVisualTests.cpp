#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA2NR8TestTypes.h"
#include "NativeHandLayoutTestUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "UObject/GarbageCollection.h"
#include "Presentation/BattlePresentationController.h"
SLAYTHESPIREDEMO_API void CacheG9TestWidgetGeometry(UWidget*, const FGeometry&);

namespace G9CVisualTest
{
	struct FFixture
	{
		Phase6UIA1Test::FHUDTestFixture Game{ECardTargetType::None,1};
		UPhase6UIA2NR8HUDProbe* HUD = nullptr;
		UBattleHandFanPanel* Fan = nullptr;
		UBattlePresentationController* Controller = nullptr;
		FFixture()
		{
			Game.World->AddToRoot(); Game.World->SetGameInstance(NewObject<UGameInstance>(Game.World));
			Game.Battle->bEnableCommittedPresentationRecording = true;
			Game.Battle->StartBattle(); Game.DrainInitialReady(); Game.InitializeViewModel();
			HUD = NewObject<UPhase6UIA2NR8HUDProbe>(Game.World); HUD->AddToRoot(); HUD->SetTestWorld(Game.World);
			UOverlay* Play = NewObject<UOverlay>(HUD);
			HUD->ConfigureCardSurfaces(NewObject<UHorizontalBox>(HUD),Play,NewObject<UTextBlock>(HUD),NewObject<UTextBlock>(HUD),NewObject<UTextBlock>(HUD));
			Fan = NewObject<UBattleHandFanPanel>(HUD); HUD->ConfigureFanForTesting(Fan); Fan->TakeWidget();
			HUD->SetViewModel(Game.ViewModel);
			const auto Root = FGeometry::MakeRoot(FVector2D(1000,700),FSlateLayoutTransform());
			CacheG9TestWidgetGeometry(HUD->WidgetTree->RootWidget,Root);
			CacheG9TestWidgetGeometry(Fan,Root.MakeChild(FVector2D(800,220),FSlateLayoutTransform(FVector2D(100,480))));
			CacheG9TestWidgetGeometry(Play,Root.MakeChild(FVector2D(400,300),FSlateLayoutTransform(FVector2D(300,100))));
			Controller = NewObject<UBattlePresentationController>(Game.World);
			HUD->SetPresentationController(Controller); Controller->Initialize(Game.Battle,Game.ViewModel,HUD);
		}
		~FFixture()
		{
			Controller->Shutdown(); HUD->InvokeNativeDestructForTesting(); HUD->SetViewModel(nullptr); HUD->RemoveFromRoot();
			Game.World->RemoveFromRoot();
		}
		bool Play()
		{
			auto* Card = Game.Battle->GetDeckRuntimeForTesting()->GetHandCards()[0].Get();
			const bool Accepted = Game.Battle->RequestPlayCard(Card,nullptr).IsAcceptedForResolution(); Game.FlushReady(); return Accepted;
		}
		void Finish() { HUD->InvokeFinishForTesting(Controller->GetActivePlaybackTokenForTesting()); FTSTicker::GetCoreTicker().Tick(0); }
	};
}
using namespace G9CVisualTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9CHostedBlockingTest,"SlayTheSpireDemo.SelectionPresentation.G9C.Visual.HostedBlockingAndGC",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9CHostedBlockingTest::RunTest(const FString&)
{
	FFixture F; if (!TestTrue(TEXT("Normal play accepted"),F.Play())) return false;
	if (!TestEqual(TEXT("Production Controller uses one hosted job"),F.HUD->HostedPlayedCountForTesting(),1)) return false;
	auto* Card = F.HUD->PlayedCardForTesting();
	TestTrue(TEXT("Host is an independent Canvas"),Card && Cast<UCanvasPanel>(Card->GetParent()));
	TestFalse(TEXT("Private clone is never interactive"),Card->GetIsEnabled());
	F.HUD->HideHostedForSelectionForTesting(true);
	TestTrue(TEXT("Selection temporarily hides the retained visual"),Card->GetVisibility()==ESlateVisibility::Hidden);
	F.HUD->HideHostedForSelectionForTesting(false);
	TestTrue(TEXT("Selection restores cosmetic visibility without input"),Card->GetVisibility()==ESlateVisibility::HitTestInvisible && !Card->GetIsEnabled());
	const auto Visual = F.HUD->HostedPlayedTokenForTesting(); const auto Arrival = F.Controller->GetActivePlaybackTokenForTesting();
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("UPROPERTY job retains clone across GC"),IsValid(Card));
	TestTrue(TEXT("Controller still waits for arrival"),F.Controller->IsWaitingForCompletionForTesting());
	F.Finish();
	TestEqual(TEXT("Arrival hands the same job to destination"),F.HUD->HostedPlayedCountForTesting(),1);
	TestTrue(TEXT("Destination retains occurrence and visual generation"),F.HUD->HostedPlayedTokenForTesting()==Visual);
	TestFalse(TEXT("Old arrival completion cannot complete destination"),F.HUD->CompleteHostedForTesting(Visual,Arrival));
	F.Finish();
	TestEqual(TEXT("Destination cleans job"),F.HUD->HostedPlayedCountForTesting(),0);
	TestEqual(TEXT("Gameplay cost remains once"),F.Game.Battle->Energy,2);
	TestFalse(TEXT("Blocking controller completes normally"),F.Controller->IsWaitingForCompletionForTesting());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9CPreparedCapacityTest,"SlayTheSpireDemo.SelectionPresentation.G9C.Visual.PreparedCapacityAndIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9CPreparedCapacityTest::RunTest(const FString&)
{
	FFixture F; FPresentationSessionToken Session;
	if (!TestTrue(TEXT("Real bound session"),F.Controller->TryGetPresentationSessionToken(Session))) return false;
	FCardPlayVisualOrigin Origin; Origin.Center=FVector2D(.5,.8); Origin.Size=FVector2D(150,210);
	const int32 OriginalHand=F.Game.ViewModel->HandCards.Num(); const int32 OriginalEnergy=F.Game.Battle->Energy;
	FDetachedCardVisualToken First; FPresentationPlaybackToken FirstBlocking;
	for (int32 I=0;I<33;++I)
	{
		FPresentationRecord Record; Record.BattleId=Session.BattleId; Record.ResolutionId=100; Record.PresentationSequence=I+1;
		Record.Type=EBattlePresentationRecordType::CardPlayed;
		Record.CardPlayed.Card.RuntimeId=1000+I; Record.CardPlayed.Card.CardId=TEXT("Prepared");
		Record.CardPlayed.Card.DisplayName=FText::FromString(TEXT("Prepared"));
		FPlayedCardPresentationLifecycleToken Life; Life.SessionToken=Session; Life.BattleId=Session.BattleId;
		Life.SourceResolutionId=100; Life.CardPlayedPresentationSequence=I+1; Life.LocalLifecycleGeneration=I+1;
		Life.RuntimeId=1000+I; Life.CardId=TEXT("Prepared");
		FPresentationPlaybackToken Blocking; Blocking.BattleId=Record.BattleId; Blocking.ResolutionId=100;
		Blocking.PresentationSequence=I+1; Blocking.LocalPlaybackGeneration=1;
		FDetachedCardVisualToken Visual;
		TestEqual(TEXT("Preparation is bounded at 32"),F.HUD->PrepareHostedForTesting(Record,Blocking,Life,Origin,Visual),I<32);
		if(I==0){First=Visual;FirstBlocking=Blocking;}
	}
	TestEqual(TEXT("Prepared candidates never alter Hand"),F.Game.ViewModel->HandCards.Num(),OriginalHand);
	TestEqual(TEXT("Prepared candidates never debit Gameplay"),F.Game.Battle->Energy,OriginalEnergy);
	TestFalse(TEXT("Prepared job cannot complete Blocking chronology"),F.HUD->CompleteHostedForTesting(First,FirstBlocking));
	CollectGarbage(RF_NoFlags);
	TestEqual(TEXT("All candidates remain GC-reachable"),F.HUD->HostedPlayedCountForTesting(),32);
	// Full private capacity follows the sealed Native Blocking decline path.
	TestTrue(TEXT("Play still accepted with full private capacity"),F.Play());
	TestEqual(TEXT("Capacity decline leaves candidates untouched"),F.HUD->HostedPlayedCountForTesting(),32);
	TestTrue(TEXT("Fallback still presents a card"),F.HUD->PlayedCardForTesting()!=nullptr);
	F.Controller->SkipPresentation();
	F.HUD->CancelPlayedCardVisualsForSession(Session);
	TestEqual(TEXT("Exact session cleanup removes all candidates"),F.HUD->HostedPlayedCountForTesting(),0);
	TestFalse(TEXT("Old visual completion is inert"),F.HUD->CompleteHostedForTesting(First,FirstBlocking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9CVisualLossTest,"SlayTheSpireDemo.SelectionPresentation.G9C.Visual.LossKeepsHistoryAndSkipCleans",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9CVisualLossTest::RunTest(const FString&)
{
	FFixture F; if (!TestTrue(TEXT("Play accepted"),F.Play())) return false;
	const auto OldVisual=F.HUD->HostedPlayedTokenForTesting(); const auto OldBlocking=F.Controller->GetActivePlaybackTokenForTesting();
	F.Finish();
	const auto Destination=F.Controller->GetActivePlaybackTokenForTesting();
	F.HUD->CancelAllPlayedCardVisuals();
	TestEqual(TEXT("Visual loss cleans only visuals"),F.HUD->HostedPlayedCountForTesting(),0);
	TestTrue(TEXT("Chronology still waits for exact destination"),F.Controller->IsWaitingForCompletionForTesting());
	F.Finish();
	TestEqual(TEXT("Formal destination still commits after visual loss"),F.Game.ViewModel->DiscardCount,1);
	TestFalse(TEXT("Lost job callback cannot return"),F.HUD->CompleteHostedForTesting(OldVisual,OldBlocking));
	TestFalse(TEXT("Destination differs from old arrival"),Destination==OldBlocking);
	TestTrue(TEXT("Presentation failure never faults Gameplay"),F.Game.Battle->BattleState==EBattleState::PlayerTurn);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9CVisualOwnerTest,"SlayTheSpireDemo.SelectionPresentation.G9C.Visual.FormalOwnerAndSessionIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9CVisualOwnerTest::RunTest(const FString&)
{
	FFixture F; FPresentationSessionToken Session;
	if (!TestTrue(TEXT("Real bound session"),F.Controller->TryGetPresentationSessionToken(Session))) return false;
	const auto& Hand = F.Game.ViewModel->HandCards[0];
	FPresentationRecord Record; Record.Type=EBattlePresentationRecordType::CardPlayed;
	Record.BattleId=Session.BattleId; Record.ResolutionId=100; Record.PresentationSequence=1;
	Record.CardPlayed.Card.RuntimeId=Hand.RuntimeId; Record.CardPlayed.Card.CardId=Hand.CardId;
	Record.CardPlayed.Card.DisplayName=Hand.DisplayName;
	FPlayedCardPresentationLifecycleToken Life; Life.SessionToken=Session; Life.BattleId=Session.BattleId;
	Life.SourceResolutionId=100; Life.CardPlayedPresentationSequence=1; Life.LocalLifecycleGeneration=1;
	Life.RuntimeId=Hand.RuntimeId; Life.CardId=Hand.CardId;
	FPresentationPlaybackToken Blocking; Blocking.BattleId=Session.BattleId; Blocking.ResolutionId=100;
	Blocking.PresentationSequence=1; Blocking.LocalPlaybackGeneration=1;
	FCardPlayVisualOrigin Origin; Origin.Center=FVector2D(.5,.8); Origin.Size=FVector2D(150,210);
	FDetachedCardVisualToken Visual;
	if (!TestTrue(TEXT("Private candidate prepared"),F.HUD->PrepareHostedForTesting(Record,Blocking,Life,Origin,Visual))) return false;
	auto* Fan = F.Fan;
	if (!TestNotNull(TEXT("Formal fan"),Fan)) return false;
	auto* Formal = Cast<UBattleCardWidget>(Fan->GetChildAt(0));
	if (!TestNotNull(TEXT("Formal card"),Formal)) return false;
	Formal->SetIsEnabled(false);
	F.HUD->RetireHostedCollisionsForTesting();
	TestEqual(TEXT("Visible formal Hand owner retires old visual even while input disabled"),F.HUD->HostedPlayedCountForTesting(),0);
	TestFalse(TEXT("Old callback cannot affect formal card"),F.HUD->CompleteHostedForTesting(Visual,Blocking));
	TestTrue(TEXT("Formal Widget identity survives"),Fan->GetChildAt(0)==Formal);
	Record.CardPlayed.Card.RuntimeId=999; Life.RuntimeId=999; Life.LocalLifecycleGeneration=2;
	if (!TestTrue(TEXT("Unrelated visual candidate"),F.HUD->PrepareHostedForTesting(Record,Blocking,Life,Origin,Visual))) return false;
	F.HUD->SetViewModel(nullptr);
	TestEqual(TEXT("ViewModel unbind cleans private jobs without Tick"),F.HUD->HostedPlayedCountForTesting(),0);
	TestFalse(TEXT("Unbound HUD cannot prepare a visual"),F.HUD->PrepareHostedForTesting(Record,Blocking,Life,Origin,Visual));
	F.HUD->SetViewModel(F.Game.ViewModel);
	if (!TestTrue(TEXT("Candidate for session replacement"),F.HUD->PrepareHostedForTesting(Record,Blocking,Life,Origin,Visual))) return false;
	F.Controller->Shutdown();
	TestEqual(TEXT("Session replacement retires every old job"),F.HUD->HostedPlayedCountForTesting(),0);
	TestFalse(TEXT("Old session cannot prepare new visuals"),F.HUD->PrepareHostedForTesting(Record,Blocking,Life,Origin,Visual));
	return true;
}
#endif
