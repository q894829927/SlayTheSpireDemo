#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2D5TestSupport.h"
#include "Phase6UIA2D5TestTypes.h"
#include "Actions/ApplyStatusAction.h"
#include "Actions/BattleAction.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/GainBlockAction.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Events/TurnEndStatusDecayTrigger.h"
#include "Presentation/BattlePresentationController.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Status/StatusInstance.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2D5TurnCycleOrderingTest
{
	using namespace Phase6UIA2D5Test;

	UCardData* CreateCard(UObject* Outer, const TCHAR* CardId)
	{
		UCardData* Card = NewObject<UCardData>(Outer);
		if (!IsValid(Card))
		{
			return nullptr;
		}
		Card->CardId = FName(CardId);
		Card->DisplayName = FText::FromString(CardId);
		Card->Description = FText::FromString(FString::Printf(TEXT("%s turn-cycle fixture."), CardId));
		Card->CardType = ECardType::Skill;
		Card->TargetType = ECardTargetType::None;
		Card->BaseCost = 0;
		Card->DefaultDestination = ECardDestination::Discard;
		return Card;
	}

	UStatusData* CreateDecayStatus(UObject* Outer)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		if (!IsValid(Definition))
		{
			return nullptr;
		}

		Definition->StatusId = TEXT("TurnCycleDecay");
		Definition->DisplayName = FText::FromString(TEXT("Turn Cycle Decay"));
		Definition->Description = FText::FromString(TEXT("Turn-cycle amount {Amount}."));

		UTurnEndStatusDecayTrigger* Trigger = NewObject<UTurnEndStatusDecayTrigger>(Definition);
		if (!IsValid(Trigger))
		{
			return nullptr;
		}
		Trigger->AmountToRemove = 1;
		Definition->Triggers.Add(Trigger);
		return Definition;
	}

	FName ResolveId(ABattleManager* Battle, ACombatant* Combatant)
	{
		FName Id = NAME_None;
		if (IsValid(Battle) && IsValid(Combatant))
		{
			Battle->TryResolveCombatantPresentationId(Combatant, Id);
		}
		return Id;
	}

	bool RunSetupBatch(FAcceptanceFixture& Fixture, UStatusData* DecayStatus)
	{
		if (!Fixture.IsReady() || !IsValid(DecayStatus))
		{
			return false;
		}

		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		if (!IsValid(Queue)
			|| Queue->IsBusy()
			|| !Fixture.Battle->BeginSystemPresentationResolutionForTesting())
		{
			return false;
		}

		const FPresentationRecordWriter Writer = Fixture.Battle->GetActivePresentationRecordWriterForTesting();
		if (!Writer.IsAvailable())
		{
			return false;
		}

		UApplyStatusAction* ApplyDecay = NewObject<UApplyStatusAction>(Queue);
		UGainBlockAction* GainPlayerBlock = NewObject<UGainBlockAction>(Queue);
		UGainBlockAction* GainEnemyBlock = NewObject<UGainBlockAction>(Queue);
		if (!IsValid(ApplyDecay) || !IsValid(GainPlayerBlock) || !IsValid(GainEnemyBlock))
		{
			return false;
		}

		ApplyDecay->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, DecayStatus, 2);
		ApplyDecay->SetPresentationRecordWriter(Writer);

		GainPlayerBlock->Initialize(Fixture.Player, Fixture.Player, 7);
		GainPlayerBlock->SetPresentationParticipantIds(
			ResolveId(Fixture.Battle, Fixture.Player),
			ResolveId(Fixture.Battle, Fixture.Player)
		);
		GainPlayerBlock->SetPresentationRecordWriter(Writer);

		GainEnemyBlock->Initialize(Fixture.Enemy, Fixture.Enemy, 5);
		GainEnemyBlock->SetPresentationParticipantIds(
			ResolveId(Fixture.Battle, Fixture.Enemy),
			ResolveId(Fixture.Battle, Fixture.Enemy)
		);
		GainEnemyBlock->SetPresentationRecordWriter(Writer);

		TArray<UBattleAction*> Batch;
		Batch.Add(ApplyDecay);
		Batch.Add(GainPlayerBlock);
		Batch.Add(GainEnemyBlock);
		if (!Queue->AddBatchToBackPreserveOrder(Batch) || !Queue->StartProcessing())
		{
			return false;
		}

		Fixture.Flush();
		return !Queue->IsBusy() && !Queue->IsResolutionFaulted();
	}

	UStatusInstance* FindMutableStatus(UStatusContainer* Container, FName StatusId)
	{
		if (!IsValid(Container))
		{
			return nullptr;
		}
		for (const TObjectPtr<UStatusInstance>& Status : Container->GetStatuses())
		{
			if (IsValid(Status.Get()) && Status->GetStatusId() == StatusId)
			{
				return Status.Get();
			}
		}
		return nullptr;
	}

	const FBattleHUDStatusView* FindDisplayedStatus(const UBattleHUDViewModel* ViewModel, FName StatusId)
	{
		if (!IsValid(ViewModel))
		{
			return nullptr;
		}
		return ViewModel->Player.Statuses.FindByPredicate(
			[StatusId](const FBattleHUDStatusView& Status)
			{
				return Status.StatusId == StatusId;
			}
		);
	}

	TArray<int32> HandRuntimeIds(const UDeckRuntime* Deck)
	{
		TArray<int32> Result;
		if (!IsValid(Deck))
		{
			return Result;
		}
		for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
		{
			Result.Add(IsValid(Card.Get()) ? Card->GetRuntimeId() : INDEX_NONE);
		}
		return Result;
	}

	bool AssertType(
		FAutomationTestBase& Test,
		const FPresentationResolutionEnvelope& Envelope,
		int32 Index,
		EBattlePresentationRecordType Type
	)
	{
		if (!Envelope.Records.IsValidIndex(Index))
		{
			Test.AddError(FString::Printf(TEXT("TurnCycle missing Record[%d]."), Index));
			return false;
		}
		return Test.TestTrue(
			*FString::Printf(TEXT("TurnCycle Record[%d] type"), Index),
			Envelope.Records[Index].Type == Type
		);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D5TurnCycleOrderingTest,
	"SlayTheSpireDemo.Phase6UIA2D5.TurnCycleOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D5TurnCycleOrderingTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D5TurnCycleOrderingTest;

	FAcceptanceFixture Fixture;
	UCardData* CardA = CreateCard(Fixture.World, TEXT("TurnCycleA"));
	UCardData* CardB = CreateCard(Fixture.World, TEXT("TurnCycleB"));
	UCardData* CardC = CreateCard(Fixture.World, TEXT("TurnCycleC"));
	UStatusData* DecayStatus = CreateDecayStatus(Fixture.World);
	if (!TestNotNull(TEXT("Turn-cycle card A"), CardA)
		|| !TestNotNull(TEXT("Turn-cycle card B"), CardB)
		|| !TestNotNull(TEXT("Turn-cycle card C"), CardC)
		|| !TestNotNull(TEXT("Turn-cycle decay status"), DecayStatus))
	{
		return false;
	}

	TArray<UCardData*> Definitions;
	Definitions.Add(CardA);
	Definitions.Add(CardB);
	Definitions.Add(CardC);
	if (!TestTrue(
		TEXT("Turn-cycle fixture starts with all three cards in opening Hand"),
		Fixture.Start(Definitions, 3, 2, 3)
	))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UStatusContainer* PlayerStatuses = Fixture.Player->GetStatusContainer();
	if (!TestNotNull(TEXT("Turn-cycle deck runtime"), Deck)
		|| !TestNotNull(TEXT("Turn-cycle player status container"), PlayerStatuses))
	{
		return false;
	}

	if (!TestTrue(TEXT("Setup status and both blocks commit"), RunSetupBatch(Fixture, DecayStatus))
		|| !TestTrue(TEXT("Setup playback drains"), Fixture.DrainPlayback()))
	{
		return false;
	}

	UStatusInstance* RuntimeDecay = FindMutableStatus(PlayerStatuses, TEXT("TurnCycleDecay"));
	const FBattleHUDStatusView* DisplayedDecay = FindDisplayedStatus(Fixture.ViewModel, TEXT("TurnCycleDecay"));
	if (!TestNotNull(TEXT("Runtime decay status exists"), RuntimeDecay)
		|| !TestNotNull(TEXT("Displayed decay status exists"), DisplayedDecay))
	{
		return false;
	}
	const int64 DecayRuntimeSequence = static_cast<int64>(RuntimeDecay->GetRuntimeSequence());

	TestEqual(TEXT("Setup Energy is nonzero"), Fixture.Battle->Energy, 3);
	TestEqual(TEXT("Setup Player Block"), Fixture.Player->Block, 7);
	TestEqual(TEXT("Setup Enemy Block"), Fixture.Enemy->Block, 5);
	TestEqual(TEXT("Setup decay amount"), RuntimeDecay->GetAmount(), 2);
	TestEqual(TEXT("Setup Hand count"), Deck->GetHandCount(), 3);
	TestEqual(TEXT("Setup DrawPile intentionally empty"), Deck->GetDrawCount(), 0);
	TestEqual(TEXT("Setup Discard empty before EndTurn"), Deck->GetDiscardCount(), 0);
	TestEqual(TEXT("Displayed setup Player Block"), Fixture.ViewModel->Player.Block, 7);
	TestEqual(TEXT("Displayed setup Enemy Block"), Fixture.ViewModel->Enemy.Block, 5);
	TestEqual(TEXT("Displayed setup decay amount"), DisplayedDecay->Amount, 2);

	const TArray<int32> InitialHandIds = HandRuntimeIds(Deck);
	if (!TestEqual(TEXT("Three concrete opening card identities"), InitialHandIds.Num(), 3))
	{
		return false;
	}

	if (!TestTrue(TEXT("Reset formal A2D5-4 capture after setup"), Fixture.ResetAcceptanceCapture()))
	{
		return false;
	}

	const FGameplayRequestResult EndTurnResult = Fixture.Battle->RequestEndPlayerTurn();
	if (!TestTrue(TEXT("EndPlayerTurn request accepted"), EndTurnResult.IsAcceptedForResolution()))
	{
		return false;
	}
	Fixture.Flush();

	if (!TestEqual(TEXT("Whole macro turn publishes one EndTurn Envelope"), Fixture.CapturedEnvelopes.Num(), 1))
	{
		return false;
	}
	const FCapturedEnvelope& Capture = Fixture.CapturedEnvelopes[0];
	const FPresentationResolutionEnvelope& Envelope = Capture.Envelope;
	if (!TestEqual(TEXT("Turn cycle emits exactly twelve visible Records"), Envelope.Records.Num(), 12))
	{
		return false;
	}

	const EBattlePresentationRecordType ExpectedTypes[] = {
		EBattlePresentationRecordType::EnergyChanged,
		EBattlePresentationRecordType::CardZoneChanged,
		EBattlePresentationRecordType::CardZoneChanged,
		EBattlePresentationRecordType::CardZoneChanged,
		EBattlePresentationRecordType::StatusChanged,
		EBattlePresentationRecordType::BlockChanged,
		EBattlePresentationRecordType::Damage,
		EBattlePresentationRecordType::EnergyChanged,
		EBattlePresentationRecordType::BlockChanged,
		EBattlePresentationRecordType::DeckShuffled,
		EBattlePresentationRecordType::CardZoneChanged,
		EBattlePresentationRecordType::CardZoneChanged
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedTypes); ++Index)
	{
		AssertType(*this, Envelope, Index, ExpectedTypes[Index]);
	}

	const FPresentationRecord& EndEnergy = Envelope.Records[0];
	TestEqual(TEXT("EndTurn Energy before"), EndEnergy.EnergyChanged.EnergyBefore, 3);
	TestEqual(TEXT("EndTurn Energy after"), EndEnergy.EnergyChanged.EnergyAfter, 0);
	TestEqual(TEXT("EndTurn Energy delta"), EndEnergy.EnergyChanged.Delta, -3);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FPresentationRecord& Discard = Envelope.Records[1 + Index];
		TestEqual(
			*FString::Printf(TEXT("Discard[%d] exact RuntimeId"), Index),
			Discard.CardZoneChanged.Card.RuntimeId,
			InitialHandIds[Index]
		);
		TestEqual(*FString::Printf(TEXT("Discard[%d] FromZone"), Index), Discard.CardZoneChanged.FromZone, ECardZone::Hand);
		TestEqual(*FString::Printf(TEXT("Discard[%d] ToZone"), Index), Discard.CardZoneChanged.ToZone, ECardZone::DiscardPile);
	}

	const FPresentationRecord& Decay = Envelope.Records[4];
	TestEqual(TEXT("Decay StatusId"), Decay.StatusChanged.StatusId, FName(TEXT("TurnCycleDecay")));
	TestEqual(TEXT("Decay Source"), Decay.StatusChanged.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Decay Target"), Decay.StatusChanged.TargetPresentationId, FName(TEXT("PlayerHero")));
	TestTrue(TEXT("Decay reason is TurnEndDecay"), Decay.StatusChanged.Reason == EStatusChangeReason::TurnEndDecay);
	TestEqual(TEXT("Decay amount before"), Decay.StatusChanged.AmountBefore, 2);
	TestEqual(TEXT("Decay amount after"), Decay.StatusChanged.AmountAfter, 1);
	TestEqual(TEXT("Decay RuntimeSequence stable"), Decay.StatusChanged.RuntimeSequence, DecayRuntimeSequence);
	TestFalse(TEXT("Decay does not create"), Decay.StatusChanged.bCreated);
	TestFalse(TEXT("Decay does not remove"), Decay.StatusChanged.bRemoved);

	const FPresentationRecord& EnemyClear = Envelope.Records[5];
	TestTrue(TEXT("Enemy clear reason"), EnemyClear.BlockChanged.Reason == EBlockPresentationReason::TurnStartClear);
	TestTrue(TEXT("Enemy clear Source is None"), EnemyClear.BlockChanged.SourcePresentationId.IsNone());
	TestEqual(TEXT("Enemy clear Target"), EnemyClear.BlockChanged.TargetPresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("Enemy clear before"), EnemyClear.BlockChanged.BlockBefore, 5);
	TestEqual(TEXT("Enemy clear after"), EnemyClear.BlockChanged.BlockAfter, 0);
	TestEqual(TEXT("Enemy clear delta"), EnemyClear.BlockChanged.BlockDelta, -5);

	const FPresentationRecord& EnemyDamage = Envelope.Records[6];
	TestEqual(TEXT("Enemy Damage source"), EnemyDamage.Damage.SourcePresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("Enemy Damage target"), EnemyDamage.Damage.TargetPresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Enemy Damage incoming"), EnemyDamage.Damage.IncomingDamage, 3);
	TestEqual(TEXT("Enemy Damage player HP before"), EnemyDamage.Damage.HPBefore, 100);
	TestEqual(TEXT("Enemy Damage player HP after"), EnemyDamage.Damage.HPAfter, 100);
	TestEqual(TEXT("Enemy Damage block before"), EnemyDamage.Damage.BlockBefore, 7);
	TestEqual(TEXT("Enemy Damage block after"), EnemyDamage.Damage.BlockAfter, 4);
	TestEqual(TEXT("Enemy Damage blocked amount"), EnemyDamage.Damage.BlockedDamage, 3);
	TestEqual(TEXT("Enemy Damage HP damage"), EnemyDamage.Damage.HPDamage, 0);

	const FPresentationRecord& StartEnergy = Envelope.Records[7];
	TestEqual(TEXT("PlayerTurnStart Energy before"), StartEnergy.EnergyChanged.EnergyBefore, 0);
	TestEqual(TEXT("PlayerTurnStart Energy after"), StartEnergy.EnergyChanged.EnergyAfter, 3);
	TestEqual(TEXT("PlayerTurnStart Energy delta"), StartEnergy.EnergyChanged.Delta, 3);

	const FPresentationRecord& PlayerClear = Envelope.Records[8];
	TestTrue(TEXT("Player clear reason"), PlayerClear.BlockChanged.Reason == EBlockPresentationReason::TurnStartClear);
	TestTrue(TEXT("Player clear Source is None"), PlayerClear.BlockChanged.SourcePresentationId.IsNone());
	TestEqual(TEXT("Player clear Target"), PlayerClear.BlockChanged.TargetPresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Player clear before"), PlayerClear.BlockChanged.BlockBefore, 4);
	TestEqual(TEXT("Player clear after"), PlayerClear.BlockChanged.BlockAfter, 0);
	TestEqual(TEXT("Player clear delta"), PlayerClear.BlockChanged.BlockDelta, -4);

	const FPresentationRecord& Shuffle = Envelope.Records[9];
	TestEqual(TEXT("Shuffle moved all three discarded cards"), Shuffle.DeckShuffled.MovedCardCount, 3);
	TestEqual(TEXT("Shuffle Draw before"), Shuffle.DeckShuffled.DrawCountBefore, 0);
	TestEqual(TEXT("Shuffle Draw after"), Shuffle.DeckShuffled.DrawCountAfter, 3);
	TestEqual(TEXT("Shuffle Discard before"), Shuffle.DeckShuffled.DiscardCountBefore, 3);
	TestEqual(TEXT("Shuffle Discard after"), Shuffle.DeckShuffled.DiscardCountAfter, 0);

	const FPresentationRecord& DrawOne = Envelope.Records[10];
	const FPresentationRecord& DrawTwo = Envelope.Records[11];
	TestEqual(TEXT("First post-shuffle draw FromZone"), DrawOne.CardZoneChanged.FromZone, ECardZone::DrawPile);
	TestEqual(TEXT("First post-shuffle draw ToZone"), DrawOne.CardZoneChanged.ToZone, ECardZone::Hand);
	TestEqual(TEXT("Second post-shuffle draw FromZone"), DrawTwo.CardZoneChanged.FromZone, ECardZone::DrawPile);
	TestEqual(TEXT("Second post-shuffle draw ToZone"), DrawTwo.CardZoneChanged.ToZone, ECardZone::Hand);
	TestTrue(TEXT("Two post-shuffle draws are different concrete cards"), DrawOne.CardZoneChanged.Card.RuntimeId != DrawTwo.CardZoneChanged.Card.RuntimeId);
	TestTrue(TEXT("First drawn card came from original turn-cycle deck"), InitialHandIds.Contains(DrawOne.CardZoneChanged.Card.RuntimeId));
	TestTrue(TEXT("Second drawn card came from original turn-cycle deck"), InitialHandIds.Contains(DrawTwo.CardZoneChanged.Card.RuntimeId));

	// Gameplay has completed the complete macro turn before visible playback catches up.
	TestTrue(TEXT("Authoritative battle is back in PlayerTurn"), Fixture.Battle->BattleState == EBattleState::PlayerTurn);
	TestEqual(TEXT("Authoritative Energy restored"), Fixture.Battle->Energy, 3);
	TestEqual(TEXT("Authoritative Player Block cleared"), Fixture.Player->Block, 0);
	TestEqual(TEXT("Authoritative Enemy Block cleared"), Fixture.Enemy->Block, 0);
	TestEqual(TEXT("Authoritative Player HP remains fully blocked"), Fixture.Player->HP, 100);
	TestEqual(TEXT("Authoritative decay amount is one"), RuntimeDecay->GetAmount(), 1);
	TestEqual(TEXT("Authoritative Hand has two cards"), Deck->GetHandCount(), 2);
	TestEqual(TEXT("Authoritative Draw has one card"), Deck->GetDrawCount(), 1);
	TestEqual(TEXT("Authoritative Discard is empty after shuffle"), Deck->GetDiscardCount(), 0);

	if (!TestTrue(TEXT("Controller begins with EndTurn EnergyChanged"), Fixture.Controller->IsWaitingForCompletionForTesting())
		|| !TestEqual(TEXT("Only first turn-cycle record reached widget initially"), Fixture.Widget->PlayCallCount, 1))
	{
		return false;
	}
	TestEqual(TEXT("Displayed baseline Energy before playback"), Fixture.ViewModel->Energy, 3);
	TestEqual(TEXT("Displayed baseline Hand before playback"), Fixture.ViewModel->HandCards.Num(), 3);
	TestEqual(TEXT("Displayed baseline Draw before playback"), Fixture.ViewModel->DrawCount, 0);
	TestEqual(TEXT("Displayed baseline Discard before playback"), Fixture.ViewModel->DiscardCount, 0);
	TestEqual(TEXT("Displayed baseline Player Block before playback"), Fixture.ViewModel->Player.Block, 7);
	TestEqual(TEXT("Displayed baseline Enemy Block before playback"), Fixture.ViewModel->Enemy.Block, 5);
	TestEqual(TEXT("Displayed baseline decay amount"), FindDisplayedStatus(Fixture.ViewModel, TEXT("TurnCycleDecay"))->Amount, 2);

	if (!TestTrue(TEXT("Complete EndTurn EnergyChanged"), Fixture.CompleteCurrentPlayback())) return false;
	TestEqual(TEXT("Displayed Energy becomes zero"), Fixture.ViewModel->Energy, 0);
	TestEqual(TEXT("Displayed Hand still three before discards"), Fixture.ViewModel->HandCards.Num(), 3);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!TestTrue(*FString::Printf(TEXT("Complete Hand discard %d"), Index), Fixture.CompleteCurrentPlayback())) return false;
		TestEqual(
			*FString::Printf(TEXT("Displayed Hand count after discard %d"), Index),
			Fixture.ViewModel->HandCards.Num(),
			2 - Index
		);
		TestEqual(
			*FString::Printf(TEXT("Displayed Discard count after discard %d"), Index),
			Fixture.ViewModel->DiscardCount,
			1 + Index
		);
	}

	if (!TestTrue(TEXT("Complete TurnEndDecay"), Fixture.CompleteCurrentPlayback())) return false;
	const FBattleHUDStatusView* DisplayedDecayAfter = FindDisplayedStatus(Fixture.ViewModel, TEXT("TurnCycleDecay"));
	if (!TestNotNull(TEXT("Displayed decay remains after TurnEndDecay"), DisplayedDecayAfter)) return false;
	TestEqual(TEXT("Displayed decay advances to one"), DisplayedDecayAfter->Amount, 1);
	TestEqual(TEXT("Displayed decay RuntimeSequence remains stable"), DisplayedDecayAfter->RuntimeSequence, DecayRuntimeSequence);

	if (!TestTrue(TEXT("Complete Enemy Block clear"), Fixture.CompleteCurrentPlayback())) return false;
	TestEqual(TEXT("Displayed Enemy Block clears before Damage"), Fixture.ViewModel->Enemy.Block, 0);
	TestEqual(TEXT("Displayed Player Block still seven before Damage"), Fixture.ViewModel->Player.Block, 7);

	if (!TestTrue(TEXT("Complete Enemy Damage"), Fixture.CompleteCurrentPlayback())) return false;
	TestEqual(TEXT("Displayed Player Block reduced by Damage"), Fixture.ViewModel->Player.Block, 4);
	TestEqual(TEXT("Displayed Player HP remains 100"), Fixture.ViewModel->Player.HP, 100);

	if (!TestTrue(TEXT("Complete PlayerTurnStart EnergyChanged"), Fixture.CompleteCurrentPlayback())) return false;
	TestEqual(TEXT("Displayed Energy restores before player Block clear"), Fixture.ViewModel->Energy, 3);
	TestEqual(TEXT("Displayed Player Block still four before clear"), Fixture.ViewModel->Player.Block, 4);

	if (!TestTrue(TEXT("Complete Player Block clear"), Fixture.CompleteCurrentPlayback())) return false;
	TestEqual(TEXT("Displayed Player Block clears"), Fixture.ViewModel->Player.Block, 0);
	TestEqual(TEXT("Displayed Discard remains three before shuffle"), Fixture.ViewModel->DiscardCount, 3);
	TestEqual(TEXT("Displayed Draw remains zero before shuffle"), Fixture.ViewModel->DrawCount, 0);

	if (!TestTrue(TEXT("Complete DeckShuffled"), Fixture.CompleteCurrentPlayback())) return false;
	TestEqual(TEXT("Displayed Draw becomes three after shuffle"), Fixture.ViewModel->DrawCount, 3);
	TestEqual(TEXT("Displayed Discard clears after shuffle"), Fixture.ViewModel->DiscardCount, 0);
	TestEqual(TEXT("Displayed Hand still empty before draws"), Fixture.ViewModel->HandCards.Num(), 0);

	if (!TestTrue(TEXT("Complete first post-shuffle draw"), Fixture.CompleteCurrentPlayback())) return false;
	TestEqual(TEXT("Displayed Hand one after first draw"), Fixture.ViewModel->HandCards.Num(), 1);
	TestEqual(TEXT("Displayed Draw two after first draw"), Fixture.ViewModel->DrawCount, 2);
	TestEqual(TEXT("Displayed first drawn RuntimeId"), Fixture.ViewModel->HandCards[0].RuntimeId, DrawOne.CardZoneChanged.Card.RuntimeId);

	if (!TestTrue(TEXT("Complete second post-shuffle draw"), Fixture.CompleteCurrentPlayback())) return false;
	if (!TestTrue(TEXT("Turn-cycle playback fully drains"), Fixture.DrainPlayback())) return false;
	TestEqual(TEXT("Displayed Hand two after second draw"), Fixture.ViewModel->HandCards.Num(), 2);
	TestEqual(TEXT("Displayed Draw one after second draw"), Fixture.ViewModel->DrawCount, 1);
	TestEqual(TEXT("Displayed Discard zero after macro turn"), Fixture.ViewModel->DiscardCount, 0);
	TestEqual(TEXT("Displayed second drawn RuntimeId"), Fixture.ViewModel->HandCards[1].RuntimeId, DrawTwo.CardZoneChanged.Card.RuntimeId);

	TestTrue(
		TEXT("Turn-cycle Envelope reducer-owned state matches FinalSnapshot"),
		AssertReducerOwnedStateMatchesFinalSnapshot(
			*this,
			Capture.Baseline,
			Envelope,
			TEXT("TurnCycleOrdering")
		)
	);
	TestTrue(
		TEXT("Turn-cycle captured Envelope order remains monotonic"),
		AssertCapturedEnvelopeOrder(*this, Fixture.CapturedEnvelopes, TEXT("TurnCycleOrdering"))
	);
	TestTrue(
		TEXT("Turn-cycle Controller playback exactly matches producer history"),
		AssertControllerPlaybackMatchesCapturedHistory(
			*this,
			Fixture.CapturedEnvelopes,
			Fixture.Widget,
			TEXT("TurnCycleOrdering Controller")
		)
	);

	TestEqual(TEXT("FinalSnapshot Energy"), Envelope.FinalSnapshot.Energy, 3);
	TestEqual(TEXT("FinalSnapshot Player Block"), Envelope.FinalSnapshot.Player.Block, 0);
	TestEqual(TEXT("FinalSnapshot Enemy Block"), Envelope.FinalSnapshot.Enemy.Block, 0);
	TestEqual(TEXT("FinalSnapshot Player HP"), Envelope.FinalSnapshot.Player.HP, 100);
	TestEqual(TEXT("FinalSnapshot Hand count"), Envelope.FinalSnapshot.HandCards.Num(), 2);
	TestEqual(TEXT("FinalSnapshot Draw count"), Envelope.FinalSnapshot.DrawCount, 1);
	TestEqual(TEXT("FinalSnapshot Discard count"), Envelope.FinalSnapshot.DiscardCount, 0);
	TestTrue(TEXT("FinalSnapshot returns to PlayerTurn"), Envelope.FinalSnapshot.BattleState == EBattleState::PlayerTurn);
	TestEqual(TEXT("FinalSnapshot displayed Hand[0] identity"), Envelope.FinalSnapshot.HandCards[0].RuntimeId, DrawOne.CardZoneChanged.Card.RuntimeId);
	TestEqual(TEXT("FinalSnapshot displayed Hand[1] identity"), Envelope.FinalSnapshot.HandCards[1].RuntimeId, DrawTwo.CardZoneChanged.Card.RuntimeId);
	return true;
}

#endif
