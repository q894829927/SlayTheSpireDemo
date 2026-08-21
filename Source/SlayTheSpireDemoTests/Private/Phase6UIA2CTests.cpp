#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Battle/EnergyMutation.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Events/BattleEvent.h"
#include "Events/BattleEventDispatcher.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/BattlePresentationRecorder.h"
#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"
#include "Engine/World.h"

namespace Phase6UIA2CTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		TArray<FPresentationResolutionEnvelope> Deliveries;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(),
				FTransform(FVector(100.0, 0.0, 0.0)),
				SpawnParameters
			);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle))
			{
				return;
			}

			Player->MaxHP = 100;
			Enemy->MaxHP = 100;
			Player->PresentationId = TEXT("PlayerHero");
			Enemy->PresentationId = TEXT("EnemyPrimary");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));

			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->bEnableCommittedPresentationRecording = true;
			Battle->OnPresentationResolutionReady.AddLambda(
				[this](const FPresentationResolutionEnvelope& Envelope)
				{
					Deliveries.Add(Envelope);
				}
			);
		}

		~FFixture()
		{
			UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		UCardData* CreateCard(
			const TCHAR* CardId,
			int32 Cost = 1,
			ECardTargetType TargetType = ECardTargetType::None,
			ECardDestination Destination = ECardDestination::Discard
		)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(CardId);
			Card->DisplayName = FText::FromString(CardId);
			Card->Description = FText::FromString(FString::Printf(TEXT("%s description"), CardId));
			Card->BaseCost = Cost;
			Card->TargetType = TargetType;
			Card->DefaultDestination = Destination;
			return Card;
		}

		void AddDamageEffect(UCardData* Card, int32 Damage)
		{
			UDamageCardEffect* Effect = NewObject<UDamageCardEffect>(Card);
			Effect->BaseAmount = Damage;
			Effect->HitCount = 1;
			Card->Effects.Add(Effect);
		}

		bool Start(
			const TArray<UCardData*>& Definitions,
			int32 OpeningDrawCount,
			int32 PlayerDrawCount = 0,
			int32 EnemyDamage = 0,
			bool bEnablePresentation = true
		)
		{
			if (!IsValid(Battle))
			{
				return false;
			}

			Battle->DebugStartingDeck.Reset();
			for (UCardData* Definition : Definitions)
			{
				Battle->DebugStartingDeck.Add(Definition);
			}
			Battle->OpeningHandDrawCount = OpeningDrawCount;
			Battle->PlayerTurnDrawCount = PlayerDrawCount;
			Battle->EnemyTestAttackDamage = EnemyDamage;
			Battle->bEnableCommittedPresentationRecording = bEnablePresentation;
			Battle->StartBattle();
			Flush();
			return IsReady();
		}

		bool IsReady() const
		{
			return IsValid(World)
				&& IsValid(Player)
				&& IsValid(Enemy)
				&& IsValid(Battle)
				&& IsValid(Battle->GetActionQueueForTesting())
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}

		void Flush() const
		{
			if (IsValid(Battle))
			{
				Battle->FlushScheduledReadStateReadyForTesting();
			}
		}

		void ResetDeliveries()
		{
			Deliveries.Reset();
		}

		const FPresentationResolutionEnvelope* LastDelivery() const
		{
			return Deliveries.Num() > 0 ? &Deliveries.Last() : nullptr;
		}
	};

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

	const FPresentationRecord* FindFirstRecord(
		const FPresentationResolutionEnvelope& Envelope,
		EBattlePresentationRecordType Type
	)
	{
		return Envelope.Records.FindByPredicate(
			[Type](const FPresentationRecord& Record)
			{
				return Record.Type == Type;
			}
		);
	}

	TArray<const FPresentationRecord*> FindRecords(
		const FPresentationResolutionEnvelope& Envelope,
		EBattlePresentationRecordType Type
	)
	{
		TArray<const FPresentationRecord*> Out;
		for (const FPresentationRecord& Record : Envelope.Records)
		{
			if (Record.Type == Type)
			{
				Out.Add(&Record);
			}
		}
		return Out;
	}

	TArray<int32> HandRuntimeIds(const UDeckRuntime* Deck)
	{
		TArray<int32> Out;
		if (!IsValid(Deck))
		{
			return Out;
		}
		for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
		{
			Out.Add(IsValid(Card.Get()) ? Card->GetRuntimeId() : INDEX_NONE);
		}
		return Out;
	}

	bool StartSingleCardFixture(
		FAutomationTestBase& Test,
		FFixture& Fixture,
		UCardData* Card,
		int32 OpeningDrawCount = 1,
		int32 PlayerDrawCount = 0,
		int32 EnemyDamage = 0,
		bool bEnablePresentation = true
	)
	{
		TArray<UCardData*> Cards;
		Cards.Add(Card);
		if (Fixture.Start(Cards, OpeningDrawCount, PlayerDrawCount, EnemyDamage, bEnablePresentation))
		{
			return true;
		}
		Test.AddError(TEXT("Failed to start the Phase 6UI-A2C fixture."));
		return false;
	}
}

using namespace Phase6UIA2CTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2CEnergyCommitResultTest,
	"SlayTheSpireDemo.Phase6UIA2C.Commit.EnergyResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2CEnergyCommitResultTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!IsValid(Fixture.Battle)) return false;
	Fixture.Battle->MaxEnergy = 3;
	Fixture.Battle->Energy = 3;

	const FEnergyCommitResult Spend = BattleEnergyMutation::TrySpend(Fixture.Battle, 1);
	TestTrue(TEXT("Positive spend succeeds"), Spend.bSucceeded);
	TestTrue(TEXT("Positive spend commits a state change"), Spend.bCommitted);
	TestEqual(TEXT("Positive spend before"), Spend.EnergyBefore, 3);
	TestEqual(TEXT("Positive spend after"), Spend.EnergyAfter, 2);
	TestEqual(TEXT("Positive spend delta"), Spend.Delta, -1);
	TestEqual(TEXT("Positive spend delta identity"), Spend.Delta, Spend.EnergyAfter - Spend.EnergyBefore);

	const FEnergyCommitResult Zero = BattleEnergyMutation::TrySpend(Fixture.Battle, 0);
	TestTrue(TEXT("Zero-cost spend succeeds"), Zero.bSucceeded);
	TestFalse(TEXT("Zero-cost spend has no state commit"), Zero.bCommitted);
	TestEqual(TEXT("Zero-cost energy unchanged"), Zero.EnergyAfter, 2);
	TestEqual(TEXT("Zero-cost delta"), Zero.Delta, 0);

	const FEnergyCommitResult Insufficient = BattleEnergyMutation::TrySpend(Fixture.Battle, 5);
	TestFalse(TEXT("Insufficient spend fails"), Insufficient.bSucceeded);
	TestFalse(TEXT("Insufficient spend does not commit"), Insufficient.bCommitted);
	TestEqual(TEXT("Insufficient spend does not mutate energy"), Fixture.Battle->Energy, 2);

	const FEnergyCommitResult Clear = BattleEnergyMutation::SetValue(Fixture.Battle, 0);
	TestTrue(TEXT("SetValue succeeds"), Clear.bSucceeded);
	TestTrue(TEXT("SetValue to a different value commits"), Clear.bCommitted);
	TestEqual(TEXT("SetValue delta identity"), Clear.Delta, Clear.EnergyAfter - Clear.EnergyBefore);
	const FEnergyCommitResult NoopSet = BattleEnergyMutation::SetValue(Fixture.Battle, 0);
	TestTrue(TEXT("No-op SetValue succeeds"), NoopSet.bSucceeded);
	TestFalse(TEXT("No-op SetValue does not commit"), NoopSet.bCommitted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2CDeckMutationTest,
	"SlayTheSpireDemo.Phase6UIA2C.Commit.DeckMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2CDeckMutationTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!IsValid(Fixture.World)) return false;

	UDeckRuntime* Deck = NewObject<UDeckRuntime>(Fixture.World);
	TArray<TObjectPtr<UCardData>> Definitions;
	Definitions.Add(Fixture.CreateCard(TEXT("DeckA"), 0));
	Definitions.Add(Fixture.CreateCard(TEXT("DeckB"), 0));
	Definitions.Add(Fixture.CreateCard(TEXT("DeckC"), 0));
	Deck->InitializeFromDefinitions(Definitions, 1337);

	const int32 InitialDrawCount = Deck->GetDrawCount();
	UCardInstance* FirstDraw = nullptr;
	const FCardZoneMutationResult Draw = Deck->TryDrawTopCardCommit(FirstDraw);
	TestTrue(TEXT("Draw commits"), Draw.bCommitted);
	TestEqual(TEXT("Draw FromZone"), Draw.FromZone, ECardZone::DrawPile);
	TestEqual(TEXT("Draw ToZone"), Draw.ToZone, ECardZone::Hand);
	TestEqual(TEXT("Draw starts at DrawPile top index"), Draw.FromIndex, InitialDrawCount - 1);
	TestEqual(TEXT("First draw enters Hand index zero"), Draw.ToIndex, 0);
	TestTrue(TEXT("Draw RuntimeId matches exact card"), IsValid(FirstDraw) && Draw.CardRuntimeId == FirstDraw->GetRuntimeId());
	TestTrue(TEXT("Draw CardId matches exact card"), IsValid(FirstDraw) && Draw.CardId == FirstDraw->GetCardId());

	UCardInstance* SecondDraw = nullptr;
	const FCardZoneMutationResult Draw2 = Deck->TryDrawTopCardCommit(SecondDraw);
	TestTrue(TEXT("Second draw commits"), Draw2.bCommitted);
	const TArray<int32> HandBeforePlay = HandRuntimeIds(Deck);
	TestEqual(TEXT("Two cards are in Hand"), HandBeforePlay.Num(), 2);

	const FCardZoneMutationResult BeginPlay = Deck->TryMoveHandCardToPlayAreaCommit(FirstDraw);
	TestTrue(TEXT("Hand to PlayArea commits"), BeginPlay.bCommitted);
	TestEqual(TEXT("Hand to PlayArea original index"), BeginPlay.FromIndex, 0);
	TestEqual(TEXT("First PlayArea insertion index"), BeginPlay.ToIndex, 0);
	const FCardZoneMutationResult Rollback = Deck->TryReturnPlayAreaCardToHandAtIndexCommit(FirstDraw, BeginPlay.FromIndex);
	TestTrue(TEXT("Exact rollback commits"), Rollback.bCommitted);
	TestEqual(TEXT("Rollback restores original Hand index"), Rollback.ToIndex, BeginPlay.FromIndex);
	TestTrue(TEXT("Rollback restores exact Hand order"), HandRuntimeIds(Deck) == HandBeforePlay);

	const FCardZoneMutationResult Discard = Deck->TryDiscardCardCommit(FirstDraw);
	TestTrue(TEXT("Hand to Discard commits"), Discard.bCommitted);
	TestEqual(TEXT("Discard FromZone"), Discard.FromZone, ECardZone::Hand);
	TestEqual(TEXT("Discard ToZone"), Discard.ToZone, ECardZone::DiscardPile);
	TestEqual(TEXT("Discard destination index"), Discard.ToIndex, 0);

	const int32 DrawBeforeNoopDiscard = Deck->GetDrawCount();
	const int32 HandBeforeNoopDiscard = Deck->GetHandCount();
	const int32 DiscardBeforeNoopDiscard = Deck->GetDiscardCount();
	const FCardZoneMutationResult DuplicateDiscard = Deck->TryDiscardCardCommit(FirstDraw);
	TestFalse(TEXT("Discarding a non-Hand card is a no-op"), DuplicateDiscard.bCommitted);
	TestEqual(TEXT("No-op discard keeps Draw count"), Deck->GetDrawCount(), DrawBeforeNoopDiscard);
	TestEqual(TEXT("No-op discard keeps Hand count"), Deck->GetHandCount(), HandBeforeNoopDiscard);
	TestEqual(TEXT("No-op discard keeps Discard count"), Deck->GetDiscardCount(), DiscardBeforeNoopDiscard);

	const FCardZoneMutationResult BeginPlay2 = Deck->TryMoveHandCardToPlayAreaCommit(SecondDraw);
	TestTrue(TEXT("Second card enters PlayArea"), BeginPlay2.bCommitted);
	const FCardZoneMutationResult Exhaust = Deck->TryMovePlayAreaCardToDestinationCommit(SecondDraw, ECardDestination::Exhaust);
	TestTrue(TEXT("PlayArea to Exhaust commits"), Exhaust.bCommitted);
	TestEqual(TEXT("Exhaust zone"), Exhaust.ToZone, ECardZone::ExhaustPile);
	TestEqual(TEXT("Exhaust destination index"), Exhaust.ToIndex, 0);

	UCardInstance* ThirdDraw = nullptr;
	TestTrue(TEXT("Third draw commits"), Deck->TryDrawTopCardCommit(ThirdDraw).bCommitted);
	TestTrue(TEXT("Third card enters PlayArea"), Deck->TryMoveHandCardToPlayAreaCommit(ThirdDraw).bCommitted);
	const FCardZoneMutationResult Removed = Deck->TryMovePlayAreaCardToDestinationCommit(ThirdDraw, ECardDestination::Removed);
	TestTrue(TEXT("PlayArea to Removed commits"), Removed.bCommitted);
	TestEqual(TEXT("Removed zone"), Removed.ToZone, ECardZone::RemovedPile);

	UDeckRuntime* InvalidTopDeck = NewObject<UDeckRuntime>(Fixture.World);
	TArray<TObjectPtr<UCardData>> InvalidDefinitions;
	InvalidDefinitions.Add(Fixture.CreateCard(TEXT("InvalidTop"), 0));
	InvalidTopDeck->InitializeFromDefinitions(InvalidDefinitions, 1337);
	TestTrue(TEXT("Invalid-top fixture mutates only test setup"), InvalidTopDeck->InvalidateDrawPileTopForTesting());
	const int32 InvalidDrawBefore = InvalidTopDeck->GetDrawCount();
	const int32 InvalidHandBefore = InvalidTopDeck->GetHandCount();
	UCardInstance* InvalidOut = nullptr;
	const FCardZoneMutationResult InvalidDraw = InvalidTopDeck->TryDrawTopCardCommit(InvalidOut);
	TestFalse(TEXT("Invalid top card does not commit"), InvalidDraw.bCommitted);
	TestNull(TEXT("Invalid top draw returns no card"), InvalidOut);
	TestEqual(TEXT("Invalid top does not pop DrawPile"), InvalidTopDeck->GetDrawCount(), InvalidDrawBefore);
	TestEqual(TEXT("Invalid top does not mutate Hand"), InvalidTopDeck->GetHandCount(), InvalidHandBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2CCardPlayedRecordTest,
	"SlayTheSpireDemo.Phase6UIA2C.Record.CardPlayed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2CCardPlayedRecordTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* CardDefinition = Fixture.CreateCard(TEXT("A2CStrike"), 1, ECardTargetType::None);
	if (!StartSingleCardFixture(*this, Fixture, CardDefinition)) return false;
	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* Card = Deck->GetFirstHandCard();
	if (!TestNotNull(TEXT("Opening card exists"), Card)) return false;
	const int32 RuntimeId = Card->GetRuntimeId();
	Fixture.ResetDeliveries();

	const FGameplayRequestResult PlayResult = Fixture.Battle->RequestPlayCard(Card, nullptr);
	TestTrue(TEXT("One-cost card request accepted"), PlayResult.IsAcceptedForResolution());
	Fixture.Flush();
	const FPresentationResolutionEnvelope* Envelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("CardPlayed envelope exists"), Envelope)) return false;
	TestEqual(TEXT("Exactly one CardPlayed"), CountRecords(*Envelope, EBattlePresentationRecordType::CardPlayed), 1);
	TestEqual(TEXT("Card cost does not emit EnergyChanged"), CountRecords(*Envelope, EBattlePresentationRecordType::EnergyChanged), 0);

	const FPresentationRecord* Played = FindFirstRecord(*Envelope, EBattlePresentationRecordType::CardPlayed);
	if (!TestNotNull(TEXT("CardPlayed record exists"), Played)) return false;
	TestEqual(TEXT("CardPlayed RuntimeId"), Played->CardPlayed.Card.RuntimeId, RuntimeId);
	TestEqual(TEXT("CardPlayed CardId"), Played->CardPlayed.Card.CardId, FName(TEXT("A2CStrike")));
	TestEqual(TEXT("CardPlayed frozen DisplayName"), Played->CardPlayed.Card.DisplayName.ToString(), FString(TEXT("A2CStrike")));
	TestEqual(TEXT("CardPlayed frozen Cost"), Played->CardPlayed.Card.Cost, 1);
	TestEqual(TEXT("CardPlayed Source id"), Played->CardPlayed.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestTrue(TEXT("Targetless CardPlayed keeps Target=None"), Played->CardPlayed.TargetPresentationId.IsNone());
	TestEqual(TEXT("CardPlayed Hand index"), Played->CardPlayed.HandIndexBefore, 0);
	TestEqual(TEXT("CardPlayed PlayArea index"), Played->CardPlayed.PlayAreaIndexAfter, 0);
	TestEqual(TEXT("CardPlayed Energy before"), Played->CardPlayed.EnergyBefore, 3);
	TestEqual(TEXT("CardPlayed Energy after"), Played->CardPlayed.EnergyAfter, 2);
	TestEqual(TEXT("CardPlayed CostPaid"), Played->CardPlayed.CostPaid, 1);
	TestEqual(TEXT("Card leaves Hand in final snapshot"), Envelope->FinalSnapshot.HandCards.Num(), 0);
	TestEqual(TEXT("One-cost card final energy"), Envelope->FinalSnapshot.Energy, 2);

	int32 HandToPlayAreaZoneRecords = 0;
	for (const FPresentationRecord& Record : Envelope->Records)
	{
		if (Record.Type == EBattlePresentationRecordType::CardZoneChanged
			&& Record.CardZoneChanged.FromZone == ECardZone::Hand
			&& Record.CardZoneChanged.ToZone == ECardZone::PlayArea)
		{
			++HandToPlayAreaZoneRecords;
		}
	}
	TestEqual(TEXT("CardPlayed absorbs Hand to PlayArea history"), HandToPlayAreaZoneRecords, 0);

	FFixture ZeroFixture;
	UCardData* ZeroDefinition = ZeroFixture.CreateCard(TEXT("A2CZero"), 0, ECardTargetType::None);
	if (!StartSingleCardFixture(*this, ZeroFixture, ZeroDefinition)) return false;
	UCardInstance* ZeroCard = ZeroFixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
	ZeroFixture.ResetDeliveries();
	TestTrue(TEXT("Zero-cost card request accepted"), ZeroFixture.Battle->RequestPlayCard(ZeroCard, nullptr).IsAcceptedForResolution());
	ZeroFixture.Flush();
	const FPresentationResolutionEnvelope* ZeroEnvelope = ZeroFixture.LastDelivery();
	if (!TestNotNull(TEXT("Zero-cost envelope"), ZeroEnvelope)) return false;
	const FPresentationRecord* ZeroPlayed = FindFirstRecord(*ZeroEnvelope, EBattlePresentationRecordType::CardPlayed);
	if (!TestNotNull(TEXT("Zero-cost CardPlayed"), ZeroPlayed)) return false;
	TestEqual(TEXT("Zero-cost CardPlayed CostPaid"), ZeroPlayed->CardPlayed.CostPaid, 0);
	TestEqual(TEXT("Zero-cost CardPlayed preserves Energy"), ZeroPlayed->CardPlayed.EnergyAfter, ZeroPlayed->CardPlayed.EnergyBefore);
	TestEqual(TEXT("Zero-cost play emits no EnergyChanged"), CountRecords(*ZeroEnvelope, EBattlePresentationRecordType::EnergyChanged), 0);

	FFixture RollbackFixture;
	UCardData* RollA = RollbackFixture.CreateCard(TEXT("RollbackA"), 1, ECardTargetType::None);
	UCardData* RollB = RollbackFixture.CreateCard(TEXT("RollbackB"), 1, ECardTargetType::None);
	TArray<UCardData*> RollCards;
	RollCards.Add(RollA);
	RollCards.Add(RollB);
	if (!RollbackFixture.Start(RollCards, 2)) return false;
	UDeckRuntime* RollDeck = RollbackFixture.Battle->GetDeckRuntimeForTesting();
	const TArray<int32> OriginalOrder = HandRuntimeIds(RollDeck);
	UCardInstance* RollCard = RollDeck->GetFirstHandCard();
	const int32 EnergyBeforeRollback = RollbackFixture.Battle->Energy;
	RollbackFixture.ResetDeliveries();
	RollbackFixture.Battle->SetForceNextEnergySpendFailureForTesting(true);
	TestTrue(TEXT("Rollback probe request is accepted before Execute-time spend"), RollbackFixture.Battle->RequestPlayCard(RollCard, nullptr).IsAcceptedForResolution());
	RollbackFixture.Flush();
	TestEqual(TEXT("Failed spend keeps energy unchanged"), RollbackFixture.Battle->Energy, EnergyBeforeRollback);
	TestTrue(TEXT("Failed spend restores exact Hand order"), HandRuntimeIds(RollDeck) == OriginalOrder);
	TestEqual(TEXT("Failed spend leaves PlayArea empty"), RollDeck->GetPlayAreaCount(), 0);
	TestFalse(TEXT("Successful exact rollback does not fault Gameplay"), RollbackFixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	const FPresentationResolutionEnvelope* RollEnvelope = RollbackFixture.LastDelivery();
	if (!TestNotNull(TEXT("Rollback no-op history envelope"), RollEnvelope)) return false;
	TestEqual(TEXT("Failed spend emits no CardPlayed"), CountRecords(*RollEnvelope, EBattlePresentationRecordType::CardPlayed), 0);
	TestEqual(TEXT("Internal rollback emits no zone record"), CountRecords(*RollEnvelope, EBattlePresentationRecordType::CardZoneChanged), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2CCardZoneChangedRecordTest,
	"SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2CCardZoneChangedRecordTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* CardDefinition = Fixture.CreateCard(TEXT("ZoneCard"), 0, ECardTargetType::None);
	if (!StartSingleCardFixture(*this, Fixture, CardDefinition, 0)) return false;
	Fixture.ResetDeliveries();

	Fixture.Battle->TestDrawCard();
	Fixture.Flush();
	const FPresentationResolutionEnvelope* DrawEnvelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Draw envelope"), DrawEnvelope)) return false;
	const FPresentationRecord* DrawRecord = FindFirstRecord(*DrawEnvelope, EBattlePresentationRecordType::CardZoneChanged);
	if (!TestNotNull(TEXT("Draw CardZoneChanged"), DrawRecord)) return false;
	TestEqual(TEXT("Draw FromZone"), DrawRecord->CardZoneChanged.FromZone, ECardZone::DrawPile);
	TestEqual(TEXT("Draw ToZone"), DrawRecord->CardZoneChanged.ToZone, ECardZone::Hand);
	TestEqual(TEXT("Draw ToIndex"), DrawRecord->CardZoneChanged.ToIndex, 0);
	TestEqual(TEXT("Draw record freezes CardId"), DrawRecord->CardZoneChanged.Card.CardId, FName(TEXT("ZoneCard")));
	TestEqual(TEXT("Draw final Hand contains card"), DrawEnvelope->FinalSnapshot.HandCards.Num(), 1);

	Fixture.ResetDeliveries();
	Fixture.Battle->TestDiscardCard();
	Fixture.Flush();
	const FPresentationResolutionEnvelope* DiscardEnvelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Discard envelope"), DiscardEnvelope)) return false;
	const FPresentationRecord* DiscardRecord = FindFirstRecord(*DiscardEnvelope, EBattlePresentationRecordType::CardZoneChanged);
	if (!TestNotNull(TEXT("Discard CardZoneChanged"), DiscardRecord)) return false;
	TestEqual(TEXT("Discard FromZone"), DiscardRecord->CardZoneChanged.FromZone, ECardZone::Hand);
	TestEqual(TEXT("Discard ToZone"), DiscardRecord->CardZoneChanged.ToZone, ECardZone::DiscardPile);
	TestEqual(TEXT("Discard final count"), DiscardEnvelope->FinalSnapshot.DiscardCount, 1);

	FFixture ExhaustFixture;
	UCardData* ExhaustDefinition = ExhaustFixture.CreateCard(
		TEXT("ExhaustCard"),
		0,
		ECardTargetType::None,
		ECardDestination::Exhaust
	);
	if (!StartSingleCardFixture(*this, ExhaustFixture, ExhaustDefinition)) return false;
	UCardInstance* ExhaustCard = ExhaustFixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
	ExhaustFixture.ResetDeliveries();
	TestTrue(TEXT("Exhaust card request accepted"), ExhaustFixture.Battle->RequestPlayCard(ExhaustCard, nullptr).IsAcceptedForResolution());
	ExhaustFixture.Flush();
	const FPresentationResolutionEnvelope* ExhaustEnvelope = ExhaustFixture.LastDelivery();
	if (!TestNotNull(TEXT("Exhaust envelope"), ExhaustEnvelope)) return false;
	const TArray<const FPresentationRecord*> ExhaustZones = FindRecords(*ExhaustEnvelope, EBattlePresentationRecordType::CardZoneChanged);
	TestEqual(TEXT("Exhaust play emits one finish zone record"), ExhaustZones.Num(), 1);
	if (ExhaustZones.Num() == 1)
	{
		TestEqual(TEXT("Finish FromZone is PlayArea"), ExhaustZones[0]->CardZoneChanged.FromZone, ECardZone::PlayArea);
		TestEqual(TEXT("Finish ToZone is Exhaust"), ExhaustZones[0]->CardZoneChanged.ToZone, ECardZone::ExhaustPile);
	}
	TestEqual(TEXT("Exhaust final count"), ExhaustEnvelope->FinalSnapshot.ExhaustCount, 1);

	FFixture RemovedFixture;
	UCardData* RemovedDefinition = RemovedFixture.CreateCard(
		TEXT("RemovedCard"),
		0,
		ECardTargetType::None,
		ECardDestination::Removed
	);
	if (!StartSingleCardFixture(*this, RemovedFixture, RemovedDefinition)) return false;
	UCardInstance* RemovedCard = RemovedFixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
	RemovedFixture.ResetDeliveries();
	TestTrue(TEXT("Removed card request accepted"), RemovedFixture.Battle->RequestPlayCard(RemovedCard, nullptr).IsAcceptedForResolution());
	RemovedFixture.Flush();
	const FPresentationResolutionEnvelope* RemovedEnvelope = RemovedFixture.LastDelivery();
	if (!TestNotNull(TEXT("Removed envelope"), RemovedEnvelope)) return false;
	const TArray<const FPresentationRecord*> RemovedZones = FindRecords(*RemovedEnvelope, EBattlePresentationRecordType::CardZoneChanged);
	TestEqual(TEXT("Removed play emits one finish zone record"), RemovedZones.Num(), 1);
	if (RemovedZones.Num() == 1)
	{
		TestEqual(TEXT("Removed finish FromZone"), RemovedZones[0]->CardZoneChanged.FromZone, ECardZone::PlayArea);
		TestEqual(TEXT("Removed finish ToZone"), RemovedZones[0]->CardZoneChanged.ToZone, ECardZone::RemovedPile);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2CShuffleOrderingTest,
	"SlayTheSpireDemo.Phase6UIA2C.Record.ShuffleOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2CShuffleOrderingTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* CardDefinition = Fixture.CreateCard(TEXT("ShuffleCard"), 0, ECardTargetType::None);
	if (!StartSingleCardFixture(*this, Fixture, CardDefinition, 1)) return false;
	Fixture.ResetDeliveries();
	Fixture.Battle->TestDiscardCard();
	Fixture.Flush();
	TestEqual(TEXT("Precondition DrawPile empty"), Fixture.Battle->GetDeckRuntimeForTesting()->GetDrawCount(), 0);
	TestEqual(TEXT("Precondition Discard contains card"), Fixture.Battle->GetDeckRuntimeForTesting()->GetDiscardCount(), 1);
	Fixture.ResetDeliveries();

	bool bSawDeckShuffledEvent = false;
	int32 ActiveRecordCountAtEvent = INDEX_NONE;
	UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
	UBattleEventDispatcher::OnEventDispatchedForTesting.AddLambda(
		[&](const FBattleEvent& Event)
		{
			if (Event.TryGet<FDeckShuffledEvent>() != nullptr)
			{
				bSawDeckShuffledEvent = true;
				if (UBattlePresentationRecorder* Recorder = Fixture.Battle->GetPresentationRecorderForTesting())
				{
					ActiveRecordCountAtEvent = Recorder->GetActiveRecordCountForTesting();
				}
			}
		}
	);

	Fixture.Battle->TestDrawCard();
	Fixture.Flush();
	UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
	const FPresentationResolutionEnvelope* Envelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Shuffle+RetryDraw envelope"), Envelope)) return false;
	TestTrue(TEXT("DeckShuffled gameplay event dispatched"), bSawDeckShuffledEvent);
	TestEqual(TEXT("DeckShuffled presentation fact exists before event dispatch"), ActiveRecordCountAtEvent, 1);
	TestEqual(TEXT("One DeckShuffled record"), CountRecords(*Envelope, EBattlePresentationRecordType::DeckShuffled), 1);
	TestEqual(TEXT("RetryDraw produces one zone record"), CountRecords(*Envelope, EBattlePresentationRecordType::CardZoneChanged), 1);
	if (Envelope->Records.Num() >= 2)
	{
		TestEqual(TEXT("History starts with DeckShuffled"), Envelope->Records[0].Type, EBattlePresentationRecordType::DeckShuffled);
		TestEqual(TEXT("RetryDraw follows shuffle"), Envelope->Records.Last().Type, EBattlePresentationRecordType::CardZoneChanged);
		TestTrue(TEXT("Shuffle sequence precedes retry draw"), Envelope->Records[0].PresentationSequence < Envelope->Records.Last().PresentationSequence);
	}
	const FPresentationRecord* ShuffleRecord = FindFirstRecord(*Envelope, EBattlePresentationRecordType::DeckShuffled);
	if (!TestNotNull(TEXT("DeckShuffled payload"), ShuffleRecord)) return false;
	TestEqual(TEXT("Shuffle moved one card"), ShuffleRecord->DeckShuffled.MovedCardCount, 1);
	TestEqual(TEXT("Shuffle draw before"), ShuffleRecord->DeckShuffled.DrawCountBefore, 0);
	TestEqual(TEXT("Shuffle draw after"), ShuffleRecord->DeckShuffled.DrawCountAfter, 1);
	TestEqual(TEXT("Shuffle discard before"), ShuffleRecord->DeckShuffled.DiscardCountBefore, 1);
	TestEqual(TEXT("Shuffle discard after"), ShuffleRecord->DeckShuffled.DiscardCountAfter, 0);
	TestEqual(TEXT("Retry draw leaves card in Hand"), Envelope->FinalSnapshot.HandCards.Num(), 1);
	TestEqual(TEXT("Retry draw leaves DrawPile empty"), Envelope->FinalSnapshot.DrawCount, 0);

	FFixture OpeningFixture;
	UCardData* OpeningCard = OpeningFixture.CreateCard(TEXT("OpeningNoHistory"), 0);
	if (!StartSingleCardFixture(*this, OpeningFixture, OpeningCard, 1)) return false;
	TestEqual(TEXT("BattleStart publishes one opening envelope"), OpeningFixture.Deliveries.Num(), 1);
	if (OpeningFixture.Deliveries.Num() == 1)
	{
		TestEqual(TEXT("Opening setup emits no DeckShuffled"), CountRecords(OpeningFixture.Deliveries[0], EBattlePresentationRecordType::DeckShuffled), 0);
		TestEqual(TEXT("Opening setup emits no CardZoneChanged"), CountRecords(OpeningFixture.Deliveries[0], EBattlePresentationRecordType::CardZoneChanged), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2CEndTurnEnergyTest,
	"SlayTheSpireDemo.Phase6UIA2C.Record.EndTurnEnergy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2CEndTurnEnergyTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* CardDefinition = Fixture.CreateCard(TEXT("EndTurnCard"), 0);
	if (!StartSingleCardFixture(*this, Fixture, CardDefinition, 1, 0, 0)) return false;
	Fixture.ResetDeliveries();
	TestEqual(TEXT("Player starts turn at max energy"), Fixture.Battle->Energy, 3);
	TestTrue(TEXT("EndTurn accepted"), Fixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	Fixture.Flush();
	const FPresentationResolutionEnvelope* Envelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("EndTurn envelope"), Envelope)) return false;
	const TArray<const FPresentationRecord*> EnergyRecords = FindRecords(*Envelope, EBattlePresentationRecordType::EnergyChanged);
	TestEqual(TEXT("Surviving EndTurn has clear and restore Energy records"), EnergyRecords.Num(), 2);
	if (EnergyRecords.Num() == 2)
	{
		TestEqual(TEXT("EndTurn clear before"), EnergyRecords[0]->EnergyChanged.EnergyBefore, 3);
		TestEqual(TEXT("EndTurn clear after"), EnergyRecords[0]->EnergyChanged.EnergyAfter, 0);
		TestEqual(TEXT("EndTurn clear delta"), EnergyRecords[0]->EnergyChanged.Delta, -3);
		TestEqual(TEXT("Next player turn restore before"), EnergyRecords[1]->EnergyChanged.EnergyBefore, 0);
		TestEqual(TEXT("Next player turn restore after"), EnergyRecords[1]->EnergyChanged.EnergyAfter, 3);
		TestEqual(TEXT("Next player turn restore delta"), EnergyRecords[1]->EnergyChanged.Delta, 3);
		TestTrue(TEXT("Energy records preserve macro order"), EnergyRecords[0]->PresentationSequence < EnergyRecords[1]->PresentationSequence);
	}
	TestEqual(TEXT("Final snapshot restored max energy"), Envelope->FinalSnapshot.Energy, 3);

	FFixture LethalFixture;
	UCardData* LethalCard = LethalFixture.CreateCard(TEXT("LethalEndTurnCard"), 0);
	if (!StartSingleCardFixture(*this, LethalFixture, LethalCard, 1, 0, 10)) return false;
	LethalFixture.Player->HP = 5;
	LethalFixture.ResetDeliveries();
	TestTrue(TEXT("Lethal EndTurn accepted"), LethalFixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	LethalFixture.Flush();
	const FPresentationResolutionEnvelope* LethalEnvelope = LethalFixture.LastDelivery();
	if (!TestNotNull(TEXT("Lethal EndTurn envelope"), LethalEnvelope)) return false;
	TestEqual(TEXT("Lethal EndTurn has only energy clear"), CountRecords(*LethalEnvelope, EBattlePresentationRecordType::EnergyChanged), 1);
	TestEqual(TEXT("Lethal EndTurn ends in Defeat"), LethalEnvelope->FinalSnapshot.BattleState, EBattleState::Defeat);
	TestEqual(TEXT("Terminal energy normalization relies on final snapshot"), LethalEnvelope->FinalSnapshot.Energy, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2CWorkingSnapshotPlaybackTest,
	"SlayTheSpireDemo.Phase6UIA2C.Playback.WorkingSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2CWorkingSnapshotPlaybackTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* CardDefinition = Fixture.CreateCard(TEXT("WorkingStrike"), 1, ECardTargetType::Enemy);
	Fixture.AddDamageEffect(CardDefinition, 5);
	if (!StartSingleCardFixture(*this, Fixture, CardDefinition, 1)) return false;
	UCardInstance* Card = Fixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
	if (!TestNotNull(TEXT("Working-snapshot card"), Card)) return false;

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	TestTrue(TEXT("Presentation-owned ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true));
	UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	Widget->bAcceptAsyncPlayback = true;
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	TestTrue(TEXT("Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, Widget));
	Fixture.ResetDeliveries();

	TestTrue(TEXT("Damage card request accepted"), Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy).IsAcceptedForResolution());
	Fixture.Flush();
	const FPresentationResolutionEnvelope* Envelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Working-snapshot envelope"), Envelope)) return false;
	TestTrue(TEXT("CardPlayed is first record"), Envelope->Records.Num() >= 3 && Envelope->Records[0].Type == EBattlePresentationRecordType::CardPlayed);
	TestTrue(TEXT("Controller waits on CardPlayed"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Before CardPlayed completion Hand still shows card"), ViewModel->HandCards.Num(), 1);
	TestEqual(TEXT("Before CardPlayed completion Energy is old value"), ViewModel->Energy, 3);

	FPresentationPlaybackToken CardPlayedToken = Controller->GetActivePlaybackTokenForTesting();
	Controller->NotifyPresentationFinished(CardPlayedToken);
	TestEqual(TEXT("CardPlayed reducer removes Hand card before Damage"), ViewModel->HandCards.Num(), 0);
	TestEqual(TEXT("CardPlayed reducer applies EnergyAfter before Damage"), ViewModel->Energy, 2);
	FPresentationStateSnapshot WorkingAfterPlay;
	TestTrue(TEXT("Controller exposes working snapshot"), Controller->TryGetWorkingSnapshotForTesting(WorkingAfterPlay));
	TestEqual(TEXT("Working snapshot Hand after CardPlayed"), WorkingAfterPlay.HandCards.Num(), 0);
	TestEqual(TEXT("Working snapshot Energy after CardPlayed"), WorkingAfterPlay.Energy, 2);
	TestTrue(TEXT("Controller advances to Damage"), Controller->IsWaitingForCompletionForTesting());

	const int32 EnemyHPBeforeDamageDisplay = ViewModel->Enemy.HP;
	FPresentationPlaybackToken DamageToken = Controller->GetActivePlaybackTokenForTesting();
	Controller->NotifyPresentationFinished(DamageToken);
	TestEqual(TEXT("Damage reducer applies HPAfter before finish-zone record"), ViewModel->Enemy.HP, EnemyHPBeforeDamageDisplay - 5);
	TestTrue(TEXT("Controller advances to finish-zone record"), Controller->IsWaitingForCompletionForTesting());

	FPresentationPlaybackToken ZoneToken = Controller->GetActivePlaybackTokenForTesting();
	Controller->NotifyPresentationFinished(ZoneToken);
	TestFalse(TEXT("Envelope completes after finish-zone record"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Final snapshot keeps card out of Hand"), ViewModel->HandCards.Num(), 0);
	TestEqual(TEXT("Final snapshot has discard count"), ViewModel->DiscardCount, 1);
	TestEqual(TEXT("Final snapshot keeps energy"), ViewModel->Energy, 2);

	Widget->bAcceptAsyncPlayback = false;
	FFixture FallbackFixture;
	UCardData* FallbackCardDefinition = FallbackFixture.CreateCard(TEXT("FallbackCard"), 1, ECardTargetType::None);
	if (!StartSingleCardFixture(*this, FallbackFixture, FallbackCardDefinition, 1)) return false;
	UBattleHUDViewModel* FallbackVM = NewObject<UBattleHUDViewModel>(FallbackFixture.World);
	TestTrue(TEXT("Fallback VM initializes"), FallbackVM->Initialize(FallbackFixture.Battle, true));
	UPhase6UIA2APlaybackWidget* FallbackWidget = NewObject<UPhase6UIA2APlaybackWidget>(FallbackFixture.World);
	FallbackWidget->bAcceptAsyncPlayback = false;
	UBattlePresentationController* FallbackController = NewObject<UBattlePresentationController>(FallbackFixture.World);
	TestTrue(TEXT("Fallback controller initializes"), FallbackController->Initialize(FallbackFixture.Battle, FallbackVM, FallbackWidget));
	UCardInstance* FallbackCard = FallbackFixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
	FallbackFixture.ResetDeliveries();
	TestTrue(TEXT("Fallback card request accepted"), FallbackFixture.Battle->RequestPlayCard(FallbackCard, nullptr).IsAcceptedForResolution());
	FallbackFixture.Flush();
	TestFalse(TEXT("Blueprint false never waits for timeout"), FallbackController->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Immediate fallback reaches final Hand"), FallbackVM->HandCards.Num(), 0);
	TestEqual(TEXT("Immediate fallback reaches final Energy"), FallbackVM->Energy, 2);

	Controller->Shutdown();
	FallbackController->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2CPresentationFailureIsolationTest,
	"SlayTheSpireDemo.Phase6UIA2C.Failure.PresentationDoesNotAffectGameplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2CPresentationFailureIsolationTest::RunTest(const FString& Parameters)
{
	FFixture NoHistoryFixture;
	UCardData* NoHistoryDefinition = NoHistoryFixture.CreateCard(TEXT("NoHistoryCard"), 1, ECardTargetType::None);
	if (!StartSingleCardFixture(*this, NoHistoryFixture, NoHistoryDefinition, 1, 0, 0, false)) return false;
	UCardInstance* NoHistoryCard = NoHistoryFixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
	const int32 NoHistoryEnergyBefore = NoHistoryFixture.Battle->Energy;
	NoHistoryFixture.ResetDeliveries();
	TestTrue(TEXT("No-history card request accepted"), NoHistoryFixture.Battle->RequestPlayCard(NoHistoryCard, nullptr).IsAcceptedForResolution());
	NoHistoryFixture.Flush();
	TestEqual(TEXT("No-history card still leaves Hand"), NoHistoryFixture.Battle->GetDeckRuntimeForTesting()->GetHandCount(), 0);
	TestEqual(TEXT("No-history card still spends energy"), NoHistoryFixture.Battle->Energy, NoHistoryEnergyBefore - 1);
	TestTrue(TEXT("No-history mode remains Presentation-available"), NoHistoryFixture.Battle->IsPresentationAvailable());
	TestFalse(TEXT("No-history mode does not Gameplay-fault"), NoHistoryFixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	TestEqual(TEXT("No-history mode publishes no historical Envelope"), NoHistoryFixture.Deliveries.Num(), 0);

	FFixture InvalidPayloadFixture;
	UCardData* InvalidDefinition = InvalidPayloadFixture.CreateCard(TEXT("TemporaryId"), 1, ECardTargetType::None);
	InvalidDefinition->CardId = NAME_None;
	if (!StartSingleCardFixture(*this, InvalidPayloadFixture, InvalidDefinition, 1)) return false;
	UCardInstance* InvalidCard = InvalidPayloadFixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
	const int32 InvalidEnergyBefore = InvalidPayloadFixture.Battle->Energy;
	InvalidPayloadFixture.ResetDeliveries();
	TestTrue(TEXT("Invalid-history card request still accepted"), InvalidPayloadFixture.Battle->RequestPlayCard(InvalidCard, nullptr).IsAcceptedForResolution());
	InvalidPayloadFixture.Flush();
	TestEqual(TEXT("Invalid frozen payload cannot roll back Gameplay zone"), InvalidPayloadFixture.Battle->GetDeckRuntimeForTesting()->GetHandCount(), 0);
	TestEqual(TEXT("Invalid frozen payload cannot roll back energy"), InvalidPayloadFixture.Battle->Energy, InvalidEnergyBefore - 1);
	TestFalse(TEXT("Invalid frozen payload disables Presentation only"), InvalidPayloadFixture.Battle->IsPresentationAvailable());
	TestFalse(TEXT("Invalid frozen payload does not Gameplay-fault"), InvalidPayloadFixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	TestEqual(TEXT("Invalid history publishes no partial Envelope"), InvalidPayloadFixture.Deliveries.Num(), 0);
	FPresentationStateSnapshot InvalidBaseline;
	TestTrue(TEXT("Invalid history still freezes latest baseline"), InvalidPayloadFixture.Battle->TryGetLatestFrozenPresentationBaseline(InvalidBaseline));
	TestEqual(TEXT("Invalid history baseline reflects committed energy"), InvalidBaseline.Energy, InvalidPayloadFixture.Battle->Energy);

	FFixture AppendFailureFixture;
	UCardData* AppendDefinition = AppendFailureFixture.CreateCard(TEXT("AppendFailureCard"), 1, ECardTargetType::None);
	if (!StartSingleCardFixture(*this, AppendFailureFixture, AppendDefinition, 1)) return false;
	UCardInstance* AppendCard = AppendFailureFixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
	const int32 AppendEnergyBefore = AppendFailureFixture.Battle->Energy;
	AppendFailureFixture.ResetDeliveries();
	UBattlePresentationRecorder* Recorder = AppendFailureFixture.Battle->GetPresentationRecorderForTesting();
	if (!TestNotNull(TEXT("Append-failure recorder"), Recorder)) return false;
	Recorder->SetForceNextAppendFailureForTesting(true);
	TestTrue(TEXT("Append-failure card request accepted"), AppendFailureFixture.Battle->RequestPlayCard(AppendCard, nullptr).IsAcceptedForResolution());
	AppendFailureFixture.Flush();
	TestEqual(TEXT("Append failure cannot roll back card Gameplay"), AppendFailureFixture.Battle->GetDeckRuntimeForTesting()->GetHandCount(), 0);
	TestEqual(TEXT("Append failure cannot roll back energy Gameplay"), AppendFailureFixture.Battle->Energy, AppendEnergyBefore - 1);
	TestFalse(TEXT("Append failure disables Presentation only"), AppendFailureFixture.Battle->IsPresentationAvailable());
	TestFalse(TEXT("Append failure does not Gameplay-fault"), AppendFailureFixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	TestEqual(TEXT("Append failure publishes no partial Envelope"), AppendFailureFixture.Deliveries.Num(), 0);
	FPresentationStateSnapshot AppendBaseline;
	TestTrue(TEXT("Append failure freezes latest baseline"), AppendFailureFixture.Battle->TryGetLatestFrozenPresentationBaseline(AppendBaseline));
	TestEqual(TEXT("Append failure baseline reflects committed energy"), AppendBaseline.Energy, AppendFailureFixture.Battle->Energy);
	return true;
}

#endif
