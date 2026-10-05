#include "SelectionPresentationG9TestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Phase6UIA2NR8TestTypes.h"
#include "UI/BattleHandFanPanel.h"
#include "NativeHandLayoutTestUtils.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Containers/Ticker.h"

using namespace SelectionPresentationG9ATest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BCardReplayTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.CardReplayWithoutSkip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BCardReplayTest::RunTest(const FString&)
{
	FShadowFixture F;
	F.Widget->SetBufferedPlayerInputEnabled(true);
	if (!TestTrue(TEXT("A playback starts"), F.PlayA())) return false;
	const int32 B = F.Card(TEXT("B"))->GetRuntimeId();
	const int32 C = F.Card(TEXT("C"))->GetRuntimeId();
	TestTrue(TEXT("B captures an already sealed target"), F.Widget->TryBufferCardSelection(B));
	TestTrue(TEXT("B fully confirmed during playback"), F.Widget->ConfirmSelectedCard());
	TestFalse(TEXT("Duplicate B cannot be enqueued"), F.Widget->TryBufferCardSelection(B));
	TestTrue(TEXT("New physical click captures C"), F.Widget->TryBufferCardSelection(C));
	TestTrue(TEXT("C fully confirmed during playback"), F.Widget->ConfirmSelectedCard());
	TestTrue(TEXT("Buffer does not Skip A"), F.Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("No early selection"), F.Gameplay.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	F.FinishPlayback();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestTrue(TEXT("B executes automatically"), F.Card(TEXT("B")) == nullptr);
	TestTrue(TEXT("C waits for B history"), F.Card(TEXT("C")) != nullptr);
	F.Gameplay.FlushReady();
	F.FinishPlayback();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestTrue(TEXT("C executes automatically after B history"), F.Card(TEXT("C")) == nullptr);
	F.Widget->NotifyBufferedPlayerInputReadinessChanged();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Repeated event cannot submit twice"), F.Gameplay.Battle->GetDeckRuntimeForTesting()->GetDiscardCount(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BEndTurnPresentationTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.EndTurnRetiresCardWithoutSkip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BEndTurnPresentationTest::RunTest(const FString&)
{
	FShadowFixture F;
	F.Widget->SetBufferedPlayerInputEnabled(true);
	if (!TestTrue(TEXT("A playback starts"), F.PlayA())) return false;
	TestTrue(TEXT("Older B buffered"), F.Widget->TryBufferCardSelection(F.Card(TEXT("B"))->GetRuntimeId()));
	const FPresentationPlaybackToken Token = F.Controller->GetActivePlaybackTokenForTesting();
	FPlayerTurnAuthorityToken Before;
	F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Before);
	TestTrue(TEXT("EndTurn control accepts through Blocking playback"), F.Widget->CanAcceptEndTurnIntent());
	TestTrue(TEXT("Physical EndTurn accepted"), F.Widget->EndTurn());
	TestTrue(TEXT("EndTurn preserves active visual token"), F.Controller->GetActivePlaybackTokenForTesting() == Token);
	F.Gameplay.FlushReady();
	F.FinishPlayback();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	FPlayerTurnAuthorityToken After;
	TestTrue(TEXT("Next player turn reached"), F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(After));
	TestEqual(TEXT("Exactly one turn transition"), After.PlayerTurnSerial, Before.PlayerTurnSerial + 1);
	TestEqual(TEXT("Older buffered card never selects"), F.Gameplay.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BEndTurnBusyTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.EndTurnBusyAndDirectBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BEndTurnBusyTest::RunTest(const FString&)
{
	for (bool bRecorded : {false, true})
	{
		FShadowFixture F(bRecorded);
		F.Widget->SetBufferedPlayerInputEnabled(true);
		UPhase6UIA0ManualFinishAction* Hold = F.HoldQueue();
		FPlayerTurnAuthorityToken Before;
		F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Before);
		TestTrue(TEXT("Busy same-turn intent available"), F.Widget->CanAcceptEndTurnIntent());
		TestTrue(TEXT("Busy EndTurn accepted"), F.Widget->EndTurn());
		TestTrue(TEXT("Exactly one EndTurn stored"), F.Widget->HasAcceptedBufferedEndTurn());
		TestFalse(TEXT("Second physical EndTurn rejected"), F.Widget->EndTurn());
		TestFalse(TEXT("No speculative card buffer during Gameplay busy"), F.Widget->TryBufferCardSelection(F.Card(TEXT("B"))->GetRuntimeId()));
		Hold->CompleteManually();
		F.Gameplay.FlushReady();
		FTSTicker::GetCoreTicker().Tick(0.0f);
		F.Gameplay.FlushReady();
		F.FinishPlayback();
		FTSTicker::GetCoreTicker().Tick(0.0f); // revised EndTurn waits for Blocking history
		FPlayerTurnAuthorityToken After;
		TestTrue(TEXT("Same turn command reached next turn"), F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(After));
		TestEqual(TEXT("Buffered EndTurn executes once"), After.PlayerTurnSerial, Before.PlayerTurnSerial + 1);
		TestFalse(TEXT("Consumed EndTurn storage is empty"), F.Widget->HasAcceptedBufferedEndTurn());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BTransientEndTurnTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.TransientReplacementAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BTransientEndTurnTest::RunTest(const FString&)
{
	for (ECardTargetType Target : {ECardTargetType::None, ECardTargetType::Enemy})
	{
		FShadowFixture F(false, false, Target);
		const int32 B = F.Card(TEXT("B"))->GetRuntimeId();
		TestTrue(TEXT("Normal transient selection"), F.Gameplay.ViewModel->SelectCardByRuntimeId(B));
		if (Target == ECardTargetType::Enemy)
		{
			TestFalse(TEXT("Disabled G9 preserves G8 rejection"), F.Widget->EndTurn());
			TestEqual(TEXT("Rejected EndTurn preserves selected card"), F.Gameplay.ViewModel->SelectedCardRuntimeId, B);
		}
		F.Widget->SetBufferedPlayerInputEnabled(true);
		const EBattleHUDInteractionState SelectedState = F.Gameplay.ViewModel->InteractionState;
		F.Gameplay.ViewModel->InteractionState = EBattleHUDInteractionState::PresentationUnavailable;
		TestFalse(TEXT("Unsafe authority rejects before transient cancellation"), F.Widget->EndTurn());
		TestEqual(TEXT("Rejected G9 EndTurn preserves selection"), F.Gameplay.ViewModel->SelectedCardRuntimeId, B);
		F.Gameplay.ViewModel->InteractionState = SelectedState;
		TestTrue(TEXT("G9 accepts transient replacement"), F.Widget->EndTurn());
		TestEqual(TEXT("Accepted EndTurn clears transient selection"), F.Gameplay.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
		TestTrue(TEXT("Legal targets retired"), F.Gameplay.ViewModel->LegalTargets.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BStaleAndMandatoryTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.StaleMandatoryAndDisable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BStaleAndMandatoryTest::RunTest(const FString&)
{
	for (int32 Case = 0; Case < 4; ++Case)
	{
		FShadowFixture F(false, true);
		F.Widget->SetBufferedPlayerInputEnabled(true);
		UPhase6UIA0ManualFinishAction* Hold = F.HoldQueue();
		TestTrue(TEXT("Busy EndTurn accepted"), F.Widget->EndTurn());
		if (Case == 0) F.Widget->SetBufferedPlayerInputEnabled(false);
		if (Case == 1) F.Gameplay.Battle->StartBattle();
		if (Case != 1) Hold->CompleteManually();
		if (Case == 3) TestTrue(TEXT("External turn transition before deferred replay"), F.Gameplay.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
		if (Case == 2)
		{
			TestTrue(TEXT("Real mandatory choice begins before replay"), F.Gameplay.Battle->RequestPlayCard(F.Card(TEXT("A")), nullptr).IsAcceptedForResolution());
			F.Gameplay.FlushReady();
			TestFalse(TEXT("Mandatory choice kills pending EndTurn"), F.Widget->HasAcceptedBufferedEndTurn());
			TestFalse(TEXT("Mandatory choice rejects new EndTurn"), F.Widget->EndTurn());
			TestTrue(TEXT("Resolve through normal selection API"), F.Gameplay.ViewModel->SubmitPendingCardSelectionByRuntimeIds({F.Card(TEXT("B"))->GetRuntimeId()}));
		}
		F.Gameplay.FlushReady();
		FTSTicker::GetCoreTicker().Tick(0.0f);
		TestFalse(TEXT("Old intent cannot resurrect"), F.Widget->HasAcceptedBufferedEndTurn());
		FPlayerTurnAuthorityToken Turn;
		TestTrue(TEXT("Player turn retained"), F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn));
		TestEqual(TEXT("No old EndTurn fired"), Turn.PlayerTurnSerial, Case == 3 ? uint64(2) : uint64(1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BStableHandTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.StableHandAndHover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BStableHandTest::RunTest(const FString&)
{
	FShadowFixture F(false);
	F.Gameplay.World->SetGameInstance(NewObject<UGameInstance>(F.Gameplay.World));
	UClass* NativeClass = LoadClass<UBattleHUDWidget>(nullptr,
		TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C"));
	UBattleHUDWidget* NativeHUD = NativeClass ? CreateWidget<UBattleHUDWidget>(F.Gameplay.World, NativeClass) : nullptr;
	if (!TestNotNull(TEXT("Production Native HUD loads"), NativeHUD)) return false;
	NativeHUD->SetViewModel(F.Gameplay.ViewModel);
	UBattleHandFanPanel* NativeHand = Cast<UBattleHandFanPanel>(NativeHUD->GetWidgetFromName(TEXT("FanHand")));
	if (!TestNotNull(TEXT("Production formal fan exists"), NativeHand)
		|| !TestEqual(TEXT("Production Hand displays all cards"), NativeHand->GetChildrenCount(), 3)) return false;
	TArray<UWidget*> Before = NativeHand->GetAllChildren();
	TestTrue(TEXT("Normal selection publishes Input dirty"), F.Gameplay.ViewModel->SelectCardByRuntimeId(F.Card(TEXT("B"))->GetRuntimeId()));
	for (int32 Index = 0; Index < Before.Num(); ++Index)
		TestTrue(TEXT("Input-only publication preserves every formal Widget"), NativeHand->GetChildAt(Index) == Before[Index]);
	F.Gameplay.ViewModel->CancelSelection();
	// Exercise a live Slate tree: UObject reuse alone cannot prove that a
	// membership update preserves the card face and its Canvas placement.
	TSharedRef<SWidget> LiveHUD = NativeHUD->TakeWidget();
	TObjectPtr<UPanelSlot> SurvivorSlot = Before[1]->Slot;
	TWeakPtr<SWidget> SurvivorSlate = Before[1]->GetCachedWrappedWidget();
	if (!TestTrue(TEXT("Formal card has a live Slate tree"), SurvivorSlate.IsValid())) return false;
	TestTrue(TEXT("Normal Gameplay play removes A"), F.Gameplay.Battle->RequestPlayCard(F.Card(TEXT("A")), nullptr).IsAcceptedForResolution());
	F.Gameplay.FlushReady();
	TestEqual(TEXT("Frozen Hand loses exactly the played card"), NativeHand->GetChildrenCount(), 2);
	TestTrue(TEXT("Remaining B keeps its dedicated Hand slot"), Before[1]->Slot == SurvivorSlot);
	TestTrue(TEXT("Remaining B keeps its live Slate tree"), SurvivorSlate.IsValid()
		&& Before[1]->GetCachedWrappedWidget() == SurvivorSlate.Pin());
	const auto Arranged = NativeHandLayoutTest::Arrange(NativeHand, FVector2D(900, 280));
	const FGeometry* Remaining = NativeHandLayoutTest::Find(Arranged, Before[1]);
	if (!TestNotNull(TEXT("Remaining card receives its first Slate arrangement without NativeTick"), Remaining)) return false;
	TestTrue(TEXT("Remaining card retains the full configured size"), FVector2D(Remaining->GetLocalSize()) == NativeHUD->HandCardSize);
	const FGeometry* Other = NativeHandLayoutTest::Find(Arranged, Before[2]);
	TestTrue(TEXT("Remaining cards stay bottom-centered"), Other && FMath::IsNearlyEqual(
		NativeHandLayoutTest::Position(*Remaining).X + NativeHandLayoutTest::Position(*Other).X, 900.0 - NativeHUD->HandCardSize.X));
	UPhase6UIA2NR8HUDProbe* HUD = NewObject<UPhase6UIA2NR8HUDProbe>(F.Gameplay.World);
	HUD->SetTestWorld(F.Gameplay.World);
	UHorizontalBox* Hand = NewObject<UHorizontalBox>(HUD);
	HUD->ConfigureCardSurfaces(Hand, NewObject<UOverlay>(HUD), nullptr, nullptr, nullptr);
	HUD->SetViewModel(F.Gameplay.ViewModel);
	HUD->ReconcileHandForTesting();
	UWidget* Survivor = Hand->GetChildAt(1);
	Survivor->SetVisibility(ESlateVisibility::Hidden);
	Survivor->SetIsEnabled(false);
	HUD->ReconcileHandForTesting();
	TestTrue(TEXT("Same identity keeps exact Widget"), Hand->GetChildAt(1) == Survivor);
	TestEqual(TEXT("Reconcile preserves hidden source"), Survivor->GetVisibility(), ESlateVisibility::Hidden);
	F.Gameplay.ViewModel->HandCards.RemoveAt(0);
	HUD->ReconcileHandForTesting();
	TestTrue(TEXT("Membership change reuses survivor"), Hand->GetChildAt(0) == Survivor);
	UBattleHandFanPanel* Fan = NewObject<UBattleHandFanPanel>();
	Fan->AddChild(Survivor);
	Fan->CommitFrozenOrder();
	const auto BeforeHover = NativeHandLayoutTest::Arrange(Fan, FVector2D(900, 280));
	const FVector2D Position = NativeHandLayoutTest::Position(*NativeHandLayoutTest::Find(BeforeHover, Survivor));
	Fan->UpdateHoverAffordance(FVector2D::ZeroVector, INDEX_NONE, true, 1.0f);
	const auto AfterHover = NativeHandLayoutTest::Arrange(Fan, FVector2D(900, 280));
	TestTrue(TEXT("Hover cannot relayout structural slot"), NativeHandLayoutTest::Position(*NativeHandLayoutTest::Find(AfterHover, Survivor)) == Position);
	TestEqual(TEXT("Hover cannot reveal hidden owner"), Survivor->GetVisibility(), ESlateVisibility::Hidden);
	return true;
}
namespace NativeHandProductionPlaybackTest
{
	struct FContext
	{
		FShadowFixture Fixture;
		TStrongObjectPtr<UWorld> KeepWorld{Fixture.Gameplay.World};
		TStrongObjectPtr<UBattleHUDWidget> HUD;
		TSharedPtr<SWidget> LiveHUD;
		TArray<UWidget*> Cards;
		UPanelSlot* SurvivorSlot = nullptr;
		TWeakPtr<SWidget> SurvivorSlate;
		FAutomationTestBase* Test = nullptr;
		int32 Step = 0;
	};
	class FObserveBlockingHand : public IAutomationLatentCommand
	{
	public:
		explicit FObserveBlockingHand(TSharedRef<FContext> InContext) : Context(InContext) {}
		virtual bool Update() override
		{
			FContext& C = *Context;
			// First activate pending timers, then let the real HUD finish CardPlayed
			// on a later engine frame. Never call NativeTick or bypass its finish.
			C.Fixture.Gameplay.World->GetTimerManager().Tick(C.Step++ == 0 ? 0.0f : 0.6f);
			if (C.Step == 1) return false;
			FTSTicker::GetCoreTicker().Tick(0.0f); // normal deferred completion forwarding
			UBattleHandFanPanel* Hand = CastChecked<UBattleHandFanPanel>(C.HUD->GetWidgetFromName(TEXT("FanHand")));
			C.Test->TestEqual(TEXT("Real Blocking CardPlayed completion commits two survivors without NativeTick"), Hand->GetChildrenCount(), 2);
			C.Test->TestTrue(TEXT("Production survivor retains exact Widget and slot"), Hand->GetChildAt(0) == C.Cards[1] && C.Cards[1]->Slot == C.SurvivorSlot);
			C.Test->TestTrue(TEXT("Production survivor retains live Slate tree"), C.Cards[1]->GetCachedWrappedWidget() == C.SurvivorSlate.Pin());
			for (const FVector2D Size : { FVector2D(500, 280), FVector2D(1200, 360) })
			{
				const auto Arranged = NativeHandLayoutTest::Arrange(Hand, Size);
				const FGeometry* Geometry = NativeHandLayoutTest::Find(Arranged, C.Cards[1]);
				C.Test->TestTrue(TEXT("First allotted-size arrangement is full card size during trailing playback"), Geometry && FVector2D(Geometry->GetLocalSize()) == C.HUD->HandCardSize);
			}
			C.Test->TestTrue(TEXT("Remaining Blocking destination still owns playback"), C.Fixture.Controller->IsWaitingForCompletionForTesting());
			C.Fixture.Controller->Shutdown();
			C.HUD->SetViewModel(nullptr);
			return true;
		}
	private:
		TSharedRef<FContext> Context;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9BNativeHandBlockingLayoutTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.NativeHandBlockingWithoutTick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9BNativeHandBlockingLayoutTest::RunTest(const FString& Parameters)
{
	using namespace NativeHandProductionPlaybackTest;
	TSharedRef<FContext> Context = MakeShared<FContext>();
	Context->Test = this;
	Context->Fixture.Gameplay.World->SetGameInstance(NewObject<UGameInstance>(Context->Fixture.Gameplay.World));
	UClass* NativeClass = LoadClass<UBattleHUDWidget>(nullptr,
		TEXT("/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C"));
	Context->HUD.Reset(NativeClass ? CreateWidget<UBattleHUDWidget>(Context->Fixture.Gameplay.World, NativeClass) : nullptr);
	if (!TestNotNull(TEXT("Production HUD loads"), Context->HUD.Get())) return false;
	Context->HUD->SetViewModel(Context->Fixture.Gameplay.ViewModel);
	Context->Fixture.Controller->Initialize(Context->Fixture.Gameplay.Battle, Context->Fixture.Gameplay.ViewModel, Context->HUD.Get());
	Context->HUD->SetPresentationController(Context->Fixture.Controller);
	Context->LiveHUD = Context->HUD->TakeWidget();
	UBattleHandFanPanel* Hand = CastChecked<UBattleHandFanPanel>(Context->HUD->GetWidgetFromName(TEXT("FanHand")));
	Context->Cards = Hand->GetAllChildren();
	if (!TestEqual(TEXT("Frozen baseline has three cards"), Context->Cards.Num(), 3)) return false;
	Context->SurvivorSlot = Context->Cards[1]->Slot;
	Context->SurvivorSlate = Context->Cards[1]->GetCachedWrappedWidget();
	if (!TestTrue(TEXT("Actual Native CardPlayed accepts Blocking playback"), Context->Fixture.PlayA())) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FObserveBlockingHand(Context));
	return true;
}
#endif
