#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2D5TestSupport.h"
#include "Phase6UIA2D5TestTypes.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2D5TerminalDefeatTest
{
	using namespace Phase6UIA2D5Test;

	int32 CountRecords(
		const FPresentationResolutionEnvelope& Envelope,
		EBattlePresentationRecordType Type
	)
	{
		int32 Count = 0;
		for (const FPresentationRecord& Record : Envelope.Records)
		{
			if (Record.Type == Type)
			{
				++Count;
			}
		}
		return Count;
	}

	bool AssertWorkingNonTerminal(
		FAutomationTestBase& Test,
		UBattlePresentationController* Controller,
		const FString& Context,
		FPresentationStateSnapshot* OutSnapshot = nullptr
	)
	{
		if (!IsValid(Controller))
		{
			Test.AddError(FString::Printf(TEXT("%s Controller is invalid."), *Context));
			return false;
		}

		FPresentationStateSnapshot Working;
		if (!Test.TestTrue(
			*FString::Printf(TEXT("%s WorkingSnapshot exists"), *Context),
			Controller->TryGetWorkingSnapshotForTesting(Working)
		))
		{
			return false;
		}

		bool bOk = true;
		bOk &= Test.TestTrue(
			*FString::Printf(TEXT("%s WorkingSnapshot remains PlayerTurn"), *Context),
			Working.BattleState == EBattleState::PlayerTurn
		);
		bOk &= Test.TestTrue(
			*FString::Printf(TEXT("%s WorkingSnapshot outcome remains None"), *Context),
			Working.Outcome == EBattleHUDOutcome::None
		);
		bOk &= Test.TestTrue(
			*FString::Printf(TEXT("%s WorkingSnapshot can still end turn historically"), *Context),
			Working.bCanEndTurn
		);

		if (OutSnapshot != nullptr)
		{
			*OutSnapshot = Working;
		}
		return bOk;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D5TerminalDefeatTest,
	"SlayTheSpireDemo.Phase6UIA2D5.Terminal.Defeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D5TerminalDefeatTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D5TerminalDefeatTest;

	FAcceptanceFixture Fixture;
	TArray<UCardData*> EmptyDeck;
	if (!TestTrue(
		TEXT("Defeat fixture starts with lethal committed enemy attack"),
		Fixture.Start(EmptyDeck, 0, 0, 100)
	))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	if (!TestNotNull(TEXT("Defeat deck runtime"), Deck))
	{
		return false;
	}

	TestTrue(TEXT("Defeat baseline Gameplay is PlayerTurn"), Fixture.Battle->BattleState == EBattleState::PlayerTurn);
	TestEqual(TEXT("Defeat baseline Gameplay Energy"), Fixture.Battle->Energy, 3);
	TestEqual(TEXT("Defeat baseline Gameplay Player HP"), Fixture.Player->HP, 100);
	TestEqual(TEXT("Defeat baseline Gameplay Player Block"), Fixture.Player->Block, 0);
	TestEqual(TEXT("Defeat baseline Hand empty"), Deck->GetHandCount(), 0);
	TestEqual(TEXT("Defeat baseline displayed Energy"), Fixture.ViewModel->Energy, 3);
	TestEqual(TEXT("Defeat baseline displayed Player HP"), Fixture.ViewModel->Player.HP, 100);
	TestTrue(TEXT("Defeat baseline displayed Outcome is None"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);

	const FGameplayRequestResult EndTurnResult = Fixture.Battle->RequestEndPlayerTurn();
	if (!TestTrue(TEXT("EndPlayerTurn request accepted"), EndTurnResult.IsAcceptedForResolution()))
	{
		return false;
	}
	Fixture.Flush();

	if (!TestEqual(TEXT("Lethal enemy turn publishes one Resolution Envelope"), Fixture.CapturedEnvelopes.Num(), 1))
	{
		return false;
	}

	const FCapturedEnvelope& Capture = Fixture.CapturedEnvelopes[0];
	const FPresentationResolutionEnvelope& Envelope = Capture.Envelope;
	TestTrue(TEXT("Defeat Envelope origin is EndTurn"), Envelope.Origin == EPresentationResolutionOrigin::EndTurn);
	if (!TestEqual(TEXT("Lethal enemy turn emits exactly three visible Records"), Envelope.Records.Num(), 3))
	{
		return false;
	}

	TestTrue(TEXT("Record[0] EnergyChanged"), Envelope.Records[0].Type == EBattlePresentationRecordType::EnergyChanged);
	TestTrue(TEXT("Record[1] Damage"), Envelope.Records[1].Type == EBattlePresentationRecordType::Damage);
	TestTrue(TEXT("Record[2] Defeat"), Envelope.Records[2].Type == EBattlePresentationRecordType::Defeat);
	TestTrue(TEXT("Defeat is final Record"), Envelope.Records.Last().Type == EBattlePresentationRecordType::Defeat);
	TestEqual(TEXT("Exactly one Defeat Record"), CountRecords(Envelope, EBattlePresentationRecordType::Defeat), 1);
	TestEqual(TEXT("No Victory Record"), CountRecords(Envelope, EBattlePresentationRecordType::Victory), 0);
	TestEqual(TEXT("No ResolutionFault Record"), CountRecords(Envelope, EBattlePresentationRecordType::ResolutionFault), 0);
	TestEqual(TEXT("No BlockChanged no-op Record"), CountRecords(Envelope, EBattlePresentationRecordType::BlockChanged), 0);
	TestEqual(TEXT("No DeckShuffled Record after terminal enemy damage"), CountRecords(Envelope, EBattlePresentationRecordType::DeckShuffled), 0);
	TestEqual(TEXT("No CardZoneChanged Record in empty-hand defeat flow"), CountRecords(Envelope, EBattlePresentationRecordType::CardZoneChanged), 0);

	const FPresentationRecord& EndEnergy = Envelope.Records[0];
	TestEqual(TEXT("EndTurn Energy before"), EndEnergy.EnergyChanged.EnergyBefore, 3);
	TestEqual(TEXT("EndTurn Energy after"), EndEnergy.EnergyChanged.EnergyAfter, 0);
	TestEqual(TEXT("EndTurn Energy delta"), EndEnergy.EnergyChanged.Delta, -3);

	const FPresentationRecord& Damage = Envelope.Records[1];
	TestEqual(TEXT("Lethal enemy Damage source"), Damage.Damage.SourcePresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("Lethal enemy Damage target"), Damage.Damage.TargetPresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Lethal enemy Damage incoming"), Damage.Damage.IncomingDamage, 100);
	TestEqual(TEXT("Lethal enemy Damage HP before"), Damage.Damage.HPBefore, 100);
	TestEqual(TEXT("Lethal enemy Damage HP after"), Damage.Damage.HPAfter, 0);
	TestEqual(TEXT("Lethal enemy Damage HP damage"), Damage.Damage.HPDamage, 100);
	TestEqual(TEXT("Lethal enemy Damage block before"), Damage.Damage.BlockBefore, 0);
	TestEqual(TEXT("Lethal enemy Damage block after"), Damage.Damage.BlockAfter, 0);
	TestEqual(TEXT("Lethal enemy Damage blocked amount"), Damage.Damage.BlockedDamage, 0);

	const FPresentationRecord& Defeat = Envelope.Records[2];
	TestEqual(TEXT("Defeat winner identity"), Defeat.Terminal.WinnerPresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("Defeat defeated identity"), Defeat.Terminal.DefeatedPresentationId, FName(TEXT("PlayerHero")));
	TestTrue(TEXT("Defeat FinalSnapshot BattleState"), Envelope.FinalSnapshot.BattleState == EBattleState::Defeat);
	TestTrue(TEXT("Defeat FinalSnapshot Outcome"), Envelope.FinalSnapshot.Outcome == EBattleHUDOutcome::Defeat);
	TestFalse(TEXT("Defeat FinalSnapshot cannot end turn"), Envelope.FinalSnapshot.bCanEndTurn);
	TestEqual(TEXT("Defeat FinalSnapshot Player HP"), Envelope.FinalSnapshot.Player.HP, 0);
	TestTrue(TEXT("Defeat FinalSnapshot Player dead"), Envelope.FinalSnapshot.Player.bDead);
	TestEqual(TEXT("Defeat FinalSnapshot Energy"), Envelope.FinalSnapshot.Energy, 0);
	TestEqual(TEXT("Defeat FinalSnapshot Hand empty"), Envelope.FinalSnapshot.HandCards.Num(), 0);
	TestEqual(TEXT("Defeat FinalSnapshot Draw count"), Envelope.FinalSnapshot.DrawCount, 0);
	TestEqual(TEXT("Defeat FinalSnapshot Discard count"), Envelope.FinalSnapshot.DiscardCount, 0);

	// Gameplay is already authoritative and terminal before Presentation catches up.
	TestTrue(TEXT("Gameplay commits Defeat immediately"), Fixture.Battle->BattleState == EBattleState::Defeat);
	TestEqual(TEXT("Gameplay Energy is zero"), Fixture.Battle->Energy, 0);
	TestTrue(TEXT("Gameplay Player is dead"), Fixture.Player->IsDead());
	TestEqual(TEXT("Gameplay Player HP is zero"), Fixture.Player->HP, 0);

	if (!TestTrue(TEXT("Controller starts EndTurn EnergyChanged playback"), Fixture.Controller->IsWaitingForCompletionForTesting())
		|| !TestEqual(TEXT("Only EndTurn EnergyChanged reached widget initially"), Fixture.Widget->PlayCallCount, 1))
	{
		return false;
	}

	FPresentationStateSnapshot Working;
	if (!AssertWorkingNonTerminal(*this, Fixture.Controller, TEXT("Before EndTurn Energy completion"), &Working))
	{
		return false;
	}
	TestEqual(TEXT("Working baseline Energy"), Working.Energy, 3);
	TestEqual(TEXT("Working baseline Player HP"), Working.Player.HP, 100);
	TestFalse(TEXT("Working baseline Player alive"), Working.Player.bDead);
	TestTrue(TEXT("Displayed outcome remains None before playback"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);

	if (!TestTrue(TEXT("Complete EndTurn EnergyChanged"), Fixture.CompleteCurrentPlayback()))
	{
		return false;
	}
	if (!AssertWorkingNonTerminal(*this, Fixture.Controller, TEXT("After EndTurn Energy completion"), &Working))
	{
		return false;
	}
	TestEqual(TEXT("Working Energy after EndTurn"), Working.Energy, 0);
	TestEqual(TEXT("Working Player HP unchanged before Damage"), Working.Player.HP, 100);
	TestEqual(TEXT("Displayed Energy after EndTurn"), Fixture.ViewModel->Energy, 0);
	TestEqual(TEXT("Displayed Player HP unchanged before Damage"), Fixture.ViewModel->Player.HP, 100);
	TestEqual(TEXT("Damage becomes second visible playback"), Fixture.Widget->PlayCallCount, 2);

	if (!TestTrue(TEXT("Complete lethal enemy Damage"), Fixture.CompleteCurrentPlayback()))
	{
		return false;
	}
	if (!AssertWorkingNonTerminal(*this, Fixture.Controller, TEXT("While Defeat is animating"), &Working))
	{
		return false;
	}
	TestEqual(TEXT("Working Player HP reaches zero"), Working.Player.HP, 0);
	TestTrue(TEXT("Working Player dead before Defeat terminal"), Working.Player.bDead);
	TestEqual(TEXT("Working Energy remains zero before Defeat terminal"), Working.Energy, 0);
	TestEqual(TEXT("Displayed Player HP reaches zero"), Fixture.ViewModel->Player.HP, 0);
	TestTrue(TEXT("Displayed Player dead before Defeat terminal"), Fixture.ViewModel->Player.bDead);
	TestTrue(TEXT("Displayed outcome remains None while Defeat animates"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);
	TestTrue(TEXT("Displayed interaction remains Resolving while Defeat animates"), Fixture.ViewModel->InteractionState == EBattleHUDInteractionState::Resolving);
	TestTrue(TEXT("Displayed input remains locked while Defeat animates"), Fixture.ViewModel->bInputLocked);
	TestFalse(TEXT("Displayed EndTurn remains disabled while Defeat animates"), Fixture.ViewModel->bCanEndTurn);
	TestEqual(TEXT("Defeat becomes third visible playback"), Fixture.Widget->PlayCallCount, 3);
	TestTrue(TEXT("Defeat token is waiting"), Fixture.Controller->IsWaitingForCompletionForTesting());

	const FPresentationPlaybackToken DefeatToken = Fixture.Controller->GetActivePlaybackTokenForTesting();
	TestEqual(TEXT("Defeat token BattleId"), DefeatToken.BattleId, Defeat.BattleId);
	TestEqual(TEXT("Defeat token ResolutionId"), DefeatToken.ResolutionId, Defeat.ResolutionId);
	TestEqual(TEXT("Defeat token PresentationSequence"), DefeatToken.PresentationSequence, Defeat.PresentationSequence);
	TestTrue(TEXT("Defeat token generation positive"), DefeatToken.LocalPlaybackGeneration > 0);

	if (!TestTrue(TEXT("Complete Defeat terminal playback"), Fixture.CompleteCurrentPlayback()))
	{
		return false;
	}
	if (!TestTrue(TEXT("Defeat playback fully drains"), Fixture.DrainPlayback()))
	{
		return false;
	}

	TestFalse(TEXT("Defeat completion clears wait"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Defeat completion clears backlog"), Fixture.Controller->GetBacklogCountForTesting(), 0);
	TestEqual(TEXT("Defeat Resolution completes"), Fixture.Controller->GetLastCompletedResolutionIdForTesting(), Envelope.ResolutionId);
	TestTrue(TEXT("Displayed outcome enters Defeat only after terminal completion"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::Defeat);
	TestTrue(TEXT("Displayed interaction enters Terminal"), Fixture.ViewModel->InteractionState == EBattleHUDInteractionState::Terminal);
	TestTrue(TEXT("Displayed input remains locked in Terminal"), Fixture.ViewModel->bInputLocked);
	TestFalse(TEXT("Displayed EndTurn disabled in Terminal"), Fixture.ViewModel->bCanEndTurn);
	TestEqual(TEXT("Displayed terminal Energy"), Fixture.ViewModel->Energy, 0);
	TestEqual(TEXT("Displayed terminal Player HP"), Fixture.ViewModel->Player.HP, 0);
	TestTrue(TEXT("Displayed terminal Player dead"), Fixture.ViewModel->Player.bDead);

	FPresentationStateSnapshot ReleasedWorking;
	TestFalse(
		TEXT("Caught-up Defeat Controller releases WorkingSnapshot"),
		Fixture.Controller->TryGetWorkingSnapshotForTesting(ReleasedWorking)
	);

	const int64 CompletedResolutionBeforeDuplicate = Fixture.Controller->GetLastCompletedResolutionIdForTesting();
	const int32 PlayCallsBeforeDuplicate = Fixture.Widget->PlayCallCount;
	Fixture.Controller->NotifyPresentationFinished(DefeatToken);
	TestEqual(
		TEXT("Duplicate Defeat token cannot advance completed resolution"),
		Fixture.Controller->GetLastCompletedResolutionIdForTesting(),
		CompletedResolutionBeforeDuplicate
	);
	TestEqual(
		TEXT("Duplicate Defeat token cannot start new playback"),
		Fixture.Widget->PlayCallCount,
		PlayCallsBeforeDuplicate
	);
	TestFalse(TEXT("Duplicate Defeat token cannot restore wait state"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestTrue(TEXT("Duplicate Defeat token leaves displayed Outcome unchanged"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::Defeat);

	TestTrue(
		TEXT("Defeat Envelope reducer-owned state matches FinalSnapshot"),
		AssertReducerOwnedStateMatchesFinalSnapshot(
			*this,
			Capture.Baseline,
			Envelope,
			TEXT("Terminal.Defeat")
		)
	);
	TestTrue(
		TEXT("Defeat captured Envelope order remains monotonic"),
		AssertCapturedEnvelopeOrder(*this, Fixture.CapturedEnvelopes, TEXT("Terminal.Defeat"))
	);
	TestTrue(
		TEXT("Defeat Controller playback exactly matches producer history"),
		AssertControllerPlaybackMatchesCapturedHistory(
			*this,
			Fixture.CapturedEnvelopes,
			Fixture.Widget,
			TEXT("Terminal.Defeat Controller")
		)
	);

	return true;
}

#endif
