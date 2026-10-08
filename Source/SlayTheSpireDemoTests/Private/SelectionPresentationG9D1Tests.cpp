#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "NativePlayedCardTestFixture.h"
#include "Components/CanvasPanelSlot.h"
using namespace NativePlayedCardTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D1OverlapTest,"SlayTheSpireDemo.SelectionPresentation.G9D1.CommitOnceAndOverlap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D1OverlapTest::RunTest(const FString&)
{
	FFixture F(true,3); if (!TestTrue(TEXT("A accepted"),F.Play())) return false;
	const auto Arrival = F.Controller->GetActivePlaybackTokenForTesting();
	TestTrue(TEXT("Arrival remains Blocking"),F.Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Destination is not shown before arrival"),F.Game.ViewModel->DiscardCount,0);
	F.Finish();
	const auto A = F.HUD->LastDetachedPreparation;
	if (!TestTrue(TEXT("Exact tail prepared"),A.IsValid())) return false;
	TestEqual(TEXT("Destination commits while tail is alive"),F.Game.ViewModel->DiscardCount,1);
	TestEqual(TEXT("A private job remains"),F.HUD->HostedPlayedCountForTesting(),1);
	TestFalse(TEXT("Tail has no Blocking owner or timer"),F.HUD->IsLocalPresentationActive() || F.HUD->IsLocalFinishTimerSet());
	TestFalse(TEXT("Ready has no cosmetic debt"),F.Game.ViewModel->bInputLocked);
	TestFalse(TEXT("Tail alone is not skippable chronology"),F.Controller->HasSkippablePresentationDelay());
	TestFalse(TEXT("Old arrival cannot complete detached tail"),F.HUD->CompleteHostedForTesting(A.Visual,Arrival));
	if (!TestTrue(TEXT("B accepted before A tail completion"),F.Play())) return false;
	TestEqual(TEXT("A tail and B arrival coexist"),F.HUD->HostedPlayedCountForTesting(),2);
	CollectGarbage(RF_NoFlags);
	int32 Publications = 0;
	const auto Observer = F.Game.ViewModel->OnNativeChanged.AddLambda([&](EBattleHUDDirtyFlags){ ++Publications; });
	const auto BArrival = F.Controller->GetActivePlaybackTokenForTesting();
	F.HUD->InvokeNativeTickForTesting(.6f);
	TestEqual(TEXT("A tail retires independently"),F.HUD->HostedPlayedCountForTesting(),1);
	TestEqual(TEXT("Cosmetic completion publishes nothing"),Publications,0);
	TestTrue(TEXT("B still awaits exact Blocking completion"),F.Controller->GetActivePlaybackTokenForTesting()==BArrival);
	TestEqual(TEXT("Gameplay costs remain once per command"),F.Game.Battle->Energy,1);
	F.Game.ViewModel->OnNativeChanged.Remove(Observer);
	F.Finish();
	TestEqual(TEXT("B destination commits once"),F.Game.ViewModel->DiscardCount,2);
	TestFalse(TEXT("B tail cannot own Controller completion"),F.Controller->IsWaitingForCompletionForTesting());
	F.HUD->InvokeNativeTickForTesting(.6f);
	TestEqual(TEXT("All tails clean"),F.HUD->HostedPlayedCountForTesting(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D1DeclineTest,"SlayTheSpireDemo.SelectionPresentation.G9D1.PreparationDeclineAndPureReceipt",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D1DeclineTest::RunTest(const FString&)
{
	FFixture F(true); F.HUD->bRejectDetachedPreparation = true;
	if (!TestTrue(TEXT("Play accepted"),F.Play())) return false;
	F.Finish();
	TestTrue(TEXT("Visual decline uses Blocking destination"),F.Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Prepare decline has zero formal destination effect"),F.Game.ViewModel->DiscardCount,0);
	F.Finish(); TestEqual(TEXT("Blocking destination commits once"),F.Game.ViewModel->DiscardCount,1);
	FFixture Pure(true);
	bool Observed = false;
	Pure.HUD->DetachedPreparationHook = [&]
	{
		Observed = true;
		TestEqual(TEXT("Preparation has not advanced display"),Pure.Game.ViewModel->DiscardCount,0);
		FPresentationStateSnapshot Working;
		TestTrue(TEXT("Formal working snapshot exists"),Pure.Controller->TryGetWorkingSnapshotForTesting(Working));
		TestEqual(TEXT("Preparation has not committed reducer"),Working.DiscardCount,0);
		TestFalse(TEXT("Prepared receipt cannot activate before formal commit"),Pure.HUD->ActivatePreparedDetachedCardDestination(Pure.HUD->LastDetachedPreparation));
	};
	TestTrue(TEXT("Pure scenario play"),Pure.Play()); Pure.Finish();
	TestTrue(TEXT("Preparation boundary observed"),Observed);
	TestEqual(TEXT("Only normal formal transaction consumed destination"),Pure.Game.ViewModel->DiscardCount,1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D1ReentryTest,"SlayTheSpireDemo.SelectionPresentation.G9D1.PostCommitFailureAndReentry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D1ReentryTest::RunTest(const FString&)
{
	// Each failure is injected through an actual visual/publication boundary.
	for (int32 Mode=0; Mode<7; ++Mode)
	{
		FFixture F(true); bool Fired = false;
		F.HUD->bRejectDetachedActivation = Mode==0;
		if (Mode==3) F.HUD->DetachedActivationHook = [&]{ Fired=true; F.Controller->SkipPresentation(); };
		const auto Observer = F.Game.ViewModel->OnNativeChanged.AddLambda([&](EBattleHUDDirtyFlags)
		{
			if (Fired || F.Game.ViewModel->DiscardCount != 1 || Mode==0 || Mode==3) return;
			Fired=true;
			if (Mode==1) F.Controller->SetDetachedCardDestinationD1Enabled(false);
			if (Mode==2) F.Controller->SkipPresentation();
			if (Mode==4) { if (auto* Card=F.HUD->PlayedCardForTesting()) Card->RemoveFromParent(); }
			if (Mode==5) F.Controller->SetWidget(nullptr);
			if (Mode==6) CacheG9TestWidgetGeometry(F.HUD->WidgetTree->RootWidget,FGeometry::MakeRoot(FVector2D::ZeroVector,FSlateLayoutTransform()));
		});
		if (!TestTrue(TEXT("Failure scenario accepted"),F.Play())) return false;
		F.Finish();
		TestEqual(TEXT("Post-commit failure never replays destination"),F.Game.ViewModel->DiscardCount,1);
		TestEqual(TEXT("Post-commit failure cleans exact visual"),F.HUD->HostedPlayedCountForTesting(),0);
		TestFalse(TEXT("Post-commit failure never falls back to Blocking"),F.Controller->IsWaitingForCompletionForTesting());
		TestEqual(TEXT("Post-commit failure never pays twice"),F.Game.Battle->Energy,2);
		if (Mode!=0) TestTrue(TEXT("Exact injection boundary reached"),Fired);
		F.Game.ViewModel->OnNativeChanged.Remove(Observer);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D1DisableTest,"SlayTheSpireDemo.SelectionPresentation.G9D1.DisableAndGlobalCleanup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D1DisableTest::RunTest(const FString&)
{
	FFixture Blocking(true); TestTrue(TEXT("Play accepted"),Blocking.Play());
	FPresentationSessionToken Session; Blocking.Controller->TryGetPresentationSessionToken(Session);
	FPlayerTurnAuthorityToken Turn; Blocking.Game.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn);
	Blocking.Controller->SetDetachedCardDestinationD1Enabled(false);
	Blocking.Finish();
	TestTrue(TEXT("Disable retains necessary correlation for Blocking destination"),Blocking.Controller->IsWaitingForCompletionForTesting());
	Blocking.Finish();
	TestEqual(TEXT("Formal destination completes normally after disable"),Blocking.Game.ViewModel->DiscardCount,1);
	TestTrue(TEXT("Disable keeps exact session"),Blocking.Controller->IsCurrentPresentationSession(Session));
	FPlayerTurnAuthorityToken After; Blocking.Game.Battle->TryGetCurrentPlayerTurnAuthorityToken(After);
	TestTrue(TEXT("Disable never changes turn authority"),Turn.BattleId==After.BattleId && Turn.PlayerTurnSerial==After.PlayerTurnSerial);
	for (int32 Mode=0; Mode<3; ++Mode)
	{
		FFixture F(true); TestTrue(TEXT("Cleanup scenario"),F.Play()); F.Finish();
		if (!TestEqual(TEXT("Tail alive without Blocking unit"),F.HUD->HostedPlayedCountForTesting(),1)) return false;
		if (Mode==0) F.Controller->SkipPresentation();
		if (Mode==1) F.Controller->ReconcileActiveEnvelopeToFinalSnapshotForTesting();
		if (Mode==2) F.Controller->SetDetachedCardDestinationD1Enabled(false);
		TestEqual(TEXT("Global boundary clears tails even without active record"),F.HUD->HostedPlayedCountForTesting(),0);
		TestEqual(TEXT("Cleanup preserves formal destination"),F.Game.ViewModel->DiscardCount,1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D1OwnerTest,"SlayTheSpireDemo.SelectionPresentation.G9D1.SameRuntimeReturnAndViewport",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D1OwnerTest::RunTest(const FString&)
{
	FFixture F(true,3); TestTrue(TEXT("Play accepted"),F.Play()); F.Finish();
	const auto Old=F.HUD->LastDetachedPreparation;
	auto* Tail=F.HUD->PlayedCardForTesting();
	const int32 HandCount=F.Fan->GetChildrenCount();
	CacheG9TestWidgetGeometry(F.HUD->WidgetTree->RootWidget,FGeometry::MakeRoot(FVector2D(1250,875),FSlateLayoutTransform()));
	F.HUD->InvokeNativeTickForTesting(.1f);
	TestEqual(TEXT("Private viewport update never writes Hand structure"),F.Fan->GetChildrenCount(),HandCount);
	TestTrue(TEXT("Private tail moves under its own geometry"),Tail && !Tail->GetRenderTransform().Translation.IsNearlyZero());
	TestEqual(TEXT("Viewport does not replay destination"),F.Game.ViewModel->DiscardCount,1);
	TestTrue(TEXT("Normal end turn accepted"),F.Game.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	F.Game.FlushReady();
	for (int32 I=0; I<48 && F.Controller->IsWaitingForCompletionForTesting(); ++I) F.Finish();
	TestFalse(TEXT("Normal next-turn history drains"),F.Controller->IsWaitingForCompletionForTesting());
	UBattleCardWidget* Returned=nullptr;
	for (auto* Child:F.Fan->GetAllChildren())
		if (auto* Card=Cast<UBattleCardWidget>(Child); Card && Card->GetRuntimeId()==Old.Visual.Lifecycle.RuntimeId) Returned=Card;
	if (!TestNotNull(TEXT("Same instance returns formally through normal draw"),Returned)) return false;
	TestTrue(TEXT("New formal owner is visible and interactive"),Returned->IsVisible() && Returned->GetIsEnabled());
	TestEqual(TEXT("Formal return retires old tail without waiting for its clock"),F.HUD->HostedPlayedCountForTesting(),0);
	F.HUD->RetireDetachedCardDestination(Old);
	TestTrue(TEXT("Old receipt cannot remove new formal Widget"),Returned->GetParent()==F.Fan);
	UCardInstance* Live=nullptr;
	for (UCardInstance* Card:F.Game.Battle->GetDeckRuntimeForTesting()->GetHandCards()) if (Card->GetRuntimeId()==Old.Visual.Lifecycle.RuntimeId) Live=Card;
	TestTrue(TEXT("Same instance can play again"),Live && F.Game.Battle->RequestPlayCard(Live,nullptr).IsAcceptedForResolution());
	F.Game.FlushReady();
	const auto New=F.HUD->HostedPlayedTokenForTesting();
	TestTrue(TEXT("New play has fresh occurrence and visual generation"),New.IsValid() && !(New==Old.Visual));
	F.HUD->RetireDetachedCardDestination(Old);
	TestEqual(TEXT("Old callback cannot affect new visual"),F.HUD->HostedPlayedCountForTesting(),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9D1DestinationsTest,"SlayTheSpireDemo.SelectionPresentation.G9D1.DestinationsAndPrecommitReentry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FG9D1DestinationsTest::RunTest(const FString&)
{
	for (auto Destination : {ECardDestination::Discard,ECardDestination::Exhaust,ECardDestination::Removed})
	{
		FFixture F(true,1,Destination); TestTrue(TEXT("Destination play accepted"),F.Play()); F.Finish();
		TestEqual(TEXT("Supported destination detaches"),F.HUD->HostedPlayedCountForTesting(),1);
		TestEqual(TEXT("Discard follows committed route"),F.Game.ViewModel->DiscardCount,Destination==ECardDestination::Discard?1:0);
		TestEqual(TEXT("Exhaust follows committed route"),F.Game.ViewModel->ExhaustCount,Destination==ECardDestination::Exhaust?1:0);
		TestFalse(TEXT("All supported tails are nonblocking"),F.Controller->IsWaitingForCompletionForTesting());
		F.HUD->InvokeNativeTickForTesting(.6f); TestEqual(TEXT("Finite visual lifetime"),F.HUD->HostedPlayedCountForTesting(),0);
	}
	for (int32 Mode=0;Mode<4;++Mode)
	{
		FFixture F(true); bool Observed=false;
		F.HUD->DetachedPreparationHook = [&]
		{
			Observed=true; TestEqual(TEXT("Reentry happens before formal commit"),F.Game.ViewModel->DiscardCount,0);
			if (Mode==0) F.Controller->SkipPresentation();
			if (Mode==1) F.Controller->SetDetachedCardDestinationD1Enabled(false);
			if (Mode==2) F.HUD->PlayedCardForTesting()->RemoveFromParent();
			if (Mode==3) F.Controller->SetWidget(nullptr);
		};
		TestTrue(TEXT("Prepare-reentry play"),F.Play()); F.Finish();
		if (F.Controller->IsWaitingForCompletionForTesting()) F.Finish();
		TestTrue(TEXT("Precommit boundary reached"),Observed);
		TestEqual(TEXT("Reentry/fallback yields destination once"),F.Game.ViewModel->DiscardCount,1);
		TestEqual(TEXT("No leftover candidate/visual"),F.HUD->HostedPlayedCountForTesting(),0);
	}
	return true;
}
#endif
