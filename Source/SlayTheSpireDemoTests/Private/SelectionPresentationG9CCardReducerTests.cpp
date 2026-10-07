#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/PresentationCardReducer.h"
#include "Presentation/PresentationCardView.h"
#include "Presentation/BattlePresentationController.h"

namespace G9CardReducerTest
{
	struct FFixture
	{
		FPresentationStateSnapshot Snapshot;
		FCardPresentationHistoryState History;
		FPresentationSessionToken Session;
		FPresentationRecord Played, Destination;
		FFixture()
		{
			Snapshot.BattleId = 90; Snapshot.StateRevision = 1; Snapshot.Energy = Snapshot.MaxEnergy = 3;
			Snapshot.Player.PresentationId = TEXT("Player"); Snapshot.Enemy.PresentationId = TEXT("Enemy");
			Session.BattleId = 90; Session.ControllerEpoch = 5; Session.PresentationSessionGeneration = 2;
			Played.BattleId = 90; Played.ResolutionId = 7; Played.PresentationSequence = 10;
			Played.Type = EBattlePresentationRecordType::CardPlayed;
			auto& P = Played.CardPlayed; P.Card.RuntimeId = 1; P.Card.CardId = TEXT("A"); P.Card.DisplayName = FText::FromString(TEXT("A"));
			P.Card.Cost = 1; P.SourcePresentationId = TEXT("Player"); P.TargetPresentationId = TEXT("Enemy");
			P.HandIndexBefore = 0; P.PlayAreaIndexAfter = 0; P.EnergyBefore = 3; P.EnergyAfter = 2; P.CostPaid = 1;
			Snapshot.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(P.Card));
			Destination.BattleId = 90; Destination.ResolutionId = 7; Destination.PresentationSequence = 11;
			Destination.Type = EBattlePresentationRecordType::CardZoneChanged;
			auto& Z = Destination.CardZoneChanged; Z.Card = P.Card; Z.FromZone = ECardZone::PlayArea;
			Z.ToZone = ECardZone::DiscardPile; Z.FromIndex = 0; Z.ToIndex = 0;
		}
		bool Apply(const FPresentationRecord& Record, FPlayedCardPresentationLifecycleToken* Token = nullptr)
		{ return PresentationCardReducer::TryApplyRecord(Snapshot, History, Record, Session, Token); }
	};
}
using namespace G9CardReducerTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9CCanonicalTransactionTest, "SlayTheSpireDemo.SelectionPresentation.G9C.CardReducer.AtomicPreflightAndOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9CCanonicalTransactionTest::RunTest(const FString&)
{
	FFixture F; auto Candidate = F.Snapshot; auto CandidateHistory = F.History;
	FPlayedCardPresentationLifecycleToken Prepared, Committed, Consumed;
	TestTrue(TEXT("Pure preflight succeeds"), PresentationCardReducer::TryApplyRecord(Candidate, CandidateHistory, F.Played, F.Session, &Prepared));
	TestEqual(TEXT("Preflight leaves formal Hand intact"), F.Snapshot.HandCards.Num(), 1);
	TestEqual(TEXT("Preflight leaves generation intact"), F.History.NextLifecycleGeneration, int64(1));
	TestTrue(TEXT("Formal played commit"), F.Apply(F.Played, &Committed));
	TestTrue(TEXT("Prepared occurrence exactly equals formal occurrence"), Prepared == Committed && Committed.IsValid());
	TestEqual(TEXT("Cost paid once"), F.Snapshot.Energy, 2);
	TestFalse(TEXT("Duplicate CardPlayed cannot commit"), F.Apply(F.Played));
	TestTrue(TEXT("Exact destination consumes occurrence"), F.Apply(F.Destination, &Consumed));
	TestTrue(TEXT("Destination returns exact played occurrence"), Consumed == Committed);
	TestEqual(TEXT("Destination increments pile once"), F.Snapshot.DiscardCount, 1);
	TestEqual(TEXT("Formal association consumed once"), F.History.PendingPlays.Num(), 0);
	TestFalse(TEXT("Repeated destination rejected"), F.Apply(F.Destination));
	TestEqual(TEXT("Failed repeat has zero formal effect"), F.Snapshot.DiscardCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9CCanonicalValidationTest, "SlayTheSpireDemo.SelectionPresentation.G9C.CardReducer.IdentityEnergyAndIndices", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9CCanonicalValidationTest::RunTest(const FString&)
{
	for (int32 Mode = 0; Mode < 12; ++Mode)
	{
		FFixture F; auto Bad = F.Played; auto& P = Bad.CardPlayed;
		switch (Mode)
		{
		case 0: P.Card.CardId = TEXT("Wrong"); break;
		case 1: P.HandIndexBefore = 1; break;
		case 2: P.PlayAreaIndexAfter = 1; break;
		case 3: P.EnergyBefore = 4; break;
		case 4: P.EnergyAfter = -1; break;
		case 5: P.CostPaid = 0; break;
		case 6: P.SourcePresentationId = TEXT("Unknown"); break;
		case 7: P.TargetPresentationId = TEXT("Unknown"); break;
		case 8: P.Card.CardType = static_cast<ECardType>(255); break;
		case 9: { const auto Duplicate = F.Snapshot.HandCards[0]; F.Snapshot.HandCards.Add(Duplicate); break; }
		case 10: Bad.BattleId = 91; break;
		case 11: Bad.PresentationSequence = 0; break;
		}
		const int32 HandCount = F.Snapshot.HandCards.Num();
		TestFalse(TEXT("Malformed played record rejected"), F.Apply(Bad));
		TestEqual(TEXT("Rejection cannot remove Hand"), F.Snapshot.HandCards.Num(), HandCount);
		TestEqual(TEXT("Rejection cannot pay cost"), F.Snapshot.Energy, 3);
		TestEqual(TEXT("Rejection cannot allocate occurrence"), F.History.NextLifecycleGeneration, int64(1));
	}
	for (int32 Mode = 0; Mode < 6; ++Mode)
	{
		FFixture F; F.Apply(F.Played); auto Bad = F.Destination;
		if (Mode == 0) Bad.CardZoneChanged.ToIndex = 1;
		else if (Mode == 1) Bad.CardZoneChanged.FromIndex = 1;
		else if (Mode == 2) Bad.CardZoneChanged.Card.CardId = TEXT("Wrong");
		else if (Mode == 3) Bad.CardZoneChanged.ToZone = ECardZone::Hand;
		else if (Mode == 4) Bad.ResolutionId = 6;
		else Bad.PresentationSequence = 10;
		TestFalse(TEXT("Invalid destination rejected atomically"), F.Apply(Bad));
		TestEqual(TEXT("Invalid destination keeps association"), F.History.PendingPlays.Num(), 1);
		TestEqual(TEXT("Invalid destination keeps pile"), F.Snapshot.DiscardCount, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9CLifecycleCorrelationTest, "SlayTheSpireDemo.SelectionPresentation.G9C.CardReducer.LifecycleIsolationAndResume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9CLifecycleCorrelationTest::RunTest(const FString&)
{
	for (int32 Mode = 0; Mode < 5; ++Mode)
	{
		FFixture F; F.Apply(F.Played);
		if (Mode == 0) F.History.ClearCorrelations();
		else if (Mode == 1) { const auto Duplicate = F.History.PendingPlays[0]; F.History.PendingPlays.Add(Duplicate); }
		else if (Mode == 2) ++F.Session.PresentationSessionGeneration;
		else if (Mode == 3) ++F.History.PendingPlays[0].Token.BattleId;
		else F.History.PendingPlays[0].Token.CardId = TEXT("Wrong");
		TestFalse(TEXT("Missing/duplicate/wrong association rejected"), F.Apply(F.Destination));
		TestEqual(TEXT("No destination mutation"), F.Snapshot.DiscardCount, 0);
	}
	FFixture F; FPlayedCardPresentationLifecycleToken Old, Destination;
	TestTrue(TEXT("Play before mandatory choice"), F.Apply(F.Played, &Old));
	F.Destination.ResolutionId = 8; // mandatory choice continuation is a later sealed envelope
	TestTrue(TEXT("Resumed destination preserves original occurrence"), F.Apply(F.Destination, &Destination));
	TestTrue(TEXT("Resume does not mint another occurrence"), Destination == Old);
	F.Snapshot.DrawCount = 1;
	FPresentationRecord Draw = F.Destination; Draw.PresentationSequence = 12;
	Draw.CardZoneChanged.FromZone = ECardZone::DrawPile; Draw.CardZoneChanged.ToZone = ECardZone::Hand;
	Draw.CardZoneChanged.FromIndex = Draw.CardZoneChanged.ToIndex = 0;
	TestTrue(TEXT("Same instance reappears after destination"), F.Apply(Draw));
	F.Played.ResolutionId = 9; F.Played.PresentationSequence = 13; F.Played.CardPlayed.EnergyBefore = 2; F.Played.CardPlayed.EnergyAfter = 1;
	FPlayedCardPresentationLifecycleToken New;
	TestTrue(TEXT("Reappeared instance can play again"), F.Apply(F.Played, &New));
	TestTrue(TEXT("Old occurrence cannot ABA-match new play"), New != Old && New.LocalLifecycleGeneration > Old.LocalLifecycleGeneration);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FG9CNoWidgetAndPreviewTest, "SlayTheSpireDemo.SelectionPresentation.G9C.CardReducer.NoWidgetAndFrozenPreview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FG9CNoWidgetAndPreviewTest::RunTest(const FString&)
{
	FFixture F; F.Session = {};
	F.Played.CardPlayed.Card.RichDescription = FText::FromString(TEXT("Frozen current-target preview"));
	FPlayedCardPresentationLifecycleToken Token;
	TestTrue(TEXT("No-Widget reduction still correlates played record"), F.Apply(F.Played, &Token));
	TestTrue(TEXT("History-only occurrence has exact identity"), Token.HasHistoricalIdentity());
	TestFalse(TEXT("History-only occurrence cannot authorize a visual"), Token.IsValid());
	TestTrue(TEXT("Destination can use original card face after preview"), F.Apply(F.Destination));
	FFixture G;
	FPresentationResolutionEnvelope Envelope; Envelope.BattleId = 90; Envelope.ResolutionId = 7;
	Envelope.Records = {G.Played,G.Destination}; Envelope.FinalSnapshot = G.Snapshot;
	auto* Controller = NewObject<UBattlePresentationController>(); FPresentationStateSnapshot Result;
	TestTrue(TEXT("Controller dry-run uses canonical semantics"), Controller->ReduceEnvelopeForTesting(G.Snapshot,Envelope,Result));
	TestEqual(TEXT("Controller dry-run preserves source snapshot"), G.Snapshot.HandCards.Num(), 1);
	TestEqual(TEXT("Controller dry-run result matches production"), Result.DiscardCount, 1);
	FPresentationStateSnapshot Absent;
	TestFalse(TEXT("Dry-run did not replace formal Controller state"), Controller->TryGetWorkingSnapshotForTesting(Absent));
	return true;
}
#endif
