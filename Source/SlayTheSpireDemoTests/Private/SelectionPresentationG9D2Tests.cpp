#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "NativePlayedCardTestFixture.h"
using namespace NativePlayedCardTest;

// Editor-only mutations of real sealed data test the production pure preflight.
struct FDetachedArrivalControllerTestProbe
{
	static int32 Pending(UBattlePresentationController* C) { return C->DetachedCardArrivalReceipts.Num(); }
	static bool MutatedPlan(UBattlePresentationController* C, int32 Mode)
	{
		// Keep the original records' allocation alive while the caller retains its
		// current-record reference; malformed candidates are separate copies.
		auto Saved = MoveTemp(C->ActiveEnvelope);
		C->ActiveEnvelope = Saved;
		const auto Record = C->ActiveEnvelope.Records[C->ActiveRecordIndex];
		if (Mode==0) C->ActiveEnvelope.Records.SetNum(C->ActiveRecordIndex+1);
		if (Mode==1) { auto Duplicate=C->ActiveEnvelope.Records.Last(); ++Duplicate.PresentationSequence; C->ActiveEnvelope.Records.Add(Duplicate); }
		if (Mode==2) C->ActiveEnvelope.Records.Last().CardZoneChanged.Card.CardId=TEXT("WrongIdentity");
		if (Mode==3) C->ActiveEnvelope.Records.Last().CardZoneChanged.FromIndex=1;
		if (Mode==4) C->ActiveEnvelope.Records.Last().CardZoneChanged.ToZone=ECardZone::Hand;
		FPlayedCardPresentationLifecycleToken Life; FPresentationRecord Destination;
		const bool Result=C->TryGetDetachedCardArrivalPlan(Record,Life,Destination);
		C->ActiveEnvelope=MoveTemp(Saved); return Result;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D2EarlyDestination,"SlayTheSpireDemo.SelectionPresentation.G9D2.EarlyDestinationAndOverlap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D2EarlyDestination::RunTest(const FString&)
{
	FFixture F(true,3); F.Controller->SetDetachedCardArrivalD2Enabled(true);
	TestTrue(TEXT("A accepted through normal Gameplay"),F.Play());
	TestEqual(TEXT("Formal CardPlayed commits before any arrival tick"),F.Game.ViewModel->HandCards.Num(),2);
	TestEqual(TEXT("Destination commits during arrival"),F.Game.ViewModel->DiscardCount,1);
	TestFalse(TEXT("No cosmetic Blocking debt"),F.Controller->IsWaitingForCompletionForTesting() || F.HUD->IsLocalFinishTimerSet());
	TestTrue(TEXT("Private arrival remains entering"),F.HUD->ArrivalPhaseForTesting()==ECardVisualPhase::EnteringPlayArea);
	TestTrue(TEXT("Exact destination is pending in visual phase"),F.HUD->ArrivalHasCommittedDestinationForTesting());
	TestEqual(TEXT("Formal destination consumed admission once"),FDetachedArrivalControllerTestProbe::Pending(F.Controller),0);
	const auto A=F.HUD->LastArrivalPreparation;
	TestFalse(TEXT("Old receipt cannot activate again"),F.HUD->ActivatePreparedDetachedCardArrival(A));
	TestFalse(TEXT("Old destination callback cannot commit again"),F.HUD->CommitDetachedCardArrivalDestination(A));
	TestTrue(TEXT("B accepted while A arrival still moves"),F.Play());
	TestEqual(TEXT("Two arrivals coexist"),F.HUD->HostedPlayedCountForTesting(),2);
	CollectGarbage(RF_NoFlags);
	int32 Publications=0; auto Observer=F.Game.ViewModel->OnNativeChanged.AddLambda([&](EBattleHUDDirtyFlags){++Publications;});
	F.HUD->InvokeNativeTickForTesting(.4f);
	TestTrue(TEXT("Early destination never shortcuts arrival"),F.HUD->ArrivalPhaseForTesting()==ECardVisualPhase::EnteringPlayArea);
	F.HUD->InvokeNativeTickForTesting(.2f);
	TestTrue(TEXT("Arrival continuously hands off to tail"),F.HUD->ArrivalPhaseForTesting()==ECardVisualPhase::DestinationTail);
	F.HUD->InvokeNativeTickForTesting(.5f);
	TestEqual(TEXT("Both private journeys retire"),F.HUD->HostedPlayedCountForTesting(),0);
	TestEqual(TEXT("Cosmetic phases publish nothing"),Publications,0);
	TestEqual(TEXT("Each formal destination commits once"),F.Game.ViewModel->DiscardCount,2);
	TestEqual(TEXT("Each request charges once"),F.Game.Battle->Energy,1);
	F.Game.ViewModel->OnNativeChanged.Remove(Observer); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D2Preflight,"SlayTheSpireDemo.SelectionPresentation.G9D2.PurePreflightAndBlockingFallback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D2Preflight::RunTest(const FString&)
{
	FFixture F(true,3); F.Controller->SetDetachedCardArrivalD2Enabled(true); F.HUD->bRejectArrivalPreparation=true;
	F.HUD->ArrivalPreparationHook=[&]
	{
		FPlayedCardPresentationLifecycleToken A,B; FPresentationRecord DA,DB;
		TestTrue(TEXT("Canonical plan valid"),F.Controller->TryGetDetachedCardArrivalPlan(F.HUD->LastArrivalRecord,A,DA));
		TestTrue(TEXT("Repeated plan remains pure"),F.Controller->TryGetDetachedCardArrivalPlan(F.HUD->LastArrivalRecord,B,DB) && A==B);
		TestFalse(TEXT("Prepared receipt cannot activate before commit"),F.HUD->ActivatePreparedDetachedCardArrival(F.HUD->LastArrivalPreparation));
		for(int32 Mode=0; Mode<5; ++Mode) TestFalse(TEXT("Missing/duplicate/identity/index/unsupported future declines"),FDetachedArrivalControllerTestProbe::MutatedPlan(F.Controller,Mode));
		TestEqual(TEXT("Preflight preserves formal Hand"),F.Game.ViewModel->HandCards.Num(),3);
		TestEqual(TEXT("Preflight preserves displayed energy"),F.Game.ViewModel->Energy,3);
		TestEqual(TEXT("No committed admission during prepare"),FDetachedArrivalControllerTestProbe::Pending(F.Controller),0);
	};
	TestTrue(TEXT("Gameplay accepts once"),F.Play());
	TestTrue(TEXT("Prepare decline keeps Blocking arrival"),F.Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("No formal Hand effect from failed prepare"),F.Game.ViewModel->HandCards.Num(),3);
	TestEqual(TEXT("No orphan prepared clone"),F.HUD->HostedPlayedCountForTesting(),1);
	F.Finish(); TestEqual(TEXT("Blocking arrival still allows D1 destination"),F.Game.ViewModel->DiscardCount,1);
	TestFalse(TEXT("D1 tail remains independent"),F.Controller->IsWaitingForCompletionForTesting()); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D2Loss,"SlayTheSpireDemo.SelectionPresentation.G9D2.LossDisableAndLateDestination",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D2Loss::RunTest(const FString&)
{
	for(int32 Mode=0; Mode<4; ++Mode)
	{
		FFixture F(true,3,ECardDestination::Discard,5); F.Controller->SetDetachedCardArrivalD2Enabled(true);
		TestTrue(TEXT("Card with an intermediate Blocking record accepts"),F.Play());
		if(!TestTrue(TEXT("Only Block history owns completion"),F.Controller->IsWaitingForCompletionForTesting())) return false;
		TestEqual(TEXT("Destination uncommitted at its normal cursor"),F.Game.ViewModel->DiscardCount,0);
		TestEqual(TEXT("Exact formal admission retained"),FDetachedArrivalControllerTestProbe::Pending(F.Controller),1);
		FPresentationSessionToken Session; F.Controller->TryGetPresentationSessionToken(Session);
		FPlayerTurnAuthorityToken Turn; F.Game.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn);
		const auto Receipt=F.HUD->LastArrivalPreparation;
		if(Mode==0) F.HUD->RetireDetachedCardArrival(Receipt);
		if(Mode==1) F.Controller->SetDetachedCardArrivalD2Enabled(false);
		if(Mode==2) F.Controller->SetDetachedCardDestinationD1Enabled(false);
		if(Mode==3) { F.HUD->InvokeNativeTickForTesting(.6f); TestTrue(TEXT("Arrival may finish before destination"),F.HUD->ArrivalPhaseForTesting()==ECardVisualPhase::AtPlayArea); }
		CollectGarbage(RF_NoFlags);
		TestEqual(TEXT("Visual policy/loss retains formal obligation"),FDetachedArrivalControllerTestProbe::Pending(F.Controller),1);
		F.Finish(); TestEqual(TEXT("Future destination commits once"),F.Game.ViewModel->DiscardCount,1);
		TestEqual(TEXT("Formal admission consumed"),FDetachedArrivalControllerTestProbe::Pending(F.Controller),0);
		TestFalse(TEXT("Destination never waits for private arrival"),F.Controller->IsWaitingForCompletionForTesting());
		TestTrue(TEXT("Disable preserves session"),F.Controller->IsCurrentPresentationSession(Session));
		FPlayerTurnAuthorityToken After; F.Game.Battle->TryGetCurrentPlayerTurnAuthorityToken(After);
		TestTrue(TEXT("Policy preserves turn serial"),After.BattleId==Turn.BattleId && After.PlayerTurnSerial==Turn.PlayerTurnSerial);
		if(Mode<3) TestEqual(TEXT("Lost/disabled visual stays absent"),F.HUD->HostedPlayedCountForTesting(),0);
		else TestTrue(TEXT("Late destination starts tail from PlayArea"),F.HUD->ArrivalPhaseForTesting()==ECardVisualPhase::DestinationTail);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D2Reentry,"SlayTheSpireDemo.SelectionPresentation.G9D2.TransactionReentry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D2Reentry::RunTest(const FString&)
{
	for(int32 Mode=0; Mode<10; ++Mode)
	{
		FFixture F(true,3); F.Controller->SetDetachedCardArrivalD2Enabled(true); bool Once=false;
		if(Mode==0) F.HUD->bRejectArrivalActivation=true;
		if(Mode==1) F.HUD->bRejectArrivalDestination=true;
		if(Mode==2) F.HUD->ArrivalPreparationHook=[&]{F.Controller->SetDetachedCardArrivalD2Enabled(false);};
		if(Mode==3) F.HUD->ArrivalPreparationHook=[&]{F.Controller->SkipPresentation();};
		if(Mode==4) F.HUD->ArrivalActivationHook=[&]{F.Controller->SkipPresentation();};
		if(Mode==5) F.HUD->ArrivalDestinationHook=[&]{F.Controller->SkipPresentation();};
		auto Observer=F.Game.ViewModel->OnNativeChanged.AddLambda([&](EBattleHUDDirtyFlags)
		{
			if(Once || F.Game.ViewModel->Energy!=2) return; Once=true;
			if(Mode==6) F.Controller->SetDetachedCardArrivalD2Enabled(false);
			if(Mode==7) F.Controller->NotifyWidgetLost(F.HUD);
			if(Mode==8) F.HUD->RetireDetachedCardArrival(F.HUD->LastArrivalPreparation);
			if(Mode==9) CacheG9TestWidgetGeometry(F.HUD->WidgetTree->RootWidget,FGeometry::MakeRoot(FVector2D::ZeroVector,FSlateLayoutTransform()));
		});
		TestTrue(TEXT("Request accepts once"),F.Play());
		for(int32 I=0; I<8 && F.Controller->IsWaitingForCompletionForTesting(); ++I) F.Finish();
		TestEqual(TEXT("Reentry cannot duplicate cost"),F.Game.Battle->Energy,2);
		TestEqual(TEXT("Reentry preserves one formal destination"),F.Game.ViewModel->DiscardCount,1);
		TestEqual(TEXT("Reentry leaves no formal admission"),FDetachedArrivalControllerTestProbe::Pending(F.Controller),0);
		TestFalse(TEXT("No stuck Controller"),F.Controller->IsWaitingForCompletionForTesting());
		F.Controller->SkipPresentation(); TestEqual(TEXT("Global cleanup retires every cosmetic"),F.HUD->HostedPlayedCountForTesting(),0);
		F.Game.ViewModel->OnNativeChanged.Remove(Observer);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D2Return,"SlayTheSpireDemo.SelectionPresentation.G9D2.SameRuntimeReturnViewportAndDestinations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D2Return::RunTest(const FString&)
{
	for(auto Destination:{ECardDestination::Discard,ECardDestination::Exhaust,ECardDestination::Removed})
	{
		FFixture F(true,3,Destination); F.Controller->SetDetachedCardArrivalD2Enabled(true);
		TestTrue(TEXT("Supported destination accepts"),F.Play());
		TestFalse(TEXT("Every supported destination is private during arrival"),F.Controller->IsWaitingForCompletionForTesting());
		TestTrue(TEXT("Destination already committed"),F.HUD->ArrivalHasCommittedDestinationForTesting());
		const auto Old=F.HUD->LastArrivalPreparation; const int32 Count=F.Fan->GetChildrenCount();
		CacheG9TestWidgetGeometry(F.HUD->WidgetTree->RootWidget,FGeometry::MakeRoot(FVector2D(1250,875),FSlateLayoutTransform()));
		F.HUD->InvokeNativeTickForTesting(.1f); TestEqual(TEXT("Private resize never changes Hand structure"),F.Fan->GetChildrenCount(),Count);
		if(Destination!=ECardDestination::Discard) { F.Controller->SkipPresentation(); continue; }
		TestTrue(TEXT("Normal end turn accepted"),F.Game.Battle->RequestEndPlayerTurn().IsAcceptedForResolution()); F.Game.FlushReady();
		for(int32 I=0; I<48 && F.Controller->IsWaitingForCompletionForTesting(); ++I) F.Finish();
		UBattleCardWidget* Returned=nullptr;
		for(auto* Child:F.Fan->GetAllChildren()) if(auto* Card=Cast<UBattleCardWidget>(Child); Card && Card->GetRuntimeId()==Old.Visual.Lifecycle.RuntimeId) Returned=Card;
		if(!TestNotNull(TEXT("Same runtime returns through formal draw"),Returned)) return false;
		TestTrue(TEXT("Returned formal owner interactive"),Returned->IsVisible() && Returned->GetIsEnabled());
		TestEqual(TEXT("Return retires unfinished old arrival"),F.HUD->HostedPlayedCountForTesting(),0);
		UCardInstance* Live=nullptr;
		for(UCardInstance* Card:F.Game.Battle->GetDeckRuntimeForTesting()->GetHandCards()) if(Card->GetRuntimeId()==Old.Visual.Lifecycle.RuntimeId) Live=Card;
		TestTrue(TEXT("Same runtime replays normally"),Live && F.Game.Battle->RequestPlayCard(Live,nullptr).IsAcceptedForResolution()); F.Game.FlushReady();
		TestTrue(TEXT("New occurrence uses new visual generation"),!(F.HUD->LastArrivalPreparation.Visual==Old.Visual));
		F.HUD->RetireDetachedCardArrival(Old); TestEqual(TEXT("Old receipt cannot delete new visual"),F.HUD->HostedPlayedCountForTesting(),1);
	} return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D2Capacity,"SlayTheSpireDemo.SelectionPresentation.G9D2.CapacityAndGlobalBoundaries",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D2Capacity::RunTest(const FString&)
{
	// Exercise capacity with legal ten-card hands across turns; exhausted cards
	// cannot return and retire their older visual jobs. No private clock ticks.
	FFixture Full(true,33,ECardDestination::Exhaust,0,0); Full.Controller->SetDetachedCardArrivalD2Enabled(true);
	TestEqual(TEXT("Gameplay hand cap remains unchanged"),Full.Game.ViewModel->HandCards.Num(),10);
	for(int32 I=0; I<32; ++I)
	{
		if (Full.Game.Battle->GetDeckRuntimeForTesting()->GetHandCount()==0)
		{
			if (!TestTrue(TEXT("Normal turn transition accepted"),Full.Game.Battle->RequestEndPlayerTurn().IsAcceptedForResolution())) return false;
			Full.Game.FlushReady();
			for(int32 Step=0; Step<64 && Full.Controller->IsWaitingForCompletionForTesting(); ++Step) Full.Finish();
		}
		if(!TestTrue(TEXT("Private admission within capacity"),Full.Play())) return false;
	}
	TestEqual(TEXT("32 independent jobs retained"),Full.HUD->HostedPlayedCountForTesting(),32);
	TestTrue(TEXT("Next command accepted"),Full.Play()); TestTrue(TEXT("Over-capacity goes Blocking"),Full.Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("No extra private job beyond cap"),Full.HUD->HostedPlayedCountForTesting(),32);
	TestEqual(TEXT("Declined prepare cannot commit Hand early"),Full.Game.ViewModel->HandCards.Num(),1);
	Full.Controller->SkipPresentation(); TestEqual(TEXT("Skip clears all jobs"),Full.HUD->HostedPlayedCountForTesting(),0);
	for(int32 Mode=0; Mode<3; ++Mode)
	{
		FFixture F(true,3,ECardDestination::Discard,5); F.Controller->SetDetachedCardArrivalD2Enabled(true); TestTrue(TEXT("Boundary play accepted"),F.Play());
		if(Mode==0) F.Controller->ReconcileActiveEnvelopeToFinalSnapshotForTesting();
		if(Mode==1) F.Controller->NotifyWidgetLost(F.HUD);
		if(Mode==2) F.HUD->InvokeNativeDestructForTesting();
		TestEqual(TEXT("Boundary clears exact cosmetics"),F.HUD->HostedPlayedCountForTesting(),0);
		TestEqual(TEXT("Collapsed chronology leaves no orphan admission"),FDetachedArrivalControllerTestProbe::Pending(F.Controller),0);
		TestEqual(TEXT("Gameplay remains resolved"),F.Game.Battle->Energy,2);
	} return true;
}
#endif
