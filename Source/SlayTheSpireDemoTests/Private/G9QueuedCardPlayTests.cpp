#include "SelectionPresentationG9TestFixture.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Containers/Ticker.h"
#include "Cards/Effects/DrawCardEffect.h"
using namespace SelectionPresentationG9ATest;

namespace
{
	void Drain(FShadowFixture& F)
	{
		for (int32 Step = 0; Step < 8; ++Step)
		{ F.Gameplay.FlushReady(); F.FinishPlayback(); FTSTicker::GetCoreTicker().Tick(0.0f); }
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9QueueVisualOriginTest, "SlayTheSpireDemo.SelectionPresentation.G9B.Queue.VisualOriginReceipt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9QueueVisualOriginTest::RunTest(const FString&)
{
	FShadowFixture F; F.Widget->SetBufferedPlayerInputEnabled(true);
	if (!TestTrue(TEXT("A Blocking"), F.PlayA())) return false;
	const int32 B = F.Card(TEXT("B"))->GetRuntimeId();
	TestTrue(TEXT("B draft"), F.Widget->SelectCard(B));
	FCardPlayVisualOrigin Origin; Origin.Center = FVector2D(.3,.4); Origin.Size = FVector2D(172.5,241.5); Origin.bPointerHeld = true;
	TestTrue(TEXT("UI receipt travels with confirmed intent"), F.Input().ConfirmCardDraft(NAME_None, Origin));
	TestTrue(TEXT("Confirmation preserves exact origin"), F.Input().GetConfirmedPlays()[0].VisualOrigin->Center == Origin.Center);
	F.FinishPlayback(); F.Gameplay.FlushReady();
	FBufferedPlayerIntentDecision Decision;
	if (!TestTrue(TEXT("Receipt delivered only at ready boundary"), F.Input().TakeReadyIntent(Decision))) return false;
	TestTrue(TEXT("Popped command retains origin and input identity"), Decision.Play.InputSequence != 0 && Decision.Play.VisualOrigin->Center == Origin.Center);
	TestTrue(TEXT("Busy retry retains same receipt"), F.Input().RestoreBusyPlay(Decision.Play));
	F.Input().Clear();
	TestFalse(TEXT("Old receipt cannot restore a cleared generation"), F.Input().RestoreBusyPlay(Decision.Play));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9FullTargetTest, "SlayTheSpireDemo.SelectionPresentation.G9B.Queue.FrozenTargets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9FullTargetTest::RunTest(const FString&)
{
	for (auto Type : {ECardTargetType::Enemy, ECardTargetType::Self, ECardTargetType::None})
	{
		FShadowFixture F(true, false, Type);
		F.Widget->SetBufferedPlayerInputEnabled(true);
		if (!TestTrue(TEXT("A Blocking"), F.PlayA())) return false;
		const auto Token = F.Controller->GetActivePlaybackTokenForTesting();
		const int32 Id = F.Card(TEXT("B"))->GetRuntimeId();
		TestTrue(TEXT("Draft accepted while Resolving"), F.Widget->SelectCard(Id));
		TestTrue(TEXT("Draft does not fake Gameplay readiness"), F.Gameplay.ViewModel->bInputLocked);
		TestFalse(TEXT("Wrong target never queues"), F.Widget->SelectTarget(77));
		const bool bConfirmed = Type == ECardTargetType::None ? F.Widget->ConfirmSelectedCard() : F.Widget->SelectTarget(Type == ECardTargetType::Self ? 1 : 2);
		TestTrue(TEXT("Complete confirmation"), bConfirmed);
		TestFalse(TEXT("Repeated confirmation"), F.Widget->ConfirmSelectedCard());
		TestEqual(TEXT("One confirmed command"), F.Input().GetConfirmedPlayCount(), 1);
		TestTrue(TEXT("No Skip or early request"), F.Controller->GetActivePlaybackTokenForTesting() == Token && F.Card(TEXT("B")) != nullptr);
		int32 AcceptedEdges = 0;
		const auto Handle = F.Gameplay.ViewModel->OnNativeChanged.AddLambda([&](EBattleHUDDirtyFlags)
		{
			if (!F.Card(TEXT("B"))) { ++AcceptedEdges; TestEqual(TEXT("Pop before publication"), F.Input().GetConfirmedPlayCount(), 0); }
			F.Widget->NotifyBufferedPlayerInputReadinessChanged();
		});
		Drain(F);
		F.Gameplay.ViewModel->OnNativeChanged.Remove(Handle);
		TestTrue(TEXT("Exact target command executes once"), F.Card(TEXT("B")) == nullptr);
		TestEqual(TEXT("Exactly two destinations"), F.Gameplay.Battle->GetDeckRuntimeForTesting()->GetDiscardCount(), 2);
		TestTrue(TEXT("Publication observed"), AcceptedEdges > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9QueueEndTurnTest, "SlayTheSpireDemo.SelectionPresentation.G9B.Queue.EndTurnCancelsConfirmed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9QueueEndTurnTest::RunTest(const FString&)
{
	FShadowFixture F;
	F.Widget->SetBufferedPlayerInputEnabled(true);
	if (!TestTrue(TEXT("A Blocking"), F.PlayA())) return false;
	TestTrue(TEXT("B draft"), F.Widget->SelectCard(F.Card(TEXT("B"))->GetRuntimeId()));
	TestTrue(TEXT("B confirmed"), F.Widget->ConfirmSelectedCard());
	const int32 CId = F.Card(TEXT("C"))->GetRuntimeId();
	TestTrue(TEXT("C unconfirmed draft"), F.Widget->SelectCard(CId));
	int32 LaterCardPlays = 0;
	const auto Capture = F.Gameplay.Battle->OnPresentationResolutionReady.AddLambda([&](const auto& Envelope)
	{
		for (const auto& Record : Envelope.Records)
			if (Record.Type == EBattlePresentationRecordType::CardPlayed && Record.CardPlayed.Card.CardId != TEXT("A")) ++LaterCardPlays;
	});
	TestTrue(TEXT("EndTurn accepted"), F.Widget->EndTurn());
	TestEqual(TEXT("Draft retired"), F.Widget->GetBufferedCardDraftRuntimeId(), INDEX_NONE);
	TestEqual(TEXT("Confirmed B cancelled"), F.Input().GetConfirmedPlayCount(), 0);
	TestFalse(TEXT("No additions after EndTurn"), F.Widget->SelectCard(CId));
	F.FinishPlayback(); FTSTicker::GetCoreTicker().Tick(0.0f);
	FPlayerTurnAuthorityToken Turn;
	F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn);
	TestEqual(TEXT("Only one EndTurn request"), Turn.PlayerTurnSerial, uint64(2));
	// B/C are discarded by EndTurn, not played. No second PlayArea destination.
	TestEqual(TEXT("Current A plus discarded B/C, no queued play"), F.Gameplay.Battle->GetDeckRuntimeForTesting()->GetDiscardCount(), 3);
	Drain(F);
	F.Gameplay.Battle->OnPresentationResolutionReady.Remove(Capture);
	TestEqual(TEXT("Cancelled B/C produce no CardPlayed"), LaterCardPlays, 0);
	F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn);
	TestEqual(TEXT("EndTurn executes exactly once afterwards"), Turn.PlayerTurnSerial, uint64(2));
	TestFalse(TEXT("EndTurn consumed"), F.Widget->HasAcceptedBufferedEndTurn());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9QueueIsolationTest, "SlayTheSpireDemo.SelectionPresentation.G9B.Queue.Isolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9QueueIsolationTest::RunTest(const FString&)
{
	for (int32 Mode = 0; Mode < 5; ++Mode)
	{
		FShadowFixture F(true, false, ECardTargetType::None, Mode == 0);
		F.Widget->SetBufferedPlayerInputEnabled(true);
		if (!TestTrue(TEXT("A Blocking"), F.PlayA())) return false;
		F.Widget->SelectCard(F.Card(TEXT("B"))->GetRuntimeId()); F.Widget->ConfirmSelectedCard();
		F.Widget->SelectCard(F.Card(TEXT("C"))->GetRuntimeId()); F.Widget->ConfirmSelectedCard();
		if (Mode == 0)
		{
			F.FinishPlayback(); FTSTicker::GetCoreTicker().Tick(0.0f); F.Gameplay.FlushReady();
			TestTrue(TEXT("B enters mandatory choice"), F.Gameplay.ViewModel->HasAuthoritativePendingCardSelection());
			TestEqual(TEXT("Mandatory clears future C"), F.Input().GetConfirmedPlayCount(), 0);
			TestFalse(TEXT("Mandatory stores no EndTurn"), F.Widget->HasAcceptedBufferedEndTurn());
			TestFalse(TEXT("Mandatory cannot end turn"), F.Widget->EndTurn());
			Drain(F);
			TestTrue(TEXT("Complete mandatory through normal API"), F.Gameplay.ViewModel->SubmitPendingCardSelectionByRuntimeIds({F.Card(TEXT("C"))->GetRuntimeId()}));
		}
		else if (Mode == 1) F.Widget->SetBufferedPlayerInputEnabled(false);
		else if (Mode == 2) F.Widget->SkipPresentation();
		else if (Mode == 3) F.Widget->SetPresentationController(nullptr);
		else F.Gameplay.Battle->StartBattle();
		Drain(F);
		TestEqual(TEXT("No stale queue restored"), F.Input().GetConfirmedPlayCount(), 0);
		TestEqual(TEXT("No stale draft restored"), F.Widget->GetBufferedCardDraftRuntimeId(), INDEX_NONE);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9EndTurnRepeatFenceTest, "SlayTheSpireDemo.SelectionPresentation.G9B.Queue.EndTurnRepeatFence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9EndTurnRepeatFenceTest::RunTest(const FString&)
{
	FShadowFixture F;
	F.Widget->SetBufferedPlayerInputEnabled(true);
	if (!TestTrue(TEXT("A remains in Blocking history"), F.PlayA())) return false;
	F.Widget->SelectCard(F.Card(TEXT("B"))->GetRuntimeId()); F.Widget->ConfirmSelectedCard();
	F.Widget->SelectCard(F.Card(TEXT("C"))->GetRuntimeId()); F.Widget->ConfirmSelectedCard();
	int32 ReentrantChecks = 0;
	const auto Handle = F.Gameplay.ViewModel->OnNativeChanged.AddLambda([&](EBattleHUDDirtyFlags)
	{
		FPlayerTurnAuthorityToken Turn;
		if (F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn) && Turn.PlayerTurnSerial == 2
			&& F.Gameplay.ViewModel->bInputLocked)
		{ ++ReentrantChecks; TestFalse(TEXT("Publication cannot accept a future-turn click"), F.Widget->EndTurn()); }
	});
	TestTrue(TEXT("One EndTurn accepted"), F.Widget->EndTurn());
	F.Gameplay.FlushReady();
	for (int32 Click = 0; Click < 20; ++Click)
	{
		TestFalse(TEXT("Repeated clicks during old history are rejected"), F.Widget->EndTurn());
		TestFalse(TEXT("Button remains unavailable after pending storage is consumed"), F.Widget->CanAcceptEndTurnIntent());
	}
	F.Widget->DiscardQueuedPlayerInput();
	TestFalse(TEXT("Pending-input cleanup cannot release a submitted-turn fence"), F.Widget->EndTurn());
	FPlayerTurnAuthorityToken Turn; F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn);
	TestEqual(TEXT("No chained player turns"), Turn.PlayerTurnSerial, uint64(2));
	TestEqual(TEXT("All unsubmitted cards cleared"), F.Input().GetConfirmedPlayCount(), 0);
	Drain(F);
	F.Gameplay.ViewModel->OnNativeChanged.Remove(Handle);
	TestTrue(TEXT("Synchronous publication was checked"), ReentrantChecks > 0);
	F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn);
	TestEqual(TEXT("Drain does not resurrect another EndTurn"), Turn.PlayerTurnSerial, uint64(2));
	TestTrue(TEXT("New displayed ready turn permits a fresh click"), F.Widget->CanAcceptEndTurnIntent());
	TestTrue(TEXT("Fresh next-turn click accepted"), F.Widget->EndTurn());
	F.Gameplay.Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn);
	TestEqual(TEXT("Only the fresh click ends the next turn"), Turn.PlayerTurnSerial, uint64(3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9EndTurnReceiptTest, "SlayTheSpireDemo.SelectionPresentation.G9B.Queue.EndTurnReceiptAndBusyRetirement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9EndTurnReceiptTest::RunTest(const FString&)
{
	FShadowFixture F(false); F.Widget->SetBufferedPlayerInputEnabled(true);
	F.Widget->SelectCard(F.Card(TEXT("B"))->GetRuntimeId());
	F.Input().ConfirmCardDraft(NAME_None);
	FBufferedPlayerIntentDecision Play;
	TestTrue(TEXT("Pop a not-yet-submitted card"), F.Input().TakeReadyIntent(Play));
	TestTrue(TEXT("EndTurn accepts before retirement"), F.Input().TryAcceptEndTurn(false).bAccepted);
	TestFalse(TEXT("Busy card cannot restore after EndTurn acceptance"), F.Input().RestoreBusyPlay(Play.Play));
	F.Input().RetireRejectedPlayAttempt();
	FBufferedPlayerIntentDecision First;
	TestTrue(TEXT("Take exact EndTurn receipt"), F.Input().TakeReadyIntent(First));
	TestFalse(TEXT("Consumed same-turn receipt still rejects duplicates"), F.Input().EvaluateEndTurnAvailability().bCanAcceptEndTurnIntent);
	F.Input().Clear();
	TestFalse(TEXT("Clear keeps submitted receipt"), F.Input().EvaluateEndTurnAvailability().bCanAcceptEndTurnIntent);
	F.Input().CompleteEndTurnSubmission(First.EndTurn, false);
	TestTrue(TEXT("Exact rejected submission allows retry"), F.Input().TryAcceptEndTurn(false).bAccepted);
	FBufferedPlayerIntentDecision Second; F.Input().TakeReadyIntent(Second);
	F.Input().CompleteEndTurnSubmission(First.EndTurn, false);
	TestFalse(TEXT("Old rejection cannot retire the new receipt"), F.Input().EvaluateEndTurnAvailability().bCanAcceptEndTurnIntent);
	F.Input().CompleteEndTurnSubmission(Second.EndTurn, false);
	TestTrue(TEXT("Current rejection releases its own receipt"), F.Input().EvaluateEndTurnAvailability().bCanAcceptEndTurnIntent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9QueueInvalidSkipTest, "SlayTheSpireDemo.SelectionPresentation.G9B.Queue.InvalidSkip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9QueueInvalidSkipTest::RunTest(const FString&)
{
	FShadowFixture F;
	F.Widget->SetBufferedPlayerInputEnabled(true);
	// Definitions are authored before the next battle; B costs more than all
	// available energy, C remains free. Queue capture reserves neither resource.
	for (auto& Definition : F.Gameplay.Battle->DebugStartingDeck) if (Definition->CardId == TEXT("B")) Definition->BaseCost = 99;
	F.Gameplay.Battle->StartBattle(); F.Gameplay.FlushReady(); F.FinishPlayback();
	if (!TestTrue(TEXT("A Blocking"), F.PlayA())) return false;
	F.Widget->SelectCard(F.Card(TEXT("B"))->GetRuntimeId()); TestTrue(TEXT("Cost is revalidated at consumption"), F.Widget->ConfirmSelectedCard());
	F.Widget->SelectCard(F.Card(TEXT("C"))->GetRuntimeId()); F.Widget->ConfirmSelectedCard();
	bool bFeedback = false;
	const auto Handle = F.Gameplay.ViewModel->OnNativeChanged.AddLambda([&](EBattleHUDDirtyFlags Flags)
	{ if (EnumHasAnyFlags(Flags, EBattleHUDDirtyFlags::Feedback) && !F.Gameplay.ViewModel->LastFeedback.IsEmpty()) bFeedback = true; });
	Drain(F); F.Gameplay.ViewModel->OnNativeChanged.Remove(Handle);
	TestTrue(TEXT("Insufficient B skipped"), F.Card(TEXT("B")) != nullptr);
	TestTrue(TEXT("Later C still executes"), F.Card(TEXT("C")) == nullptr);
	TestTrue(TEXT("Visible rejection feedback published"), bFeedback);
	TestFalse(TEXT("C does not erase skipped feedback"), F.Gameplay.ViewModel->LastFeedback.IsEmpty());
	TestEqual(TEXT("No debit for rejected B"), F.Gameplay.Battle->Energy, F.Gameplay.Battle->MaxEnergy);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9QueueRemovedAndDeadTest, "SlayTheSpireDemo.SelectionPresentation.G9B.Queue.RemovedAndDeadTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9QueueRemovedAndDeadTest::RunTest(const FString&)
{
	for (bool bDeadTarget : {false, true})
	{
		FShadowFixture F(true, false, ECardTargetType::Enemy);
		F.Widget->SetBufferedPlayerInputEnabled(true);
		if (!TestTrue(TEXT("A Blocking"), F.PlayA())) return false;
		F.Widget->SelectCard(F.Card(TEXT("B"))->GetRuntimeId());
		TestTrue(TEXT("B target captured"), F.Widget->SelectTarget(2));
		F.Widget->SelectCard(F.Card(TEXT("C"))->GetRuntimeId()); F.Widget->ConfirmSelectedCard();
		if (bDeadTarget) F.Gameplay.Enemy->HP = 0;
		else
		{
			TestTrue(TEXT("External request removes queued B"), F.Gameplay.Battle->RequestPlayCard(F.Card(TEXT("B")), F.Gameplay.Enemy).IsAcceptedForResolution());
			F.Gameplay.FlushReady();
		}
		Drain(F);
		TestTrue(TEXT("Removed item skips; dead sole enemy ends battle and clears all"), bDeadTarget ? F.Card(TEXT("C")) != nullptr : F.Card(TEXT("C")) == nullptr);
		TestEqual(TEXT("FIFO consumed"), F.Input().GetConfirmedPlayCount(), 0);
		if (bDeadTarget) TestTrue(TEXT("Invalid target never retargets B"), F.Card(TEXT("B")) != nullptr);
		else TestEqual(TEXT("B never submitted twice"), F.Gameplay.Battle->GetDeckRuntimeForTesting()->GetDiscardCount(), 3);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9QueueClearFenceTest, "SlayTheSpireDemo.SelectionPresentation.G9B.Queue.ClearFence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9QueueClearFenceTest::RunTest(const FString&)
{
	FShadowFixture F;
	F.Widget->SetBufferedPlayerInputEnabled(true);
	TestTrue(TEXT("Draft"), F.Input().BeginCardDraft(F.Card(TEXT("B"))->GetRuntimeId()));
	TestTrue(TEXT("Confirm"), F.Input().ConfirmCardDraft(NAME_None));
	FBufferedPlayerIntentDecision Taken;
	TestTrue(TEXT("Exact ready item popped"), F.Input().TakeReadyIntent(Taken));
	F.Widget->DiscardQueuedPlayerInput();
	TestFalse(TEXT("Old busy result cannot restore across clear with same Session/turn"), F.Input().RestoreBusyPlay(Taken.Play));
	TestEqual(TEXT("Old item remains retired"), F.Input().GetConfirmedPlayCount(), 0);
	return true;
}
#endif
