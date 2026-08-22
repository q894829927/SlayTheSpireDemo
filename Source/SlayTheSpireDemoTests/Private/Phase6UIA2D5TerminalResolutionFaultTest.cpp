#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2D5TestSupport.h"
#include "Phase6UIA2D5TestTypes.h"
#include "Actions/BattleAction.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/TurnEndedAction.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2D5TerminalResolutionFaultTest
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
			*FString::Printf(TEXT("%s WorkingSnapshot keeps historical can-end-turn"), *Context),
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
	FPhase6UIA2D5TerminalResolutionFaultTest,
	"SlayTheSpireDemo.Phase6UIA2D5.Terminal.ResolutionFault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D5TerminalResolutionFaultTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D5TerminalResolutionFaultTest;

	{
		FAcceptanceFixture Fixture;
		TArray<UCardData*> EmptyDeck;
		if (!TestTrue(
			TEXT("ResolutionFault fixture starts in a stable PlayerTurn"),
			Fixture.Start(EmptyDeck, 0, 0, 0)
		))
		{
			return false;
		}

		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		if (!TestNotNull(TEXT("ResolutionFault ActionQueue"), Queue))
		{
			return false;
		}

		TestTrue(TEXT("Fault baseline Gameplay is PlayerTurn"), Fixture.Battle->BattleState == EBattleState::PlayerTurn);
		TestEqual(TEXT("Fault baseline Gameplay Energy"), Fixture.Battle->Energy, 3);
		TestEqual(TEXT("Fault baseline Player HP"), Fixture.Player->HP, 100);
		TestEqual(TEXT("Fault baseline displayed Energy"), Fixture.ViewModel->Energy, 3);
		TestTrue(TEXT("Fault baseline displayed Outcome is None"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);

		AddExpectedErrorPlain(
			TEXT("[Battle] StartEnemyTurn failed to enqueue the atomic enemy Intent action batch."),
			EAutomationExpectedErrorFlags::Contains,
			1
		);
		AddExpectedErrorPlain(
			TEXT("[ActionQueue] Resolution fault requested:"),
			EAutomationExpectedErrorFlags::Contains,
			1
		);
		AddExpectedErrorPlain(
			TEXT("[ActionQueue] Resolution faulted."),
			EAutomationExpectedErrorFlags::Contains,
			1
		);
		AddExpectedErrorPlain(
			TEXT("[Battle] Resolution faulted."),
			EAutomationExpectedErrorFlags::Contains,
			1
		);

		// Force a genuine framework structural failure. The authoritative player
		// EndTurn batch remains valid and commits first. Only the deferred EnemyTurn
		// batch is malformed by giving TurnEndedAction the wrong Outer, so the real
		// ActionQueue atomic insertion rejects it and BattleManager requests a fault.
		Fixture.Battle->SetForceInvalidEnemyTurnBatchForTesting(true);
		const FGameplayRequestResult EndTurnResult = Fixture.Battle->RequestEndPlayerTurn();
		if (!TestTrue(TEXT("Faulting EndPlayerTurn request is accepted for resolution"), EndTurnResult.IsAcceptedForResolution()))
		{
			return false;
		}
		Fixture.Flush();

		if (!TestEqual(TEXT("Framework fault publishes one Resolution Envelope"), Fixture.CapturedEnvelopes.Num(), 1))
		{
			return false;
		}

		const FCapturedEnvelope& Capture = Fixture.CapturedEnvelopes[0];
		const FPresentationResolutionEnvelope& Envelope = Capture.Envelope;
		TestTrue(TEXT("Fault Envelope origin is EndTurn"), Envelope.Origin == EPresentationResolutionOrigin::EndTurn);
		if (!TestEqual(TEXT("Fault Resolution emits exactly two visible Records"), Envelope.Records.Num(), 2))
		{
			return false;
		}

		TestTrue(TEXT("Record[0] EnergyChanged"), Envelope.Records[0].Type == EBattlePresentationRecordType::EnergyChanged);
		TestTrue(TEXT("Record[1] ResolutionFault"), Envelope.Records[1].Type == EBattlePresentationRecordType::ResolutionFault);
		TestTrue(TEXT("ResolutionFault is final Record"), Envelope.Records.Last().Type == EBattlePresentationRecordType::ResolutionFault);
		TestEqual(TEXT("Exactly one ResolutionFault Record"), CountRecords(Envelope, EBattlePresentationRecordType::ResolutionFault), 1);
		TestEqual(TEXT("No Victory Record"), CountRecords(Envelope, EBattlePresentationRecordType::Victory), 0);
		TestEqual(TEXT("No Defeat Record"), CountRecords(Envelope, EBattlePresentationRecordType::Defeat), 0);
		TestEqual(TEXT("No Damage Record from atomically rejected EnemyTurn batch"), CountRecords(Envelope, EBattlePresentationRecordType::Damage), 0);
		TestEqual(TEXT("No BlockChanged Record"), CountRecords(Envelope, EBattlePresentationRecordType::BlockChanged), 0);
		TestEqual(TEXT("No CardZoneChanged Record"), CountRecords(Envelope, EBattlePresentationRecordType::CardZoneChanged), 0);
		TestEqual(TEXT("No DeckShuffled Record"), CountRecords(Envelope, EBattlePresentationRecordType::DeckShuffled), 0);
		TestEqual(TEXT("No StatusChanged Record"), CountRecords(Envelope, EBattlePresentationRecordType::StatusChanged), 0);

		const FPresentationRecord& EndEnergy = Envelope.Records[0];
		TestEqual(TEXT("Fault EndTurn Energy before"), EndEnergy.EnergyChanged.EnergyBefore, 3);
		TestEqual(TEXT("Fault EndTurn Energy after"), EndEnergy.EnergyChanged.EnergyAfter, 0);
		TestEqual(TEXT("Fault EndTurn Energy delta"), EndEnergy.EnergyChanged.Delta, -3);

		const FPresentationRecord& Fault = Envelope.Records[1];
		const FString ExpectedReason = TEXT("Enemy turn batch insertion failed before EnemyTurn state commit.");
		TestEqual(TEXT("Fault reason is exact framework diagnostic"), Fault.ResolutionFault.Reason, ExpectedReason);
		TestEqual(TEXT("Queue retains exact framework fault reason"), Queue->GetResolutionFaultReason(), ExpectedReason);
		TestEqual(
			TEXT("Fault executed count freezes authoritative Queue diagnostic"),
			Fault.ResolutionFault.ExecutedActionCount,
			Queue->GetExecutedCountInResolution()
		);

		UBattleAction* LastExecutedAction = Queue->GetLastExecutedAction();
		if (!TestNotNull(TEXT("Framework fault retains last executed Action"), LastExecutedAction))
		{
			return false;
		}
		TestTrue(TEXT("Last executed Action is the real player TurnEndedAction"), LastExecutedAction->IsA<UTurnEndedAction>());
		TestEqual(
			TEXT("Fault LastActionName freezes authoritative Queue identity"),
			Fault.ResolutionFault.LastActionName,
			LastExecutedAction->GetFName()
		);

		TestTrue(TEXT("Gameplay enters ResolutionFaulted"), Fixture.Battle->BattleState == EBattleState::ResolutionFaulted);
		TestTrue(TEXT("Queue enters ResolutionFaulted"), Queue->IsResolutionFaulted());
		TestEqual(
			TEXT("Fault occurs before EnemyTurn state commit"),
			Fixture.Battle->GetStateBeforeLastResolutionFaultForTesting(),
			EBattleState::PlayerTurnEnding
		);
		TestEqual(TEXT("Fault normalizes Gameplay Energy to zero"), Fixture.Battle->Energy, 0);
		TestEqual(TEXT("Rejected EnemyTurn batch cannot damage Player"), Fixture.Player->HP, 100);
		TestFalse(TEXT("Rejected EnemyTurn batch cannot kill Player"), Fixture.Player->IsDead());
		TestEqual(TEXT("Atomic rejection leaves no pending actions"), Queue->GetPendingCount(), 0);

		TestTrue(TEXT("Fault FinalSnapshot BattleState"), Envelope.FinalSnapshot.BattleState == EBattleState::ResolutionFaulted);
		TestTrue(TEXT("Fault FinalSnapshot Outcome"), Envelope.FinalSnapshot.Outcome == EBattleHUDOutcome::ResolutionFaulted);
		TestFalse(TEXT("Fault FinalSnapshot cannot end turn"), Envelope.FinalSnapshot.bCanEndTurn);
		TestEqual(TEXT("Fault FinalSnapshot Energy"), Envelope.FinalSnapshot.Energy, 0);
		TestEqual(TEXT("Fault FinalSnapshot Player HP unchanged"), Envelope.FinalSnapshot.Player.HP, 100);
		TestFalse(TEXT("Fault FinalSnapshot Player remains alive"), Envelope.FinalSnapshot.Player.bDead);

		if (!TestTrue(TEXT("Controller starts EndTurn EnergyChanged playback"), Fixture.Controller->IsWaitingForCompletionForTesting())
			|| !TestEqual(TEXT("Only EnergyChanged reaches widget initially"), Fixture.Widget->PlayCallCount, 1))
		{
			return false;
		}

		FPresentationStateSnapshot Working;
		if (!AssertWorkingNonTerminal(*this, Fixture.Controller, TEXT("Before fault Energy completion"), &Working))
		{
			return false;
		}
		TestEqual(TEXT("Fault Working baseline Energy"), Working.Energy, 3);
		TestEqual(TEXT("Fault Working baseline Player HP"), Working.Player.HP, 100);
		TestTrue(TEXT("Displayed outcome remains None before playback"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);

		if (!TestTrue(TEXT("Complete fault EndTurn EnergyChanged"), Fixture.CompleteCurrentPlayback()))
		{
			return false;
		}
		if (!AssertWorkingNonTerminal(*this, Fixture.Controller, TEXT("While ResolutionFault is animating"), &Working))
		{
			return false;
		}
		TestEqual(TEXT("Working Energy reaches zero before terminal fault"), Working.Energy, 0);
		TestEqual(TEXT("Working Player HP remains unchanged before terminal fault"), Working.Player.HP, 100);
		TestTrue(TEXT("Displayed outcome remains None while fault animates"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);
		TestTrue(TEXT("Displayed interaction remains Resolving while fault animates"), Fixture.ViewModel->InteractionState == EBattleHUDInteractionState::Resolving);
		TestTrue(TEXT("Displayed input remains locked while fault animates"), Fixture.ViewModel->bInputLocked);
		TestFalse(TEXT("Displayed EndTurn remains disabled while fault animates"), Fixture.ViewModel->bCanEndTurn);
		TestEqual(TEXT("ResolutionFault becomes second visible playback"), Fixture.Widget->PlayCallCount, 2);
		TestTrue(TEXT("ResolutionFault token is waiting"), Fixture.Controller->IsWaitingForCompletionForTesting());

		const FPresentationPlaybackToken FaultToken = Fixture.Controller->GetActivePlaybackTokenForTesting();
		TestEqual(TEXT("Fault token BattleId"), FaultToken.BattleId, Fault.BattleId);
		TestEqual(TEXT("Fault token ResolutionId"), FaultToken.ResolutionId, Fault.ResolutionId);
		TestEqual(TEXT("Fault token PresentationSequence"), FaultToken.PresentationSequence, Fault.PresentationSequence);
		TestTrue(TEXT("Fault token generation positive"), FaultToken.LocalPlaybackGeneration > 0);

		if (!TestTrue(TEXT("Complete ResolutionFault terminal playback"), Fixture.CompleteCurrentPlayback()))
		{
			return false;
		}
		if (!TestTrue(TEXT("ResolutionFault playback fully drains"), Fixture.DrainPlayback()))
		{
			return false;
		}

		TestFalse(TEXT("Fault completion clears wait"), Fixture.Controller->IsWaitingForCompletionForTesting());
		TestEqual(TEXT("Fault completion clears backlog"), Fixture.Controller->GetBacklogCountForTesting(), 0);
		TestEqual(TEXT("Fault Resolution completes"), Fixture.Controller->GetLastCompletedResolutionIdForTesting(), Envelope.ResolutionId);
		TestTrue(TEXT("Displayed outcome enters ResolutionFaulted only after terminal completion"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::ResolutionFaulted);
		TestTrue(TEXT("Displayed interaction enters Terminal"), Fixture.ViewModel->InteractionState == EBattleHUDInteractionState::Terminal);
		TestTrue(TEXT("Displayed input remains locked in Terminal"), Fixture.ViewModel->bInputLocked);
		TestFalse(TEXT("Displayed EndTurn disabled in Terminal"), Fixture.ViewModel->bCanEndTurn);
		TestEqual(TEXT("Displayed terminal Energy"), Fixture.ViewModel->Energy, 0);
		TestEqual(TEXT("Displayed terminal Player HP"), Fixture.ViewModel->Player.HP, 100);

		FPresentationStateSnapshot ReleasedWorking;
		TestFalse(
			TEXT("Caught-up fault Controller releases WorkingSnapshot"),
			Fixture.Controller->TryGetWorkingSnapshotForTesting(ReleasedWorking)
		);

		const int64 CompletedResolutionBeforeDuplicate = Fixture.Controller->GetLastCompletedResolutionIdForTesting();
		const int32 PlayCallsBeforeDuplicate = Fixture.Widget->PlayCallCount;
		Fixture.Controller->NotifyPresentationFinished(FaultToken);
		TestEqual(
			TEXT("Duplicate fault token cannot advance completed resolution"),
			Fixture.Controller->GetLastCompletedResolutionIdForTesting(),
			CompletedResolutionBeforeDuplicate
		);
		TestEqual(
			TEXT("Duplicate fault token cannot start new playback"),
			Fixture.Widget->PlayCallCount,
			PlayCallsBeforeDuplicate
		);
		TestFalse(TEXT("Duplicate fault token cannot restore wait state"), Fixture.Controller->IsWaitingForCompletionForTesting());
		TestTrue(TEXT("Duplicate fault token leaves displayed Outcome unchanged"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::ResolutionFaulted);

		TestTrue(
			TEXT("ResolutionFault Envelope reducer-owned state matches FinalSnapshot"),
			AssertReducerOwnedStateMatchesFinalSnapshot(
				*this,
				Capture.Baseline,
				Envelope,
				TEXT("Terminal.ResolutionFault")
			)
		);
		TestTrue(
			TEXT("ResolutionFault captured Envelope order remains monotonic"),
			AssertCapturedEnvelopeOrder(*this, Fixture.CapturedEnvelopes, TEXT("Terminal.ResolutionFault"))
		);
		TestTrue(
			TEXT("ResolutionFault Controller playback exactly matches producer history"),
			AssertControllerPlaybackMatchesCapturedHistory(
				*this,
				Fixture.CapturedEnvelopes,
				Fixture.Widget,
				TEXT("Terminal.ResolutionFault Controller")
			)
		);
	}

	// Negative ownership assertion: a Presentation-only freeze failure must disable
	// committed historical playback without turning the authoritative Gameplay
	// battle into ResolutionFaulted.
	{
		FAcceptanceFixture PresentationFailureFixture;
		TArray<UCardData*> EmptyDeck;
		if (!TestTrue(
			TEXT("Presentation-failure fixture starts normally"),
			PresentationFailureFixture.Start(EmptyDeck, 0, 0, 0)
		))
		{
			return false;
		}

		TestTrue(
			TEXT("Presentation-only negative Resolution begins"),
			PresentationFailureFixture.Battle->BeginSystemPresentationResolutionForTesting()
		);
		PresentationFailureFixture.Battle->SetForcePresentationFreezeFailureForTesting(true);
		TestFalse(
			TEXT("Forced Presentation freeze failure cannot seal an Envelope"),
			PresentationFailureFixture.Battle->SealActivePresentationResolutionForTesting()
		);
		PresentationFailureFixture.Flush();

		TestFalse(
			TEXT("Presentation freeze failure marks Presentation unavailable"),
			PresentationFailureFixture.Battle->IsPresentationAvailable()
		);
		TestTrue(
			TEXT("Presentation freeze failure leaves Gameplay in PlayerTurn"),
			PresentationFailureFixture.Battle->BattleState == EBattleState::PlayerTurn
		);
		TestFalse(
			TEXT("Presentation freeze failure does not fault ActionQueue"),
			PresentationFailureFixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted()
		);
		TestEqual(
			TEXT("Presentation freeze failure publishes no ResolutionFault Envelope"),
			PresentationFailureFixture.CapturedEnvelopes.Num(),
			0
		);
		TestEqual(
			TEXT("Presentation freeze failure produces no playback Record"),
			PresentationFailureFixture.Widget->PlayCallCount,
			0
		);
		TestTrue(
			TEXT("Presentation freeze failure exposes PresentationUnavailable UI"),
			PresentationFailureFixture.ViewModel->InteractionState == EBattleHUDInteractionState::PresentationUnavailable
		);
		TestTrue(
			TEXT("Presentation freeze failure does not expose terminal Gameplay outcome"),
			PresentationFailureFixture.ViewModel->Outcome == EBattleHUDOutcome::None
		);
	}

	return true;
}

#endif
