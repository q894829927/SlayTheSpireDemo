#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2D5TestSupport.h"
#include "Phase6UIA2D5TestTypes.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2D5TerminalVictoryTest
{
	using namespace Phase6UIA2D5Test;

	UCardData* CreateLethalCard(UObject* Outer)
	{
		if (!IsValid(Outer))
		{
			return nullptr;
		}

		UCardData* Card = NewObject<UCardData>(Outer);
		if (!IsValid(Card))
		{
			return nullptr;
		}

		Card->CardId = TEXT("A2D5TerminalVictory");
		Card->DisplayName = FText::FromString(TEXT("A2D5 Terminal Victory"));
		Card->Description = FText::FromString(TEXT("Deal {Damage} lethal damage."));
		Card->CardType = ECardType::Attack;
		Card->TargetType = ECardTargetType::Enemy;
		Card->BaseCost = 1;
		Card->DefaultDestination = ECardDestination::Discard;

		UDamageCardEffect* Damage = NewObject<UDamageCardEffect>(Card);
		if (!IsValid(Damage))
		{
			return nullptr;
		}

		Damage->DescriptionArgumentName = TEXT("Damage");
		Damage->BaseAmount = 100;
		Damage->HitCount = 1;
		Damage->DamageKind = EDamageKind::Attack;
		Card->Effects.Add(Damage);
		return Card;
	}

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
			*FString::Printf(TEXT("%s WorkingSnapshot can still end turn"), *Context),
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
	FPhase6UIA2D5TerminalVictoryTest,
	"SlayTheSpireDemo.Phase6UIA2D5.Terminal.Victory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D5TerminalVictoryTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D5TerminalVictoryTest;

	FAcceptanceFixture Fixture;
	UCardData* LethalDefinition = CreateLethalCard(Fixture.World);
	if (!TestNotNull(TEXT("Lethal card definition"), LethalDefinition))
	{
		return false;
	}

	TArray<UCardData*> Definitions;
	Definitions.Add(LethalDefinition);
	if (!TestTrue(
		TEXT("Victory fixture starts with lethal card in opening Hand"),
		Fixture.Start(Definitions, 1, 0, 0)
	))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	if (!TestNotNull(TEXT("Victory deck runtime"), Deck)
		|| !TestEqual(TEXT("Opening Hand has one lethal card"), Deck->GetHandCount(), 1)
		|| !TestEqual(TEXT("Displayed opening Hand has one lethal card"), Fixture.ViewModel->HandCards.Num(), 1))
	{
		return false;
	}

	UCardInstance* Card = Deck->GetHandCards()[0].Get();
	if (!TestNotNull(TEXT("Lethal runtime card"), Card))
	{
		return false;
	}
	const int32 RuntimeId = Card->GetRuntimeId();

	TestEqual(TEXT("Victory baseline Gameplay Energy"), Fixture.Battle->Energy, 3);
	TestEqual(TEXT("Victory baseline Gameplay Enemy HP"), Fixture.Enemy->HP, 100);
	TestEqual(TEXT("Victory baseline displayed Energy"), Fixture.ViewModel->Energy, 3);
	TestEqual(TEXT("Victory baseline displayed Enemy HP"), Fixture.ViewModel->Enemy.HP, 100);
	TestTrue(TEXT("Victory baseline Gameplay is PlayerTurn"), Fixture.Battle->BattleState == EBattleState::PlayerTurn);
	TestTrue(TEXT("Victory baseline displayed Outcome is None"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);

	const FGameplayRequestResult PlayResult = Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy);
	if (!TestTrue(TEXT("Lethal card request accepted"), PlayResult.IsAcceptedForResolution()))
	{
		return false;
	}
	Fixture.Flush();

	if (!TestEqual(TEXT("Lethal card publishes one Resolution Envelope"), Fixture.CapturedEnvelopes.Num(), 1))
	{
		return false;
	}

	const FCapturedEnvelope& Capture = Fixture.CapturedEnvelopes[0];
	const FPresentationResolutionEnvelope& Envelope = Capture.Envelope;
	if (!TestEqual(TEXT("Lethal card emits exactly four visible Records"), Envelope.Records.Num(), 4))
	{
		return false;
	}

	TestTrue(TEXT("Record[0] CardPlayed"), Envelope.Records[0].Type == EBattlePresentationRecordType::CardPlayed);
	TestTrue(TEXT("Record[1] Damage"), Envelope.Records[1].Type == EBattlePresentationRecordType::Damage);
	TestTrue(TEXT("Record[2] CardZoneChanged"), Envelope.Records[2].Type == EBattlePresentationRecordType::CardZoneChanged);
	TestTrue(TEXT("Record[3] Victory"), Envelope.Records[3].Type == EBattlePresentationRecordType::Victory);
	TestTrue(TEXT("Victory is final Record"), Envelope.Records.Last().Type == EBattlePresentationRecordType::Victory);
	TestEqual(TEXT("Exactly one Victory Record"), CountRecords(Envelope, EBattlePresentationRecordType::Victory), 1);
	TestEqual(TEXT("No Defeat Record"), CountRecords(Envelope, EBattlePresentationRecordType::Defeat), 0);
	TestEqual(TEXT("No ResolutionFault Record"), CountRecords(Envelope, EBattlePresentationRecordType::ResolutionFault), 0);
	TestEqual(TEXT("No duplicate EnergyChanged for card cost"), CountRecords(Envelope, EBattlePresentationRecordType::EnergyChanged), 0);

	const FPresentationRecord& CardPlayed = Envelope.Records[0];
	TestEqual(TEXT("CardPlayed exact RuntimeId"), CardPlayed.CardPlayed.Card.RuntimeId, RuntimeId);
	TestEqual(TEXT("CardPlayed source"), CardPlayed.CardPlayed.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("CardPlayed target"), CardPlayed.CardPlayed.TargetPresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("CardPlayed Energy before"), CardPlayed.CardPlayed.EnergyBefore, 3);
	TestEqual(TEXT("CardPlayed Energy after"), CardPlayed.CardPlayed.EnergyAfter, 2);
	TestEqual(TEXT("CardPlayed CostPaid"), CardPlayed.CardPlayed.CostPaid, 1);

	const FPresentationRecord& Damage = Envelope.Records[1];
	TestEqual(TEXT("Lethal Damage source"), Damage.Damage.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Lethal Damage target"), Damage.Damage.TargetPresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("Lethal Damage incoming"), Damage.Damage.IncomingDamage, 100);
	TestEqual(TEXT("Lethal Damage HP before"), Damage.Damage.HPBefore, 100);
	TestEqual(TEXT("Lethal Damage HP after"), Damage.Damage.HPAfter, 0);
	TestEqual(TEXT("Lethal Damage HP damage"), Damage.Damage.HPDamage, 100);
	TestEqual(TEXT("Lethal Damage block before"), Damage.Damage.BlockBefore, 0);
	TestEqual(TEXT("Lethal Damage block after"), Damage.Damage.BlockAfter, 0);

	const FPresentationRecord& Zone = Envelope.Records[2];
	TestEqual(TEXT("Finish-card exact RuntimeId"), Zone.CardZoneChanged.Card.RuntimeId, RuntimeId);
	TestTrue(TEXT("Finish-card FromZone PlayArea"), Zone.CardZoneChanged.FromZone == ECardZone::PlayArea);
	TestTrue(TEXT("Finish-card ToZone Discard"), Zone.CardZoneChanged.ToZone == ECardZone::DiscardPile);

	const FPresentationRecord& Victory = Envelope.Records[3];
	TestEqual(TEXT("Victory winner identity"), Victory.Terminal.WinnerPresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Victory defeated identity"), Victory.Terminal.DefeatedPresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("Victory FinalSnapshot BattleState"), Envelope.FinalSnapshot.BattleState, EBattleState::Victory);
	TestTrue(TEXT("Victory FinalSnapshot Outcome"), Envelope.FinalSnapshot.Outcome == EBattleHUDOutcome::Victory);
	TestFalse(TEXT("Victory FinalSnapshot cannot end turn"), Envelope.FinalSnapshot.bCanEndTurn);
	TestEqual(TEXT("Victory FinalSnapshot Enemy HP"), Envelope.FinalSnapshot.Enemy.HP, 0);
	TestTrue(TEXT("Victory FinalSnapshot Enemy dead"), Envelope.FinalSnapshot.Enemy.bDead);
	TestEqual(TEXT("Victory FinalSnapshot Discard count"), Envelope.FinalSnapshot.DiscardCount, 1);
	TestEqual(TEXT("Victory FinalSnapshot Hand empty"), Envelope.FinalSnapshot.HandCards.Num(), 0);
	TestEqual(TEXT("Victory FinalSnapshot terminal Energy normalized to zero"), Envelope.FinalSnapshot.Energy, 0);

	// Gameplay is already authoritative and terminal by the time the immutable
	// Envelope is published, while Presentation still owns the old baseline.
	TestTrue(TEXT("Gameplay commits Victory immediately"), Fixture.Battle->BattleState == EBattleState::Victory);
	TestEqual(TEXT("Gameplay terminal Energy normalized to zero"), Fixture.Battle->Energy, 0);
	TestTrue(TEXT("Gameplay Enemy is dead"), Fixture.Enemy->IsDead());
	TestEqual(TEXT("Gameplay Hand is empty"), Deck->GetHandCount(), 0);
	TestEqual(TEXT("Gameplay Discard contains played card"), Deck->GetDiscardCount(), 1);

	if (!TestTrue(TEXT("Controller starts CardPlayed playback"), Fixture.Controller->IsWaitingForCompletionForTesting())
		|| !TestEqual(TEXT("Only CardPlayed reached widget initially"), Fixture.Widget->PlayCallCount, 1))
	{
		return false;
	}

	FPresentationStateSnapshot Working;
	if (!AssertWorkingNonTerminal(*this, Fixture.Controller, TEXT("Before CardPlayed completion"), &Working))
	{
		return false;
	}
	TestEqual(TEXT("Working baseline Energy"), Working.Energy, 3);
	TestEqual(TEXT("Working baseline Hand count"), Working.HandCards.Num(), 1);
	TestEqual(TEXT("Working baseline Enemy HP"), Working.Enemy.HP, 100);
	TestFalse(TEXT("Working baseline Enemy is not dead"), Working.Enemy.bDead);
	TestTrue(TEXT("Displayed outcome remains non-terminal before playback"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);

	if (!TestTrue(TEXT("Complete CardPlayed"), Fixture.CompleteCurrentPlayback()))
	{
		return false;
	}
	if (!AssertWorkingNonTerminal(*this, Fixture.Controller, TEXT("After CardPlayed completion"), &Working))
	{
		return false;
	}
	TestEqual(TEXT("Working Energy after CardPlayed"), Working.Energy, 2);
	TestEqual(TEXT("Working Hand empty after CardPlayed"), Working.HandCards.Num(), 0);
	TestEqual(TEXT("Working Enemy HP unchanged before Damage"), Working.Enemy.HP, 100);
	TestEqual(TEXT("Displayed Energy after CardPlayed"), Fixture.ViewModel->Energy, 2);
	TestEqual(TEXT("Displayed Hand empty after CardPlayed"), Fixture.ViewModel->HandCards.Num(), 0);
	TestEqual(TEXT("Displayed Enemy HP unchanged before Damage"), Fixture.ViewModel->Enemy.HP, 100);
	TestEqual(TEXT("Damage becomes second visible playback"), Fixture.Widget->PlayCallCount, 2);

	if (!TestTrue(TEXT("Complete lethal Damage"), Fixture.CompleteCurrentPlayback()))
	{
		return false;
	}
	if (!AssertWorkingNonTerminal(*this, Fixture.Controller, TEXT("After lethal Damage completion"), &Working))
	{
		return false;
	}
	TestEqual(TEXT("Working Enemy HP reaches zero"), Working.Enemy.HP, 0);
	TestTrue(TEXT("Working Enemy dead after Damage"), Working.Enemy.bDead);
	TestEqual(TEXT("Working Discard still zero before finish-card"), Working.DiscardCount, 0);
	TestEqual(TEXT("Displayed Enemy HP reaches zero"), Fixture.ViewModel->Enemy.HP, 0);
	TestTrue(TEXT("Displayed Enemy dead after Damage"), Fixture.ViewModel->Enemy.bDead);
	TestTrue(TEXT("Displayed outcome still None after lethal Damage"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);
	TestEqual(TEXT("CardZoneChanged becomes third visible playback"), Fixture.Widget->PlayCallCount, 3);

	if (!TestTrue(TEXT("Complete finish-card CardZoneChanged"), Fixture.CompleteCurrentPlayback()))
	{
		return false;
	}
	if (!AssertWorkingNonTerminal(*this, Fixture.Controller, TEXT("While Victory is animating"), &Working))
	{
		return false;
	}
	TestEqual(TEXT("Working Discard advances before Victory"), Working.DiscardCount, 1);
	TestEqual(TEXT("Working Energy stays at post-card value before terminal completion"), Working.Energy, 2);
	TestEqual(TEXT("Displayed Discard advances before Victory"), Fixture.ViewModel->DiscardCount, 1);
	TestTrue(TEXT("Displayed outcome remains None while Victory animates"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::None);
	TestTrue(TEXT("Displayed interaction is not Terminal while Victory animates"), Fixture.ViewModel->InteractionState != EBattleHUDInteractionState::Terminal);
	TestEqual(TEXT("Victory becomes fourth visible playback"), Fixture.Widget->PlayCallCount, 4);
	TestTrue(TEXT("Victory token is waiting"), Fixture.Controller->IsWaitingForCompletionForTesting());

	const FPresentationPlaybackToken VictoryToken = Fixture.Controller->GetActivePlaybackTokenForTesting();
	TestEqual(TEXT("Victory token BattleId"), VictoryToken.BattleId, Victory.BattleId);
	TestEqual(TEXT("Victory token ResolutionId"), VictoryToken.ResolutionId, Victory.ResolutionId);
	TestEqual(TEXT("Victory token PresentationSequence"), VictoryToken.PresentationSequence, Victory.PresentationSequence);
	TestTrue(TEXT("Victory token generation positive"), VictoryToken.LocalPlaybackGeneration > 0);

	if (!TestTrue(TEXT("Complete Victory terminal playback"), Fixture.CompleteCurrentPlayback()))
	{
		return false;
	}
	if (!TestTrue(TEXT("Victory playback fully drains"), Fixture.DrainPlayback()))
	{
		return false;
	}

	TestFalse(TEXT("Victory completion clears wait"), Fixture.Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Victory completion clears backlog"), Fixture.Controller->GetBacklogCountForTesting(), 0);
	TestEqual(TEXT("Victory Resolution completes"), Fixture.Controller->GetLastCompletedResolutionIdForTesting(), Envelope.ResolutionId);
	TestTrue(TEXT("Displayed outcome enters Victory only after terminal completion"), Fixture.ViewModel->Outcome == EBattleHUDOutcome::Victory);
	TestTrue(TEXT("Displayed interaction enters Terminal"), Fixture.ViewModel->InteractionState == EBattleHUDInteractionState::Terminal);
	TestEqual(TEXT("Displayed terminal Energy reconciles to zero"), Fixture.ViewModel->Energy, 0);
	TestEqual(TEXT("Displayed terminal Enemy HP"), Fixture.ViewModel->Enemy.HP, 0);
	TestTrue(TEXT("Displayed terminal Enemy dead"), Fixture.ViewModel->Enemy.bDead);
	TestEqual(TEXT("Displayed terminal Discard count"), Fixture.ViewModel->DiscardCount, 1);

	FPresentationStateSnapshot ReleasedWorking;
	TestFalse(
		TEXT("Caught-up Controller releases WorkingSnapshot after terminal Envelope completion"),
		Fixture.Controller->TryGetWorkingSnapshotForTesting(ReleasedWorking)
	);

	TestTrue(
		TEXT("Victory Envelope reducer-owned state matches FinalSnapshot"),
		AssertReducerOwnedStateMatchesFinalSnapshot(
			*this,
			Capture.Baseline,
			Envelope,
			TEXT("Terminal.Victory")
		)
	);
	TestTrue(
		TEXT("Victory captured Envelope order remains monotonic"),
		AssertCapturedEnvelopeOrder(*this, Fixture.CapturedEnvelopes, TEXT("Terminal.Victory"))
	);
	TestTrue(
		TEXT("Victory Controller playback exactly matches producer history"),
		AssertControllerPlaybackMatchesCapturedHistory(
			*this,
			Fixture.CapturedEnvelopes,
			Fixture.Widget,
			TEXT("Terminal.Victory Controller")
		)
	);

	return true;
}

#endif
