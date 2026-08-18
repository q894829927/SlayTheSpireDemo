#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleReadSnapshot.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Combat/Combatant.h"
#include "Containers/Ticker.h"
#include "Deck/DeckRuntime.h"
#include "Events/BattleEvent.h"
#include "Events/BattleEventDispatcher.h"
#include "Events/TurnEndStatusDecayTrigger.h"
#include "Modifiers/Damage/DamageFlatAddModifier.h"
#include "Modifiers/Damage/DamageRatioModifier.h"
#include "Modifiers/ModifierTypes.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Status/StatusInstance.h"
#include "Phase6UIA0TestTypes.h"
#include "Engine/World.h"
#include "UObject/Package.h"

namespace Phase6UIA0ReviewRegression
{
	void TickReadStateReady()
	{
		FTSTicker::GetCoreTicker().Tick(0.0f);
	}

	UCardData* CreateCard(
		UObject* Outer,
		const TCHAR* CardId,
		ECardTargetType TargetType = ECardTargetType::Enemy,
		int32 Cost = 0,
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
			UDamageCardEffect* Effect = NewObject<UDamageCardEffect>(Definition);
			Effect->BaseAmount = Damage;
			Effect->DamageKind = EDamageKind::Attack;
			Definition->Effects.Add(Effect);
		}
		return Definition;
	}

	TArray<TObjectPtr<UCardData>> CreateNumberedCards(UObject* Outer, int32 Count)
	{
		TArray<TObjectPtr<UCardData>> Definitions;
		Definitions.Reserve(Count);
		for (int32 Index = 1; Index <= Count; ++Index)
		{
			Definitions.Add(CreateCard(
				Outer,
				*FString::Printf(TEXT("SeedCard%d"), Index),
				ECardTargetType::None,
				0,
				0
			));
		}
		return Definitions;
	}

	UStatusData* CreateStrength(UObject* Outer, int32 FlatPerStack = 1)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = TEXT("ReviewStrength");

		UDamageFlatAddModifier* Modifier = NewObject<UDamageFlatAddModifier>(Definition);
		Modifier->Scope = EModifierScope::Source;
		Modifier->Priority = 0;
		Modifier->ApplicableDamageKind = EDamageKind::Attack;
		Modifier->Value = FlatPerStack;
		Modifier->AmountMode = EModifierAmountMode::ScaleWithAmount;
		Definition->DamageModifiers.Add(Modifier);
		return Definition;
	}

	UStatusData* CreateVulnerable(UObject* Outer, bool bDecayAtTurnEnd)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = bDecayAtTurnEnd ? TEXT("ReviewVulnerableDecay") : TEXT("ReviewVulnerable");

		UDamageRatioModifier* Modifier = NewObject<UDamageRatioModifier>(Definition);
		Modifier->Scope = EModifierScope::Target;
		Modifier->Priority = 0;
		Modifier->ApplicableDamageKind = EDamageKind::Attack;
		Modifier->Phase = EDamageModifierPhase::TargetMultiplier;
		Modifier->Numerator = 3;
		Modifier->Denominator = 2;
		Modifier->AmountMode = EModifierAmountMode::PresenceOnly;
		Definition->DamageModifiers.Add(Modifier);

		if (bDecayAtTurnEnd)
		{
			UTurnEndStatusDecayTrigger* Trigger = NewObject<UTurnEndStatusDecayTrigger>(Definition);
			Trigger->Priority = 0;
			Trigger->AmountToRemove = 1;
			Definition->Triggers.Add(Trigger);
		}
		return Definition;
	}

	bool ApplyStatus(ACombatant* Target, UStatusData* Definition, int32 Amount, uint64 Sequence)
	{
		if (!IsValid(Target) || !IsValid(Definition) || !IsValid(Target->GetStatusContainer()))
		{
			return false;
		}
		bool bCreated = false;
		return IsValid(Target->GetStatusContainer()->ApplyStatus(Definition, Amount, Sequence, bCreated)) && bCreated;
	}

	struct FBattleFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UCardData* CardDefinition = nullptr;

		FBattleFixture(int32 DeckCount = 0, int32 OpeningDraw = 0, int32 TurnDraw = 0, int32 EnemyDamage = 5)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

			Player->MaxHP = 100;
			Enemy->MaxHP = 100;
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = OpeningDraw;
			Battle->PlayerTurnDrawCount = TurnDraw;
			Battle->EnemyTestAttackDamage = EnemyDamage;
			Battle->DeckDebugSeed = 1337;

			CardDefinition = CreateCard(World, TEXT("ReviewStrike"), ECardTargetType::Enemy, 0, 6);
			for (int32 Index = 0; Index < DeckCount; ++Index)
			{
				Battle->DebugStartingDeck.Add(CardDefinition);
			}

			Battle->StartBattle();
			// Drain the initial battle-ready publication before individual tests bind.
			TickReadStateReady();
		}

		~FBattleFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		bool IsReady() const
		{
			return IsValid(World)
				&& IsValid(Player)
				&& IsValid(Enemy)
				&& IsValid(Battle)
				&& IsValid(Battle->GetActionQueueForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}
	};

	bool RequireReady(FAutomationTestBase& Test, const FBattleFixture& Fixture)
	{
		if (Fixture.IsReady()) return true;
		Test.AddError(TEXT("Failed to create the Phase 6UI-A0 review fixture."));
		return false;
	}

	void ExpectImmediateResolutionFaultLogs(FAutomationTestBase& Test)
	{
		Test.AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution fault requested:"), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("[Battle] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FCurrentResolvedDamageUsesPipelineTest,
		"SlayTheSpireDemo.Phase6UIA0.Intent.CurrentResolvedDamageUsesDamagePipeline",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FCurrentResolvedDamageUsesPipelineTest::RunTest(const FString& Parameters)
	{
		FBattleFixture Fixture(0, 0, 0, 5);
		if (!RequireReady(*this, Fixture)) return false;

		TestTrue(TEXT("Enemy Strength applied"), ApplyStatus(Fixture.Enemy, CreateStrength(Fixture.World), 2, 1));
		TestTrue(TEXT("Player Vulnerable applied"), ApplyStatus(Fixture.Player, CreateVulnerable(Fixture.World, false), 1, 2));

		FBattleReadSnapshot Snapshot;
		TestTrue(TEXT("Player-facing snapshot is readable"), Fixture.Battle->TryBuildPlayerFacingReadSnapshot(Snapshot));
		TestEqual(TEXT("Committed plan remains Base Attack 5"), Snapshot.EnemyIntent.BaseAmount, 5);
		TestTrue(TEXT("Current resolved damage is available"), Snapshot.EnemyIntentPlayerFacing.bHasCurrentResolvedDamageAmount);
		TestEqual(TEXT("Current value reuses Strength + Vulnerable pipeline: (5+2)*3/2 = 10"), Snapshot.EnemyIntentPlayerFacing.CurrentResolvedDamageAmount, 10);

		TestTrue(TEXT("End-turn request accepted"), Fixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
		TestEqual(TEXT("With no intervening modifier change, actual EnemyTurn also deals 10"), Fixture.Player->HP, 90);
		TickReadStateReady();
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FExpiringModifierDoesNotClaimGuaranteedFutureDamageTest,
		"SlayTheSpireDemo.Phase6UIA0.Intent.ExpiringTargetModifierDoesNotClaimGuaranteedFutureDamage",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FExpiringModifierDoesNotClaimGuaranteedFutureDamageTest::RunTest(const FString& Parameters)
	{
		FBattleFixture Fixture(0, 0, 0, 5);
		if (!RequireReady(*this, Fixture)) return false;

		TestTrue(TEXT("Expiring Vulnerable applied"), ApplyStatus(Fixture.Player, CreateVulnerable(Fixture.World, true), 1, 1));

		FBattleReadSnapshot Before;
		TestTrue(TEXT("Player-facing snapshot is readable before turn end"), Fixture.Battle->TryBuildPlayerFacingReadSnapshot(Before));
		TestEqual(TEXT("Current state resolves Attack 5 through Vulnerable to 7"), Before.EnemyIntentPlayerFacing.CurrentResolvedDamageAmount, 7);

		TestTrue(TEXT("End-turn request accepted"), Fixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
		TestEqual(TEXT("Player TurnEnded decay removes Vulnerable before EnemyTurn, so actual attack is 5"), Fixture.Player->HP, 95);

		FBattleReadSnapshot After;
		TestTrue(TEXT("Next player-facing snapshot is readable"), Fixture.Battle->TryBuildPlayerFacingReadSnapshot(After));
		TestEqual(TEXT("Current value after decay is now 5"), After.EnemyIntentPlayerFacing.CurrentResolvedDamageAmount, 5);
		TickReadStateReady();
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FInitialShuffleExpectedPermutationTest,
		"SlayTheSpireDemo.Phase6UIA0.Deck.InitialShuffleHasExpectedSeededPermutation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FInitialShuffleExpectedPermutationTest::RunTest(const FString& Parameters)
	{
		UDeckRuntime* Deck = NewObject<UDeckRuntime>(GetTransientPackage());
		const TArray<TObjectPtr<UCardData>> Definitions = CreateNumberedCards(GetTransientPackage(), 5);
		Deck->InitializeFromDefinitions(Definitions, 1337);

		TArray<FName> DrawOrder;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			UCardInstance* Card = nullptr;
			TestTrue(TEXT("Seeded initial card can be drawn"), Deck->TryDrawTopCard(Card));
			if (IsValid(Card)) DrawOrder.Add(Card->GetCardId());
		}

		const TArray<FName> Expected{
			TEXT("SeedCard2"), TEXT("SeedCard3"), TEXT("SeedCard5"), TEXT("SeedCard4"), TEXT("SeedCard1")
		};
		TestEqual(TEXT("Seed 1337 has the explicit expected top-to-bottom draw permutation"), DrawOrder, Expected);
		TestNotEqual(TEXT("Initial draw order is not the unshuffled configured tail order"), DrawOrder[0], FName(TEXT("SeedCard5")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FInitialShufflePreservesIdentitySetTest,
		"SlayTheSpireDemo.Phase6UIA0.Deck.InitialShufflePreservesCardIdentitySet",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FInitialShufflePreservesIdentitySetTest::RunTest(const FString& Parameters)
	{
		UDeckRuntime* Deck = NewObject<UDeckRuntime>(GetTransientPackage());
		const TArray<TObjectPtr<UCardData>> Definitions = CreateNumberedCards(GetTransientPackage(), 5);
		Deck->InitializeFromDefinitions(Definitions, 1337);

		TSet<FName> CardIds;
		TSet<int32> RuntimeIds;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			UCardInstance* Card = nullptr;
			if (Deck->TryDrawTopCard(Card) && IsValid(Card))
			{
				CardIds.Add(Card->GetCardId());
				RuntimeIds.Add(Card->GetRuntimeId());
			}
		}

		TestEqual(TEXT("Initial shuffle preserves all five Card identities"), CardIds.Num(), 5);
		TestEqual(TEXT("Initial shuffle preserves all five Runtime identities"), RuntimeIds.Num(), 5);
		for (int32 Index = 1; Index <= 5; ++Index)
		{
			TestTrue(TEXT("Expected CardId remains present"), CardIds.Contains(FName(*FString::Printf(TEXT("SeedCard%d"), Index))));
			TestTrue(TEXT("Expected RuntimeId remains present"), RuntimeIds.Contains(Index));
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FInitialShuffleDoesNotEmitDeckShuffledTest,
		"SlayTheSpireDemo.Phase6UIA0.Deck.InitialShuffleDoesNotEmitDeckShuffled",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FInitialShuffleDoesNotEmitDeckShuffledTest::RunTest(const FString& Parameters)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
		if (!IsValid(World)) return false;

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatant* Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
		ACombatant* Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
		ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
		if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle))
		{
			World->DestroyWorld(false);
			return false;
		}

		Player->MaxHP = 100;
		Enemy->MaxHP = 100;
		Battle->Player = Player;
		Battle->Enemy = Enemy;
		Battle->OpeningHandDrawCount = 5;
		Battle->PlayerTurnDrawCount = 0;
		Battle->DebugStartingDeck = CreateNumberedCards(World, 5);

		int32 DeckShuffledDispatchCount = 0;
		const FDelegateHandle Handle = UBattleEventDispatcher::OnEventDispatchedForTesting.AddLambda(
			[&DeckShuffledDispatchCount](const FBattleEvent& Event)
			{
				if (Event.TryGet<FDeckShuffledEvent>() != nullptr)
				{
					++DeckShuffledDispatchCount;
				}
			}
		);

		Battle->StartBattle();
		TickReadStateReady();
		UBattleEventDispatcher::OnEventDispatchedForTesting.Remove(Handle);

		TestEqual(TEXT("Real BattleManager/EventDispatcher initialization path emits no DeckShuffled event"), DeckShuffledDispatchCount, 0);
		TestEqual(TEXT("Opening Hand still draws all five initialized cards"), Battle->GetDeckRuntimeForTesting()->GetHandCount(), 5);

		World->DestroyWorld(false);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FCardResolutionPublishesOnceWhenReadableTest,
		"SlayTheSpireDemo.Phase6UIA0.ReadStateReady.CardResolutionPublishesOnceWhenReadable",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FCardResolutionPublishesOnceWhenReadableTest::RunTest(const FString& Parameters)
	{
		FBattleFixture Fixture(1, 1, 0, 5);
		if (!RequireReady(*this, Fixture)) return false;

		int32 ReadyCount = 0;
		bool bRequestReturned = false;
		bool bCallbackObservedRequestReturned = false;
		bool bSnapshotReadableInCallback = false;
		uint64 PublishedBattleId = 0;
		uint64 PublishedRevision = 0;
		Fixture.Battle->OnReadStateReady.AddLambda(
			[&](uint64 BattleId, uint64 Revision)
			{
				++ReadyCount;
				bCallbackObservedRequestReturned = bRequestReturned;
				PublishedBattleId = BattleId;
				PublishedRevision = Revision;
				FBattleReadSnapshot Snapshot;
				bSnapshotReadableInCallback = Fixture.Battle->TryBuildPlayerFacingReadSnapshot(Snapshot)
					&& Snapshot.BattleId == BattleId
					&& Snapshot.StateRevision == Revision;
			}
		);

		UCardInstance* Card = Fixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
		TestNotNull(TEXT("Playable card exists"), Card);
		if (!Card) return false;

		const FGameplayRequestResult Result = Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy);
		bRequestReturned = true;
		TestTrue(TEXT("Formal card request accepted"), Result.IsAcceptedForResolution());
		TestEqual(TEXT("ReadStateReady is never re-entrant before RequestPlayCard returns"), ReadyCount, 0);

		TickReadStateReady();
		TestEqual(TEXT("Exactly one stable read notification is published for the completed card resolution"), ReadyCount, 1);
		TestTrue(TEXT("Callback observes that the public Request already returned"), bCallbackObservedRequestReturned);
		TestTrue(TEXT("Published revision is readable inside the callback"), bSnapshotReadableInCallback);
		TestTrue(TEXT("Published BattleId is non-zero"), PublishedBattleId != 0);
		TestTrue(TEXT("Published revision is non-zero"), PublishedRevision != 0);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FAsyncActionDoesNotPublishBeforeFinishTest,
		"SlayTheSpireDemo.Phase6UIA0.ReadStateReady.AsyncActionDoesNotPublishBeforeFinish",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FAsyncActionDoesNotPublishBeforeFinishTest::RunTest(const FString& Parameters)
	{
		FBattleFixture Fixture(0, 0, 0, 5);
		if (!RequireReady(*this, Fixture)) return false;

		int32 ReadyCount = 0;
		Fixture.Battle->OnReadStateReady.AddLambda([&](uint64, uint64) { ++ReadyCount; });

		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		UPhase6UIA0ManualFinishAction* Action = NewObject<UPhase6UIA0ManualFinishAction>(Queue);
		TestTrue(TEXT("Manual async action enqueued"), Queue->AddToBack(Action));
		TestTrue(TEXT("Manual async resolution started"), Queue->StartProcessing());
		TestTrue(TEXT("Manual action reached Execute"), Action->HasExecuted());
		TestEqual(TEXT("No ReadStateReady while CurrentAction still owns the Queue"), ReadyCount, 0);

		FBattleReadSnapshot BusySnapshot;
		TestFalse(TEXT("Player-facing snapshot is not readable while async Action is unresolved"), Fixture.Battle->TryBuildPlayerFacingReadSnapshot(BusySnapshot));

		Action->CompleteManually();
		TestEqual(TEXT("Completing the async Action schedules but does not synchronously publish ReadStateReady"), ReadyCount, 0);
		TickReadStateReady();
		TestEqual(TEXT("One ReadStateReady publishes after async Action finishes and deferred stable boundary runs"), ReadyCount, 1);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FFullTurnPublishesOnlyAfterMacroFlowStabilizesTest,
		"SlayTheSpireDemo.Phase6UIA0.ReadStateReady.FullTurnPublishesOnlyAfterMacroFlowStabilizes",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FFullTurnPublishesOnlyAfterMacroFlowStabilizesTest::RunTest(const FString& Parameters)
	{
		FBattleFixture Fixture(0, 0, 0, 5);
		if (!RequireReady(*this, Fixture)) return false;

		TArray<EBattleState> PublishedStates;
		Fixture.Battle->OnReadStateReady.AddLambda(
			[&](uint64, uint64)
			{
				FBattleReadSnapshot Snapshot;
				if (Fixture.Battle->TryBuildPlayerFacingReadSnapshot(Snapshot))
				{
					PublishedStates.Add(Snapshot.BattleState);
				}
			}
		);

		TestTrue(TEXT("End-turn request accepted"), Fixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
		TestEqual(TEXT("No intermediate QueueEmpty boundary publishes before Request returns"), PublishedStates.Num(), 0);
		TickReadStateReady();
		TestEqual(TEXT("Only one macro-stable state publishes after deferred completion"), PublishedStates.Num(), 1);
		if (PublishedStates.Num() == 1)
		{
			TestEqual(TEXT("Only final macro-stable PlayerTurn is published"), PublishedStates[0], EBattleState::PlayerTurn);
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FResolutionFaultPublishesReadableSnapshotTest,
		"SlayTheSpireDemo.Phase6UIA0.ReadStateReady.ResolutionFaultPublishesReadableSnapshot",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FResolutionFaultPublishesReadableSnapshotTest::RunTest(const FString& Parameters)
	{
		FBattleFixture Fixture(0, 0, 0, 5);
		if (!RequireReady(*this, Fixture)) return false;
		ExpectImmediateResolutionFaultLogs(*this);

		int32 ReadyCount = 0;
		bool bFaultSnapshotReadable = false;
		Fixture.Battle->OnReadStateReady.AddLambda(
			[&](uint64 BattleId, uint64 Revision)
			{
				++ReadyCount;
				FBattleReadSnapshot Snapshot;
				bFaultSnapshotReadable = Fixture.Battle->TryBuildPlayerFacingReadSnapshot(Snapshot)
					&& Snapshot.BattleId == BattleId
					&& Snapshot.StateRevision == Revision
					&& Snapshot.BattleState == EBattleState::ResolutionFaulted;
			}
		);

		TestTrue(TEXT("Resolution fault request accepted"), Fixture.Battle->GetActionQueueForTesting()->RequestResolutionFault(TEXT("UI-A0 readable fault test.")));
		TestEqual(TEXT("Fault state commits immediately but public stable-read notification is deferred"), ReadyCount, 0);
		TickReadStateReady();
		TestEqual(TEXT("Fault path publishes exactly one stable read notification"), ReadyCount, 1);
		TestTrue(TEXT("Published fault snapshot is readable and committed"), bFaultSnapshotReadable);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FNewBattleNotSuppressedByRepeatedRevisionTest,
		"SlayTheSpireDemo.Phase6UIA0.ReadStateReady.NewBattleIsNotSuppressedByRepeatedRevision",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
	)

	bool FNewBattleNotSuppressedByRepeatedRevisionTest::RunTest(const FString& Parameters)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
		if (!IsValid(World)) return false;

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatant* Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
		ACombatant* Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
		ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
		Player->MaxHP = 100;
		Enemy->MaxHP = 100;
		Battle->Player = Player;
		Battle->Enemy = Enemy;
		Battle->OpeningHandDrawCount = 1;
		Battle->PlayerTurnDrawCount = 0;
		Battle->DebugStartingDeck.Add(CreateCard(World, TEXT("RepeatedRevisionCard"), ECardTargetType::None, 0, 0));

		TArray<TPair<uint64, uint64>> PublishedKeys;
		Battle->OnReadStateReady.AddLambda(
			[&](uint64 BattleId, uint64 Revision)
			{
				PublishedKeys.Emplace(BattleId, Revision);
			}
		);

		Battle->StartBattle();
		TestEqual(TEXT("First battle does not publish re-entrantly from StartBattle"), PublishedKeys.Num(), 0);
		TickReadStateReady();
		Battle->StartBattle();
		TestEqual(TEXT("Second battle also waits for its stable publication boundary"), PublishedKeys.Num(), 1);
		TickReadStateReady();

		TestEqual(TEXT("Both battle starts publish stable read state"), PublishedKeys.Num(), 2);
		if (PublishedKeys.Num() == 2)
		{
			TestTrue(TEXT("Second battle has a new BattleId"), PublishedKeys[1].Key > PublishedKeys[0].Key);
			TestEqual(TEXT("Equivalent setup may legitimately repeat the same StateRevision"), PublishedKeys[1].Value, PublishedKeys[0].Value);
		}

		World->DestroyWorld(false);
		return true;
	}
}

#endif
