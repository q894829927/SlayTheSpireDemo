#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "NativePlayedCardTestFixture.h"
#include "UI/BattleHUDWidgetBase.h"
using namespace NativePlayedCardTest;

namespace
{
	bool PrepareBusyInput(FAutomationTestBase& Test, FFixture& F)
	{
		F.HUD->SetBufferedPlayerInputEnabled(true);
		F.Controller->SetDetachedCardArrivalD2Enabled(true);
		if (!Test.TestTrue(TEXT("A is accepted once"),F.Play())
			|| !Test.TestTrue(TEXT("Intermediate Block owns chronology"),F.Controller->IsWaitingForCompletionForTesting())
			|| !Test.TestEqual(TEXT("Two frozen cards remain"),F.Game.ViewModel->HandCards.Num(),2)) return false;
		const int32 B=F.Game.ViewModel->HandCards[0].RuntimeId;
		const int32 C=F.Game.ViewModel->HandCards[1].RuntimeId;
		if (!Test.TestTrue(TEXT("B draft accepted while A arrives"),F.HUD->SelectCard(B))
			|| !Test.TestTrue(TEXT("B fully confirmed"),F.HUD->SelectTarget(1))
			|| !Test.TestTrue(TEXT("C remains an unconfirmed pointer draft"),F.HUD->SelectCard(C))) return false;
		F.HUD->PointerForTesting(FVector2D(400,300));
		return Test.TestTrue(TEXT("Draft has a visible input pose"),F.Fan->MoveInputVisualTo(C,FVector2D(400,300)));
	}
	void Drain(FFixture& F)
	{
		for (int32 I=0; I<64 && F.Controller->IsWaitingForCompletionForTesting(); ++I) F.Finish();
		F.Game.FlushReady(); FTSTicker::GetCoreTicker().Tick(0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9EPolicyInput,"SlayTheSpireDemo.SelectionPresentation.G9E.InputRetirementWithoutTick",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9EPolicyInput::RunTest(const FString&)
{
	for (int32 Mode=0; Mode<6; ++Mode)
	{
		FFixture F(true,3,ECardDestination::Discard,5,1,ECardType::Skill);
		if (!PrepareBusyInput(*this,F)) return false;
		const int32 Draft=F.HUD->GetBufferedCardDraftRuntimeId();
		const auto Visual=F.HUD->LastArrivalPreparation;
		const auto Blocking=F.Controller->GetActivePlaybackTokenForTesting();
		FPresentationSessionToken Session; F.Controller->TryGetPresentationSessionToken(Session);
		FPlayerTurnAuthorityToken Turn; F.Game.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn);
		if (Mode==0) F.HUD->SetDetachedCardDestinationD1Enabled(false);
		if (Mode==1) F.HUD->SetDetachedCardArrivalD2Enabled(false);
		if (Mode==2) F.HUD->DiscardQueuedPlayerInput();
		if (Mode==3) F.HUD->SkipPresentation();
		if (Mode==4) F.Controller->ReconcileActiveEnvelopeToFinalSnapshotForTesting();
		if (Mode==5) F.Controller->Shutdown();
		TestEqual(TEXT("FIFO cleared immediately"),F.HUD->GetBufferedPlayerInputShadowForTesting().GetConfirmedPlayCount(),0);
		TestEqual(TEXT("Draft cleared immediately"),F.HUD->GetBufferedCardDraftRuntimeId(),INDEX_NONE);
		TestFalse(TEXT("Input pose is retired before any Tick"),F.Fan->MoveInputVisualTo(Draft,FVector2D(450,350)));
		if (Mode!=2) TestEqual(TEXT("Only input discard retains independent cosmetics"),F.HUD->HostedPlayedCountForTesting(),0);
		if (Mode<3) TestTrue(TEXT("Policy preserves exact session"),F.Controller->IsCurrentPresentationSession(Session));
		Drain(F);
		TestEqual(TEXT("Only A pays"),F.Game.Battle->Energy,2);
		TestEqual(TEXT("A Gameplay destination is committed once"),F.Game.Battle->GetDeckRuntimeForTesting()->GetDiscardCount(),1);
		if (Mode!=5) TestEqual(TEXT("Live chronology consumes the formal destination"),F.Game.ViewModel->DiscardCount,1);
		TestEqual(TEXT("B remains in Hand"),F.Game.Battle->GetDeckRuntimeForTesting()->GetHandCount(),2);
		FPlayerTurnAuthorityToken After; F.Game.Battle->TryGetCurrentPlayerTurnAuthorityToken(After);
		TestTrue(TEXT("Retirement preserves Gameplay turn"),After.BattleId==Turn.BattleId && After.PlayerTurnSerial==Turn.PlayerTurnSerial);
		F.HUD->RetireDetachedCardArrival(Visual);
		F.HUD->InvokeFinishForTesting(Blocking);
		TestEqual(TEXT("Old receipts cannot restart FIFO"),F.HUD->GetBufferedPlayerInputShadowForTesting().GetConfirmedPlayCount(),0);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9EEndTurnRetirement,"SlayTheSpireDemo.SelectionPresentation.G9E.EndTurnAndPolicyRetirement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9EEndTurnRetirement::RunTest(const FString&)
{
	for (bool D1:{false,true})
	{
		FFixture F(true,3,ECardDestination::Discard,5,1,ECardType::Skill);
		if (!PrepareBusyInput(*this,F)) return false;
		TestTrue(TEXT("EndTurn follows confirmed B"),F.HUD->EndTurn());
		TestFalse(TEXT("Repeat EndTurn rejected"),F.HUD->EndTurn());
		TestEqual(TEXT("EndTurn retains earlier B until retirement"),F.HUD->GetBufferedPlayerInputShadowForTesting().GetConfirmedPlayCount(),1);
		if (D1) F.HUD->SetDetachedCardDestinationD1Enabled(false); else F.HUD->SetDetachedCardArrivalD2Enabled(false);
		TestFalse(TEXT("Policy retires the old EndTurn intent"),F.HUD->HasAcceptedBufferedEndTurn());
		Drain(F);
		FPlayerTurnAuthorityToken Turn; F.Game.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn);
		TestEqual(TEXT("No old EndTurn crosses the boundary"),Turn.PlayerTurnSerial,static_cast<uint64>(1));
		TestEqual(TEXT("Already submitted A completes, queued B does not"),F.Game.Battle->Energy,2);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9EModelReplacement,"SlayTheSpireDemo.SelectionPresentation.G9E.ViewModelReplacementRejectsOldPlayback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9EModelReplacement::RunTest(const FString&)
{
	FFixture F(true,3,ECardDestination::Discard,5,1,ECardType::Skill);
	if (!PrepareBusyInput(*this,F)) return false;
	const auto Old=F.Controller->GetActivePlaybackTokenForTesting();
	Phase6UIA1Test::FHUDTestFixture Next(ECardTargetType::None,1);
	Next.World->AddToRoot(); Next.DrainInitialReady(); Next.InitializeViewModel();
	F.HUD->SetViewModel(Next.ViewModel);
	TestTrue(TEXT("New model installed"),F.HUD->ModelForTesting()==Next.ViewModel);
	TestNull(TEXT("Old Controller is detached"),F.HUD->ControllerForTesting());
	TestFalse(TEXT("Old local timer is retired synchronously"),F.HUD->IsLocalFinishTimerSet());
	TestFalse(TEXT("Old controller owns no pending playback"),F.Controller->IsWaitingForCompletionForTesting());
	const FText NewBlock=F.HUD->BlockTextForTesting()->GetText();
	const auto NewCards=F.Fan->GetAllChildren();
	CollectGarbage(RF_NoFlags);
	F.HUD->InvokeFinishForTesting(Old); F.HUD->InvokeCancelForTesting(Old); F.Controller->NotifyPresentationFinished(Old);
	FTSTicker::GetCoreTicker().Tick(0);
	TestTrue(TEXT("Old callbacks cannot change new block text"),F.HUD->BlockTextForTesting()->GetText().EqualTo(NewBlock));
	TestTrue(TEXT("Old callbacks cannot replace new Hand"),F.Fan->GetAllChildren()==NewCards);
	TestEqual(TEXT("Old battle stays paid once"),F.Game.Battle->Energy,2);
	TestEqual(TEXT("New battle remains unpaid"),Next.Battle->Energy,3);
	TestTrue(TEXT("New ordinary input works"),F.HUD->SelectCard(Next.FirstRuntimeId()) && F.HUD->ConfirmSelectedCard());
	Next.FlushReady(); FTSTicker::GetCoreTicker().Tick(0);
	TestEqual(TEXT("New request pays once"),Next.Battle->Energy,2);
	F.HUD->SetViewModel(nullptr); Next.World->RemoveFromRoot(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9EModelReentry,"SlayTheSpireDemo.SelectionPresentation.G9E.ReentrantModelReplacementKeepsNewestBinding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9EModelReentry::RunTest(const FString&)
{
	FFixture F(true,3,ECardDestination::Discard,5,1,ECardType::Skill);
	if (!PrepareBusyInput(*this,F)) return false;
	Phase6UIA1Test::FHUDTestFixture Requested,Newer;
	Requested.World->AddToRoot(); Newer.World->AddToRoot();
	Requested.DrainInitialReady(); Requested.InitializeViewModel(); Newer.DrainInitialReady(); Newer.InitializeViewModel();
	bool Reentered=false;
	const auto Observer=F.Game.ViewModel->OnNativeChanged.AddLambda([&](EBattleHUDDirtyFlags)
	{
		if (!Reentered) { Reentered=true; F.HUD->SetViewModel(Newer.ViewModel); CollectGarbage(RF_NoFlags); }
	});
	F.HUD->SetViewModel(Requested.ViewModel);
	TestTrue(TEXT("Old-history retirement publishes"),Reentered);
	TestTrue(TEXT("Reentrant newer binding wins"),F.HUD->ModelForTesting()==Newer.ViewModel);
	TestNull(TEXT("No stale controller restored"),F.HUD->ControllerForTesting());
	TestFalse(TEXT("No old timer restored"),F.HUD->IsLocalFinishTimerSet());
	F.Game.ViewModel->OnNativeChanged.Remove(Observer);
	F.HUD->SetViewModel(nullptr); Requested.World->RemoveFromRoot(); Newer.World->RemoveFromRoot(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9ELifetimeMatrix,"SlayTheSpireDemo.SelectionPresentation.G9E.BindingBattleAndDestructionMatrix",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9ELifetimeMatrix::RunTest(const FString&)
{
	for(int32 Mode=0; Mode<4; ++Mode)
	{
		FFixture F(true,3,ECardDestination::Discard,5,1,ECardType::Skill);
		if (!PrepareBusyInput(*this,F)) return false;
		const auto Old=F.Controller->GetActivePlaybackTokenForTesting();
		const auto Visual=F.HUD->LastArrivalPreparation;
		if (Mode==0) { F.Game.Battle->StartBattle(); F.Game.FlushReady(); }
		if (Mode==1) F.HUD->SetPresentationController(nullptr);
		if (Mode==2) F.HUD->InvokeNativeDestructForTesting();
		if (Mode==3) { F.Controller->Shutdown(); F.Controller->Initialize(F.Game.Battle,F.Game.ViewModel,F.HUD); F.HUD->SetPresentationController(F.Controller); }
		if (Mode==1) TestFalse(TEXT("Controller detach retires the local Blocking timer"),F.HUD->IsLocalFinishTimerSet());
		if (Mode==1) F.Controller->NotifyWidgetLost(nullptr); // duplicate loss with no owner is inert
		TestEqual(TEXT("Boundary drops old FIFO"),F.HUD->GetBufferedPlayerInputShadowForTesting().GetConfirmedPlayCount(),0);
		TestEqual(TEXT("Boundary retires cosmetics"),F.HUD->HostedPlayedCountForTesting(),0);
		TestEqual(TEXT("Boundary drops draft"),F.HUD->GetBufferedCardDraftRuntimeId(),INDEX_NONE);
		CollectGarbage(RF_NoFlags);
		const int32 Energy=F.Game.Battle->Energy; const auto Cards=F.Fan->GetAllChildren();
		F.HUD->RetireDetachedCardArrival(Visual); F.HUD->InvokeFinishForTesting(Old); F.Controller->NotifyPresentationFinished(Old);
		FTSTicker::GetCoreTicker().Tick(0);
		TestEqual(TEXT("Old callback cannot submit another request"),F.Game.Battle->Energy,Energy);
		TestTrue(TEXT("Old callback cannot touch the new Hand"),F.Fan->GetAllChildren()==Cards);
		TestTrue(TEXT("No Presentation boundary faults Gameplay"),F.Game.Battle->BattleState!=EBattleState::ResolutionFaulted);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9EHUDReplacement,"SlayTheSpireDemo.SelectionPresentation.G9E.HUDReplacementKeepsNewPlayback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9EHUDReplacement::RunTest(const FString&)
{
	FFixture F(true,3,ECardDestination::Discard,5,1,ECardType::Skill);
	if (!PrepareBusyInput(*this,F)) return false;
	const auto Old=F.Controller->GetActivePlaybackTokenForTesting();
	const auto OldVisual=F.HUD->LastArrivalPreparation;
	auto* Next=NewObject<UPhase6UIA2NR8HUDProbe>(F.Game.World); Next->AddToRoot(); Next->SetTestWorld(F.Game.World);
	auto* Play=NewObject<UOverlay>(Next); auto* Fan=NewObject<UBattleHandFanPanel>(Next);
	Next->ConfigureCardSurfaces(NewObject<UHorizontalBox>(Next),Play,NewObject<UTextBlock>(Next),NewObject<UTextBlock>(Next),NewObject<UTextBlock>(Next));
	Next->ConfigureBlockForTesting(NewObject<UTextBlock>(Next)); Next->ConfigureFanForTesting(Fan); Fan->TakeWidget();
	Next->SetViewModel(F.Game.ViewModel);
	const auto Root=FGeometry::MakeRoot(FVector2D(1000,700),FSlateLayoutTransform());
	CacheG9TestWidgetGeometry(Next->WidgetTree->RootWidget,Root);
	CacheG9TestWidgetGeometry(Fan,Root.MakeChild(FVector2D(800,220),FSlateLayoutTransform(FVector2D(100,480))));
	CacheG9TestWidgetGeometry(Play,Root.MakeChild(FVector2D(400,300),FSlateLayoutTransform(FVector2D(300,100))));
	F.Controller->SetWidget(Next); Next->SetPresentationController(F.Controller);
	Next->SetDetachedCardDestinationD1Enabled(true); Next->SetDetachedCardArrivalD2Enabled(true); Next->SetBufferedPlayerInputEnabled(true);
	TestEqual(TEXT("Old HUD FIFO is retired"),F.HUD->GetBufferedPlayerInputShadowForTesting().GetConfirmedPlayCount(),0);
	TestEqual(TEXT("Old HUD private visuals are retired"),F.HUD->HostedPlayedCountForTesting(),0);
	const int32 B=F.Game.ViewModel->HandCards[0].RuntimeId;
	TestTrue(TEXT("New HUD accepts a normal card"),Next->SelectCard(B) && Next->SelectTarget(1)); F.Game.FlushReady();
	TestTrue(TEXT("New HUD owns its Blocking continuation"),F.Controller->IsWaitingForCompletionForTesting() && Next->IsLocalFinishTimerSet());
	const FText Block=Next->BlockTextForTesting()->GetText(); const auto Cards=Fan->GetAllChildren();
	CollectGarbage(RF_NoFlags);
	F.HUD->RetireDetachedCardArrival(OldVisual); F.HUD->InvokeFinishForTesting(Old); F.HUD->InvokeCancelForTesting(Old);
	F.HUD->InvokeNativeDestructForTesting(); F.Controller->NotifyPresentationFinished(Old);
	TestTrue(TEXT("Old HUD destruction cannot cancel new playback"),Next->IsLocalFinishTimerSet() && F.Controller->IsWaitingForCompletionForTesting());
	TestTrue(TEXT("New formal Hand stays identical"),Fan->GetAllChildren()==Cards);
	TestTrue(TEXT("New block surface stays identical"),Next->BlockTextForTesting()->GetText().EqualTo(Block));
	TestEqual(TEXT("Only A and B pay"),F.Game.Battle->Energy,1);
	F.Controller->SkipPresentation(); Next->InvokeNativeDestructForTesting(); Next->SetViewModel(nullptr); Next->RemoveFromRoot();
	return true;
}
#endif
