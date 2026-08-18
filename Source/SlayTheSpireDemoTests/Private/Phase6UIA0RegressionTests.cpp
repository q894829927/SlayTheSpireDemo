#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Actions/BattleActionQueue.h"
#include "../Battle/BattleManager.h"
#include "../Battle/BattleReadSnapshot.h"
#include "../Cards/CardData.h"
#include "../Cards/CardInstance.h"
#include "../Cards/Effects/DamageCardEffect.h"
#include "../Combat/Combatant.h"
#include "../Deck/DeckRuntime.h"
#include "../Enemy/EnemyIntent.h"
#include "Engine/World.h"

namespace Phase6UIA0Regression
{
	UCardData* CreateCard(
		UObject* Outer,
		const TCHAR* CardId,
		ECardTargetType TargetType,
		int32 Cost,
		int32 Damage = 0
	)
	{
		UCardData* Definition = NewObject<UCardData>(Outer);
		Definition->CardId = FName(CardId);
		Definition->DisplayName = FText::FromString(CardId);
		Definition->BaseCost = Cost;
		Definition->TargetType = TargetType;
		Definition->DefaultDestination = ECardDestination::Discard;

		if (Damage > 0)
		{
			UDamageCardEffect* DamageEffect = NewObject<UDamageCardEffect>(Definition);
			DamageEffect->BaseAmount = Damage;
			DamageEffect->DamageKind = EDamageKind::Attack;
			Definition->Effects.Add(DamageEffect);
		}
		return Definition;
	}

	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UCardData* AttackDefinition = nullptr;

		FFixture(
			int32 DeckCount = 5,
			int32 OpeningDraw = 5,
			int32 TurnDraw = 5,
			int32 EnemyDamage = 5,
			int32 CardCost = 1
		)
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
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->MaxEnergy = 3;
			Battle->OpeningHandDrawCount = OpeningDraw;
			Battle->PlayerTurnDrawCount = TurnDraw;
			Battle->EnemyTestAttackDamage = EnemyDamage;

			AttackDefinition = CreateCard(World, TEXT("UIA0_Strike"), ECardTargetType::Enemy, CardCost, 6);
			for (int32 Index = 0; Index < DeckCount; ++Index)
			{
				Battle->DebugStartingDeck.Add(AttackDefinition);
			}

			Battle->StartBattle();
		}

		~FFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
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
	};

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (!Fixture.IsReady())
		{
			Test.AddError(TEXT("Failed to create the transient Phase 6UI-A0 battle fixture."));
			return false;
		}
		return true;
	}

	UCardInstance* FirstHandCard(ABattleManager* Battle)
	{
		return IsValid(Battle) && IsValid(Battle->GetDeckRuntimeForTesting())
			? Battle->GetDeckRuntimeForTesting()->GetFirstHandCard()
			: nullptr;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6UIA0OpeningHandLifecycleTest,
		"SlayTheSpireDemo.Phase6UIA0.Turn.OpeningHandLifecycle",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6UIA0OpeningHandLifecycleTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture(5, 5, 5, 5, 1);
		if (!RequireReady(*this, Fixture)) return false;

		FBattleReadSnapshot Snapshot;
		TestTrue(TEXT("Opening player-ready snapshot is available"), Fixture.Battle->TryBuildReadSnapshot(Snapshot));
		TestEqual(TEXT("Opening Hand contains five cards"), Snapshot.HandCount, 5);
		TestEqual(TEXT("Opening draw consumed the DrawPile"), Snapshot.DrawCount, 0);
		TestEqual(TEXT("PlayerTurn is request-eligible only after opening draws complete"), Snapshot.BattleState, EBattleState::PlayerTurn);
		TestEqual(TEXT("Opening turn restores Energy"), Snapshot.Energy, 3);
		TestEqual(TEXT("Committed initial Enemy Intent is Attack"), Snapshot.EnemyIntent.Type, EEnemyIntentType::Attack);
		TestEqual(TEXT("Committed initial Enemy Intent preserves amount"), Snapshot.EnemyIntent.BaseAmount, 5);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6UIA0EndCleanupBeforeTurnBoundaryTest,
		"SlayTheSpireDemo.Phase6UIA0.Turn.HandCleanupBeforeTurnEndedBoundary",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6UIA0EndCleanupBeforeTurnBoundaryTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture(5, 5, 0, 5, 1);
		if (!RequireReady(*this, Fixture)) return false;

		int32 HandAtPlayerEndingBoundary = INDEX_NONE;
		int32 DiscardAtPlayerEndingBoundary = INDEX_NONE;
		UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
		ABattleManager* Battle = Fixture.Battle;
		Fixture.Battle->GetActionQueueForTesting()->OnQueueEmpty.AddLambda(
			[Battle, Deck, &HandAtPlayerEndingBoundary, &DiscardAtPlayerEndingBoundary]()
			{
				if (Battle->BattleState == EBattleState::PlayerTurnEnding)
				{
					HandAtPlayerEndingBoundary = Deck->GetHandCount();
					DiscardAtPlayerEndingBoundary = Deck->GetDiscardCount();
				}
			}
		);

		const FGameplayRequestResult Result = Fixture.Battle->RequestEndPlayerTurn();
		TestTrue(TEXT("Formal EndTurn request is accepted for resolution"), Result.IsAcceptedForResolution());
		TestEqual(TEXT("Hand cleanup committed before player TurnEnded boundary"), HandAtPlayerEndingBoundary, 0);
		TestEqual(TEXT("All five cards are already in Discard at player TurnEnded boundary"), DiscardAtPlayerEndingBoundary, 5);
		TestEqual(TEXT("Zero turn draw returns to input-ready PlayerTurn"), Fixture.Battle->BattleState, EBattleState::PlayerTurn);
		TestEqual(TEXT("No cards are redrawn when PlayerTurnDrawCount is zero"), Deck->GetHandCount(), 0);
		TestEqual(TEXT("Discard remains authoritative after zero-draw turn start"), Deck->GetDiscardCount(), 5);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6UIA0PlayerTurnStartingShuffleDrawTest,
		"SlayTheSpireDemo.Phase6UIA0.Turn.PlayerTurnStartingDrawsAfterEnemy",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6UIA0PlayerTurnStartingShuffleDrawTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture(5, 5, 5, 5, 1);
		if (!RequireReady(*this, Fixture)) return false;

		TArray<EBattleState> ObservedStates;
		ABattleManager* Battle = Fixture.Battle;
		Fixture.Battle->GetActionQueueForTesting()->OnQueueEmpty.AddLambda(
			[Battle, &ObservedStates]()
			{
				ObservedStates.Add(Battle->BattleState);
			}
		);

		Fixture.Battle->RequestEndPlayerTurn();

		TestEqual(TEXT("End turn cycle exposes player-ending, enemy-ending and turn-starting QueueEmpty boundaries"), ObservedStates.Num(), 3);
		if (ObservedStates.Num() == 3)
		{
			TestEqual(TEXT("Boundary 1 is PlayerTurnEnding"), ObservedStates[0], EBattleState::PlayerTurnEnding);
			TestEqual(TEXT("Boundary 2 is EnemyTurnEnding"), ObservedStates[1], EBattleState::EnemyTurnEnding);
			TestEqual(TEXT("Boundary 3 is PlayerTurnStarting"), ObservedStates[2], EBattleState::PlayerTurnStarting);
		}

		UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
		TestEqual(TEXT("Turn-start Draw/Shuffle resolution finishes before PlayerTurn"), Fixture.Battle->BattleState, EBattleState::PlayerTurn);
		TestEqual(TEXT("Five cards are redrawn after cleanup and shuffle"), Deck->GetHandCount(), 5);
		TestEqual(TEXT("Discard is consumed by turn-start shuffle"), Deck->GetDiscardCount(), 0);
		TestEqual(TEXT("DrawPile is consumed by five redraws"), Deck->GetDrawCount(), 0);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6UIA0RequestTargetValidationTest,
		"SlayTheSpireDemo.Phase6UIA0.Request.QueryAndRequestShareTargetValidation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6UIA0RequestTargetValidationTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture(1, 1, 0, 5, 1);
		if (!RequireReady(*this, Fixture)) return false;

		UCardInstance* Card = FirstHandCard(Fixture.Battle);
		TestNotNull(TEXT("Opening Hand exposes a runtime CardInstance"), Card);
		if (!Card) return false;

		const FGameplayValidationResult Playability = Fixture.Battle->QueryCardPlayability(Card);
		TestTrue(TEXT("Card is generally playable before target selection"), Playability.bAllowed);

		const FGameplayValidationResult Query = Fixture.Battle->QueryPlayCard(Card, nullptr);
		TestFalse(TEXT("Enemy-target card rejects null target in Query"), Query.bAllowed);
		TestEqual(TEXT("Query reports structured InvalidTarget"), Query.FailureReason, EGameplayRequestFailureReason::InvalidTarget);

		const FGameplayRequestResult RejectedRequest = Fixture.Battle->RequestPlayCard(Card, nullptr);
		TestFalse(TEXT("Request revalidation rejects the same invalid target"), RejectedRequest.IsAcceptedForResolution());
		TestEqual(TEXT("Request uses the same structured InvalidTarget reason"), RejectedRequest.FailureReason, Query.FailureReason);

		const FGameplayRequestResult AcceptedRequest = Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy);
		TestTrue(TEXT("Valid target is AcceptedForResolution"), AcceptedRequest.IsAcceptedForResolution());
		TestEqual(TEXT("Resolved card spends one Energy"), Fixture.Battle->Energy, 2);
		TestEqual(TEXT("Resolved card deals its configured six damage"), Fixture.Enemy->HP, 94);
		TestEqual(TEXT("Resolved card leaves Hand"), Fixture.Battle->GetDeckRuntimeForTesting()->GetHandCount(), 0);
		TestEqual(TEXT("Resolved card reaches Discard"), Fixture.Battle->GetDeckRuntimeForTesting()->GetDiscardCount(), 1);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6UIA0RequestRevalidatesCardZoneTest,
		"SlayTheSpireDemo.Phase6UIA0.Request.RevalidatesCurrentCardZone",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6UIA0RequestRevalidatesCardZoneTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture(1, 1, 0, 5, 0);
		if (!RequireReady(*this, Fixture)) return false;

		UCardInstance* Card = FirstHandCard(Fixture.Battle);
		if (!Card) return false;

		TestTrue(TEXT("First request is accepted"), Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy).IsAcceptedForResolution());

		const FGameplayValidationResult QueryAfterCommit = Fixture.Battle->QueryPlayCard(Card, Fixture.Enemy);
		TestFalse(TEXT("Query sees that the card is no longer in Hand"), QueryAfterCommit.bAllowed);
		TestEqual(TEXT("Query reports CardNoLongerInHand"), QueryAfterCommit.FailureReason, EGameplayRequestFailureReason::CardNoLongerInHand);

		const FGameplayRequestResult SecondRequest = Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy);
		TestFalse(TEXT("Request revalidates instead of trusting stale prior playability"), SecondRequest.IsAcceptedForResolution());
		TestEqual(TEXT("Request reports the same current-zone failure"), SecondRequest.FailureReason, EGameplayRequestFailureReason::CardNoLongerInHand);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6UIA0EnergyValidationTest,
		"SlayTheSpireDemo.Phase6UIA0.Request.EnergyFailureIsShared",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6UIA0EnergyValidationTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture(1, 1, 0, 5, 4);
		if (!RequireReady(*this, Fixture)) return false;

		UCardInstance* Card = FirstHandCard(Fixture.Battle);
		if (!Card) return false;

		const FGameplayValidationResult Query = Fixture.Battle->QueryPlayCard(Card, Fixture.Enemy);
		TestFalse(TEXT("Query rejects unaffordable card"), Query.bAllowed);
		TestEqual(TEXT("Query reports NotEnoughEnergy"), Query.FailureReason, EGameplayRequestFailureReason::NotEnoughEnergy);

		const FGameplayRequestResult Request = Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy);
		TestFalse(TEXT("Request rejects unaffordable card"), Request.IsAcceptedForResolution());
		TestEqual(TEXT("Request shares NotEnoughEnergy result"), Request.FailureReason, Query.FailureReason);
		TestEqual(TEXT("Rejected request does not move the card"), Fixture.Battle->GetDeckRuntimeForTesting()->GetHandCount(), 1);
		TestEqual(TEXT("Rejected request does not spend Energy"), Fixture.Battle->Energy, 3);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6UIA0CommittedIntentDrivesExecutionTest,
		"SlayTheSpireDemo.Phase6UIA0.Intent.CommittedIntentDrivesExecution",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6UIA0CommittedIntentDrivesExecutionTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture(0, 0, 0, 5, 1);
		if (!RequireReady(*this, Fixture)) return false;

		FBattleReadSnapshot Before;
		TestTrue(TEXT("Initial snapshot is available"), Fixture.Battle->TryBuildReadSnapshot(Before));
		TestEqual(TEXT("Displayed committed Intent is Attack 5"), Before.EnemyIntent.BaseAmount, 5);

		Fixture.Battle->EnemyTestAttackDamage = 99;
		Fixture.Battle->RequestEndPlayerTurn();

		TestEqual(TEXT("Current EnemyTurn executes the previously committed Attack 5"), Fixture.Player->HP, 95);

		FBattleReadSnapshot After;
		TestTrue(TEXT("Next player-ready snapshot is available"), Fixture.Battle->TryBuildReadSnapshot(After));
		TestEqual(TEXT("After current Intent resolves, next Intent commits the new generator value"), After.EnemyIntent.BaseAmount, 99);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6UIA0SnapshotRevisionAndIdentityTest,
		"SlayTheSpireDemo.Phase6UIA0.Snapshot.CoherentRevisionAndCardIdentity",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6UIA0SnapshotRevisionAndIdentityTest::RunTest(const FString& Parameters)
	{
		FFixture Fixture(2, 2, 0, 5, 1);
		if (!RequireReady(*this, Fixture)) return false;

		FBattleReadSnapshot Before;
		if (!Fixture.Battle->TryBuildReadSnapshot(Before) || Before.HandCards.Num() != 2)
		{
			AddError(TEXT("Expected a coherent two-card opening snapshot."));
			return false;
		}

		UCardInstance* PlayedCard = Before.HandCards[0].Card.Get();
		const int32 RuntimeId = Before.HandCards[0].RuntimeId;
		const uint64 BeforeRevision = Before.StateRevision;
		TestTrue(TEXT("Card request is accepted"), Fixture.Battle->RequestPlayCard(PlayedCard, Fixture.Enemy).IsAcceptedForResolution());

		FBattleReadSnapshot After;
		TestTrue(TEXT("Post-resolution coherent snapshot is available"), Fixture.Battle->TryBuildReadSnapshot(After));
		TestTrue(TEXT("Gameplay state revision advances after the committed card resolution"), After.StateRevision > BeforeRevision);
		TestEqual(TEXT("Snapshot coherently observes one remaining Hand card"), After.HandCount, 1);
		TestEqual(TEXT("Snapshot coherently observes one Discard card"), After.DiscardCount, 1);
		TestEqual(TEXT("Snapshot coherently observes post-play Energy"), After.Energy, 2);
		TestEqual(TEXT("Snapshot coherently observes post-play Enemy HP"), After.Enemy.HP, 94);

		bool bFoundSameIdentity = false;
		for (const FCardReadView& CardView : After.DiscardCards)
		{
			if (CardView.Card.Get() == PlayedCard && CardView.RuntimeId == RuntimeId)
			{
				bFoundSameIdentity = true;
				break;
			}
		}
		TestTrue(TEXT("Read model preserves stable runtime card identity across zone movement"), bFoundSameIdentity);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase6UIA0LegalTargetSetTest,
		"SlayTheSpireDemo.Phase6UIA0.Targets.AuthoritativeLegalTargetSet",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FPhase6UIA0LegalTargetSetTest::RunTest(const FString& Parameters)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
		if (!IsValid(World)) return false;

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatant* Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
		ACombatant* Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
		ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);

		UCardData* EnemyCard = CreateCard(World, TEXT("TargetEnemy"), ECardTargetType::Enemy, 0);
		UCardData* SelfCard = CreateCard(World, TEXT("TargetSelf"), ECardTargetType::Self, 0);
		UCardData* NoneCard = CreateCard(World, TEXT("TargetNone"), ECardTargetType::None, 0);

		Player->MaxHP = 100;
		Enemy->MaxHP = 100;
		Battle->Player = Player;
		Battle->Enemy = Enemy;
		Battle->OpeningHandDrawCount = 3;
		Battle->PlayerTurnDrawCount = 0;
		Battle->DebugStartingDeck = {EnemyCard, SelfCard, NoneCard};
		Battle->StartBattle();

		FBattleReadSnapshot Snapshot;
		if (!Battle->TryBuildReadSnapshot(Snapshot))
		{
			World->DestroyWorld(false);
			return false;
		}

		UCardInstance* RuntimeEnemyCard = nullptr;
		UCardInstance* RuntimeSelfCard = nullptr;
		UCardInstance* RuntimeNoneCard = nullptr;
		for (const FCardReadView& CardView : Snapshot.HandCards)
		{
			if (CardView.CardId == TEXT("TargetEnemy")) RuntimeEnemyCard = CardView.Card.Get();
			if (CardView.CardId == TEXT("TargetSelf")) RuntimeSelfCard = CardView.Card.Get();
			if (CardView.CardId == TEXT("TargetNone")) RuntimeNoneCard = CardView.Card.Get();
		}

		TArray<ACombatant*> Targets;
		Battle->GetLegalTargetsForCard(RuntimeEnemyCard, Targets);
		TestEqual(TEXT("Enemy-target card exposes exactly one authoritative legal target"), Targets.Num(), 1);
		TestTrue(TEXT("Enemy-target legal target is the authoritative Enemy"), Targets.Num() == 1 && Targets[0] == Enemy);

		Battle->GetLegalTargetsForCard(RuntimeSelfCard, Targets);
		TestEqual(TEXT("Self-target card exposes exactly one authoritative legal target"), Targets.Num(), 1);
		TestTrue(TEXT("Self-target legal target is the authoritative Player"), Targets.Num() == 1 && Targets[0] == Player);

		Battle->GetLegalTargetsForCard(RuntimeNoneCard, Targets);
		TestEqual(TEXT("No-target card exposes no target candidates"), Targets.Num(), 0);
		TestTrue(TEXT("No-target card validates with nullptr target"), Battle->QueryPlayCard(RuntimeNoneCard, nullptr).bAllowed);

		World->DestroyWorld(false);
		return true;
	}
}

#endif
