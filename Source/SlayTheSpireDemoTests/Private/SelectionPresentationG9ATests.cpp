#include "SelectionPresentationG9TestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9APlayerTurnAuthorityTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.PlayerTurnAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9APlayerTurnAuthorityTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA1Test;
	FHUDTestFixture Fixture(ECardTargetType::None, 0, 0);
	Fixture.DrainInitialReady();
	FPlayerTurnAuthorityToken First, Same, Next, Restarted;
	if (!TestTrue(TEXT("First formal player turn has authority"), Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(First))) return false;
	TestEqual(TEXT("First turn is serial 1"), First.PlayerTurnSerial, uint64(1));
	FBattleReadSnapshot Before, After;
	Fixture.Battle->TryBuildReadSnapshot(Before);
	TestTrue(TEXT("Real card play accepted"), Fixture.Battle->RequestPlayCard(Fixture.FirstAuthoritativeHandCard(), nullptr).IsAcceptedForResolution());
	Fixture.Battle->TryBuildReadSnapshot(After);
	Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(Same);
	TestTrue(TEXT("A card changes revision"), Before.StateRevision != After.StateRevision);
	TestTrue(TEXT("A card does not change turn identity"), First == Same);
	TestTrue(TEXT("Real turn cycle accepted"), Fixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(Next);
	TestEqual(TEXT("Next turn increments exactly once"), Next.PlayerTurnSerial, uint64(2));
	TestTrue(TEXT("Returning to PlayerTurn cannot ABA-match"), First != Next);
	Fixture.Battle->StartBattle();
	Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(Restarted);
	TestEqual(TEXT("Restart resets turn serial"), Restarted.PlayerTurnSerial, uint64(1));
	TestTrue(TEXT("Restart replaces battle identity"), Restarted.BattleId != First.BattleId);
	Fixture.Battle->BattleState = EBattleState::EnemyTurn;
	TestFalse(TEXT("EnemyTurn has no player-turn token"), Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(Restarted));
	TestFalse(TEXT("Failed probe clears output token"), Restarted.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9AEndTurnShadowTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.EndTurnExpressExecuteAndDirectBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9AEndTurnShadowTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ATest;
	for (bool bRecorded : {false, true})
	{
		FShadowFixture Fixture(bRecorded);
		if (!TestNotNull(TEXT("HUD"), Fixture.Widget)) return false;
		FBattleHUDBufferedPlayerInput& Input = Fixture.Input();
		UPhase6UIA0ManualFinishAction* Hold = Fixture.HoldQueue();
		const FEndTurnIntentAvailability Availability = Input.EvaluateEndTurnAvailability();
		TestTrue(TEXT("Ordinary busy resolution still accepts EndTurn intent"), Availability.bCanAcceptEndTurnIntent);
		TestFalse(TEXT("Busy resolution cannot execute EndTurn now"), Availability.bCanExecuteEndTurnNow);
		TestTrue(TEXT("EndTurn shadow accepted"), Input.TryAcceptEndTurn(true).bAccepted);
		TestFalse(TEXT("Second EndTurn is not accepted"), Input.TryAcceptEndTurn(false).bAccepted);
		TestFalse(TEXT("Accepted EndTurn blocks later card intent"), Input.TryCaptureCard(Fixture.Card(TEXT("B"))->GetRuntimeId()));
		TestTrue(TEXT("Intent waits while busy"), Input.EvaluatePending() == EBufferedPlayerIntentEvaluation::Waiting);
		Hold->CompleteManually();
		TestTrue(TEXT("Same turn becomes executable"), Input.EvaluatePending() == EBufferedPlayerIntentEvaluation::Ready);
		FBufferedPlayerIntentDecision Decision;
		TestTrue(TEXT("Ready shadow decision consumed"), Input.TakeReadyIntent(Decision));
		TestEqual(TEXT("DirectBaseline does not fabricate a SessionToken"), Decision.EndTurn.PresentationFence.IsSet(), bRecorded);
		TestFalse(TEXT("Consumption is once only"), Input.TakeReadyIntent(Decision));
		FPlayerTurnAuthorityToken Current;
		Fixture.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Current);
		TestTrue(TEXT("Shadow did not send EndTurn"), Current == Availability.Turn);
		TestEqual(TEXT("Shadow did not play any card"), Fixture.Gameplay.Battle->GetDeckRuntimeForTesting()->GetHandCount(), 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9ACardTargetShadowTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.CardSealedTargetAndOnceOnlyTake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9ACardTargetShadowTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ATest;
	FShadowFixture Fixture;
	FBattleHUDBufferedPlayerInput& Input = Fixture.Input();
	const int32 B = Fixture.Card(TEXT("B"))->GetRuntimeId();
	const int32 C = Fixture.Card(TEXT("C"))->GetRuntimeId();
	TestFalse(TEXT("Ready normal surface needs no buffer"), Input.TryCaptureCard(B));
	if (!TestTrue(TEXT("Real A play blocks Presentation"), Fixture.PlayA())) return false;
	TestTrue(TEXT("Already sealed B target captured"), Input.TryCaptureCard(B));
	TestTrue(TEXT("Repeated click re-captures C"), Input.TryCaptureCard(C));
	TestTrue(TEXT("Display lag waits"), Input.EvaluatePending() == EBufferedPlayerIntentEvaluation::Waiting);
	Fixture.FinishPlayback();
	TestTrue(TEXT("Exact target is now ready"), Input.EvaluatePending() == EBufferedPlayerIntentEvaluation::Ready);
	FBufferedPlayerIntentDecision Decision;
	TestTrue(TEXT("One shadow selection can be taken"), Input.TakeReadyIntent(Decision));
	TestEqual(TEXT("Newest click owns the entire intent"), Decision.Card.RuntimeId, C);
	TestFalse(TEXT("No second take/reentrant replay"), Input.TakeReadyIntent(Decision));
	TestEqual(TEXT("Shadow does not select or confirm a card"), Fixture.Gameplay.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestEqual(TEXT("Only the requested A left Hand"), Fixture.Gameplay.Battle->GetDeckRuntimeForTesting()->GetHandCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9ACardFencesTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.CardAuthorityFences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9ACardFencesTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ATest;
	for (int32 Case = 0; Case < 5; ++Case)
	{
		FShadowFixture Fixture;
		FBattleHUDBufferedPlayerInput& Input = Fixture.Input();
		const int32 B = Fixture.Card(TEXT("B"))->GetRuntimeId();
		if (!TestTrue(TEXT("A play"), Fixture.PlayA())) return false;
		if (Case == 0)
		{
			UPhase6UIA0ManualFinishAction* Hold = Fixture.HoldQueue();
			TestFalse(TEXT("Busy with no exact current sealed target cannot buffer a card"), Input.TryCaptureCard(B));
			Hold->CompleteManually();
			continue;
		}
		if (!TestTrue(TEXT("Capture B"), Input.TryCaptureCard(B))) return false;
		if (Case == 1) Fixture.Controller->SkipPresentation();
		if (Case == 2) Fixture.Controller->ReconcileActiveEnvelopeToFinalSnapshotForTesting();
		if (Case == 3) Fixture.Controller->SetWidget(NewObject<UPhase6UIA2APlaybackWidget>(Fixture.Gameplay.World));
		if (Case == 4) Fixture.Gameplay.Battle->RequestPlayCard(Fixture.Card(TEXT("C")), nullptr);
		TestTrue(TEXT("Skip/recovery/session/target replacement retires old intent"), Input.EvaluatePending() == EBufferedPlayerIntentEvaluation::Dropped);
		TestTrue(TEXT("Stale card cannot be taken"), Input.GetPendingKind() == EBufferedPlayerIntentKind::None);
	}
	FShadowFixture Direct(false);
	TestFalse(TEXT("DirectBaseline cannot fabricate a card target"), Direct.Input().TryCaptureCard(Direct.Card(TEXT("B"))->GetRuntimeId()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9AAcceptBeforeRetireTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.EndTurnAcceptBeforeRetire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9AAcceptBeforeRetireTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ATest;
	FShadowFixture Fixture;
	FBattleHUDBufferedPlayerInput& Input = Fixture.Input();
	if (!TestTrue(TEXT("A play"), Fixture.PlayA())) return false;
	TestTrue(TEXT("Older card intent captured"), Input.TryCaptureCard(Fixture.Card(TEXT("B"))->GetRuntimeId()));
	Fixture.Gameplay.ViewModel->InteractionState = EBattleHUDInteractionState::PresentationUnavailable;
	FEndTurnIntentAcceptance Rejected = Input.TryAcceptEndTurn(true);
	TestFalse(TEXT("Unsafe EndTurn rejected"), Rejected.bAccepted);
	TestFalse(TEXT("Unaccepted EndTurn does not retire FastInput"), Rejected.bRetireFastInputRetry);
	TestFalse(TEXT("Unaccepted EndTurn does not cancel transient state"), Rejected.bCancelTransientSelection);
	TestTrue(TEXT("Unaccepted EndTurn preserves older card intent"), Input.GetPendingKind() == EBufferedPlayerIntentKind::CardSelection);
	Fixture.Gameplay.ViewModel->InteractionState = EBattleHUDInteractionState::ChoosingTarget;
	Fixture.Gameplay.ViewModel->SelectedCardRuntimeId = Fixture.Card(TEXT("B"))->GetRuntimeId();
	const bool bCanEndTurnBefore = Fixture.Gameplay.ViewModel->bCanEndTurn;
	FEndTurnIntentAcceptance Accepted = Input.TryAcceptEndTurn(true);
	TestTrue(TEXT("Exact turn accepts EndTurn"), Accepted.bAccepted);
	TestTrue(TEXT("Accepted EndTurn retires old card"), Accepted.bRetireCardIntent);
	TestTrue(TEXT("Accepted EndTurn retires FastInput in shadow decision"), Accepted.bRetireFastInputRetry);
	TestTrue(TEXT("Accepted EndTurn plans normal transient cancellation"), Accepted.bCancelTransientSelection);
	TestFalse(TEXT("Card cannot replace EndTurn"), Input.TryCaptureCard(Fixture.Card(TEXT("C"))->GetRuntimeId()));
	TestEqual(TEXT("Shadow leaves production target choice unchanged"), Fixture.Gameplay.ViewModel->InteractionState, EBattleHUDInteractionState::ChoosingTarget);
	TestEqual(TEXT("bCanEndTurn keeps baseline meaning"), Fixture.Gameplay.ViewModel->bCanEndTurn, bCanEndTurnBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9AEndTurnStaleTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.EndTurnStaleAndMandatorySelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9AEndTurnStaleTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ATest;
	for (int32 Case = 0; Case < 5; ++Case)
	{
		FShadowFixture Fixture(false, Case == 4);
		FBattleHUDBufferedPlayerInput& Input = Fixture.Input();
		TestTrue(TEXT("Same-turn EndTurn captured"), Input.TryAcceptEndTurn(false).bAccepted);
		if (Case == 0) Fixture.Gameplay.Battle->RequestEndPlayerTurn();
		if (Case == 1) Fixture.Gameplay.Battle->StartBattle();
		if (Case == 2) Fixture.Gameplay.ViewModel->Outcome = EBattleHUDOutcome::Victory;
		if (Case == 3) Input.SetEnabled(false);
		if (Case == 4)
		{
			TestTrue(TEXT("Real mandatory choice requested"), Fixture.Gameplay.Battle->RequestPlayCard(Fixture.Card(TEXT("A")), nullptr).IsAcceptedForResolution());
			TestTrue(TEXT("Gameplay owns unresolved selection"), Fixture.Gameplay.ViewModel->HasAuthoritativePendingCardSelection());
			TestFalse(TEXT("Mandatory selection blocks new EndTurn"), Input.EvaluateEndTurnAvailability().bCanAcceptEndTurnIntent);
		}
		const EBufferedPlayerIntentEvaluation Evaluation = Input.EvaluatePending();
		TestTrue(TEXT("Stale intent retires before take"), Evaluation == EBufferedPlayerIntentEvaluation::Dropped
			|| (Case == 3 && Evaluation == EBufferedPlayerIntentEvaluation::None));
		FBufferedPlayerIntentDecision Decision;
		TestFalse(TEXT("Old EndTurn never reaches a later turn/decision"), Input.TakeReadyIntent(Decision));
		if (Case == 4)
		{
			Fixture.Gameplay.FlushReady();
			TestTrue(TEXT("Mandatory selection resolves through normal submission"),
				Fixture.Gameplay.ViewModel->SubmitPendingCardSelectionByRuntimeIds({Fixture.Card(TEXT("B"))->GetRuntimeId()}));
			Fixture.Gameplay.FlushReady();
			TestTrue(TEXT("Resolved mandatory decision cannot resurrect old EndTurn"), Input.EvaluatePending() == EBufferedPlayerIntentEvaluation::None);
		}
	}
	return true;
}

#endif
