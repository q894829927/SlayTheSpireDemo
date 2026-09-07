#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Selection/SelectionResolver.h"
#include "Engine/World.h"

namespace CardExpansionWave1CC0RandomSelectionTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

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
		}

		~FFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		UCardData* CreatePlainCard(const TCHAR* CardId)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(CardId);
			Card->DisplayName = FText::FromString(CardId);
			Card->Description = FText::FromString(TEXT("C0 random test card."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* CreateRandomSelectExhaustCard(const TCHAR* CardId, int32 SelectionCount)
		{
			UCardData* Card = CreatePlainCard(CardId);
			USelectExhaustHandCardEffect* Effect = NewObject<USelectExhaustHandCardEffect>(Card);
			Effect->BaseSelectionMode = ESelectExhaustSelectionMode::Random;
			Effect->BaseSelectionCount = SelectionCount;
			Effect->UpgradedSelectionMode = ESelectExhaustSelectionMode::Random;
			Effect->UpgradedSelectionCount = SelectionCount;
			Card->Effects.Add(Effect);
			return Card;
		}

		bool Start(const TArray<UCardData*>& Definitions, int32 Seed)
		{
			if (!IsValid(Battle) || Definitions.Num() == 0)
			{
				return false;
			}

			Battle->DebugStartingDeck.Reset();
			for (UCardData* Definition : Definitions)
			{
				Battle->DebugStartingDeck.Add(Definition);
			}
			Battle->DeckDebugSeed = Seed;
			Battle->OpeningHandDrawCount = Definitions.Num();
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			return IsValid(Battle->GetActionQueueForTesting())
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn
				&& Battle->GetDeckRuntimeForTesting()->GetHandCount() == Definitions.Num();
		}

		UCardInstance* FindInstanceByCardId(UDeckRuntime* Deck, FName CardId) const
		{
			if (!IsValid(Deck))
			{
				return nullptr;
			}
			for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
			{
				if (IsValid(Card.Get()) && Card->GetCardId() == CardId)
				{
					return Card.Get();
				}
			}
			return nullptr;
		}

		TArray<FName> GetExhaustCardIds() const
		{
			TArray<FName> Result;
			UDeckRuntime* Deck = IsValid(Battle) ? Battle->GetDeckRuntimeForTesting() : nullptr;
			if (!IsValid(Deck))
			{
				return Result;
			}
			for (const TObjectPtr<UCardInstance>& Card : Deck->GetExhaustCards())
			{
				if (IsValid(Card.Get()))
				{
					Result.Add(Card->GetCardId());
				}
			}
			return Result;
		}
	};
}

using namespace CardExpansionWave1CC0RandomSelectionTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0BattleRngChooseIndexDeterministicTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.ChooseIndexDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0BattleRngChooseIndexDeterministicTest::RunTest(const FString& Parameters)
{
	UDeckRuntime* First = NewObject<UDeckRuntime>();
	UDeckRuntime* Second = NewObject<UDeckRuntime>();
	if (!TestNotNull(TEXT("First DeckRuntime exists"), First)
		|| !TestNotNull(TEXT("Second DeckRuntime exists"), Second))
	{
		return false;
	}

	TArray<TObjectPtr<UCardData>> EmptyDefinitions;
	First->InitializeFromDefinitions(EmptyDefinitions, 424242);
	Second->InitializeFromDefinitions(EmptyDefinitions, 424242);

	int32 InvalidIndex = 123;
	TestFalse(TEXT("Count zero is rejected"), First->TryChooseRandomIndex(0, InvalidIndex));
	TestEqual(TEXT("Rejected Count zero resets output"), InvalidIndex, INDEX_NONE);

	int32 SingleIndex = INDEX_NONE;
	TestTrue(TEXT("Count one succeeds"), First->TryChooseRandomIndex(1, SingleIndex));
	TestEqual(TEXT("Count one deterministically chooses index zero"), SingleIndex, 0);

	// Count 0 and Count 1 above must not consume RNG. The first random sequence
	// must still match a freshly initialized stream with the same seed.
	for (int32 Pick = 0; Pick < 12; ++Pick)
	{
		int32 FirstIndex = INDEX_NONE;
		int32 SecondIndex = INDEX_NONE;
		if (!TestTrue(TEXT("First deterministic choice succeeds"), First->TryChooseRandomIndex(7, FirstIndex))
			|| !TestTrue(TEXT("Second deterministic choice succeeds"), Second->TryChooseRandomIndex(7, SecondIndex)))
		{
			return false;
		}
		TestEqual(TEXT("Same seed and same call sequence produce same index"), FirstIndex, SecondIndex);
		TestTrue(TEXT("Chosen index remains in range"), FirstIndex >= 0 && FirstIndex < 7);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0RandomMultiSelectExhaustTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.MultiSelectExhaust",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0RandomMultiSelectExhaustTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* RandomCard = Fixture.CreateRandomSelectExhaustCard(TEXT("C0RandomMulti"), 2);
	UCardData* CardAData = Fixture.CreatePlainCard(TEXT("C0RandomA"));
	UCardData* CardBData = Fixture.CreatePlainCard(TEXT("C0RandomB"));
	UCardData* CardCData = Fixture.CreatePlainCard(TEXT("C0RandomC"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ RandomCard, CardAData, CardBData, CardCData }, 1337)))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("C0RandomMulti"));
	if (!TestNotNull(TEXT("Random SelectExhaust card exists"), PlayedCard))
	{
		return false;
	}

	TArray<UCardInstance*> CandidateOrder;
	for (const TObjectPtr<UCardInstance>& HandCard : Deck->GetHandCards())
	{
		if (IsValid(HandCard.Get()) && HandCard.Get() != PlayedCard)
		{
			CandidateOrder.Add(HandCard.Get());
		}
	}
	TestEqual(TEXT("Exactly three candidates exist before play"), CandidateOrder.Num(), 3);

	TestTrue(
		TEXT("Random SelectExhaust play accepted"),
		Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution()
	);
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	TestFalse(TEXT("Random mode never opens pending player selection"), IsValid(Resolver) && Resolver->HasPendingSelection());
	TestFalse(TEXT("Random resolution has no fault"), Queue->IsResolutionFaulted());
	TestEqual(TEXT("Exactly two unique candidates are exhausted"), Deck->GetExhaustCount(), 2);

	TArray<UCardInstance*> ExpectedCanonicalOrder;
	for (UCardInstance* Candidate : CandidateOrder)
	{
		const bool bWasExhausted = Deck->GetExhaustCards().ContainsByPredicate(
			[Candidate](const TObjectPtr<UCardInstance>& Exhausted)
			{
				return Exhausted.Get() == Candidate;
			}
		);
		if (bWasExhausted)
		{
			ExpectedCanonicalOrder.Add(Candidate);
		}
	}
	TestEqual(TEXT("Exactly two original candidates were selected"), ExpectedCanonicalOrder.Num(), 2);
	for (int32 Index = 0; Index < ExpectedCanonicalOrder.Num() && Index < Deck->GetExhaustCards().Num(); ++Index)
	{
		TestTrue(
			TEXT("Exhaust execution order is canonical candidate order"),
			Deck->GetExhaustCards()[Index].Get() == ExpectedCanonicalOrder[Index]
		);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0RandomSelectionReproducibleTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.SameSeedReproducible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0RandomSelectionReproducibleTest::RunTest(const FString& Parameters)
{
	FFixture First;
	FFixture Second;

	UCardData* FirstRandom = First.CreateRandomSelectExhaustCard(TEXT("C0RandomRepeat"), 2);
	UCardData* FirstA = First.CreatePlainCard(TEXT("C0RepeatA"));
	UCardData* FirstB = First.CreatePlainCard(TEXT("C0RepeatB"));
	UCardData* FirstC = First.CreatePlainCard(TEXT("C0RepeatC"));
	UCardData* SecondRandom = Second.CreateRandomSelectExhaustCard(TEXT("C0RandomRepeat"), 2);
	UCardData* SecondA = Second.CreatePlainCard(TEXT("C0RepeatA"));
	UCardData* SecondB = Second.CreatePlainCard(TEXT("C0RepeatB"));
	UCardData* SecondC = Second.CreatePlainCard(TEXT("C0RepeatC"));

	if (!TestTrue(TEXT("First fixture starts"), First.Start({ FirstRandom, FirstA, FirstB, FirstC }, 9001))
		|| !TestTrue(TEXT("Second fixture starts"), Second.Start({ SecondRandom, SecondA, SecondB, SecondC }, 9001)))
	{
		return false;
	}

	UDeckRuntime* FirstDeck = First.Battle->GetDeckRuntimeForTesting();
	UDeckRuntime* SecondDeck = Second.Battle->GetDeckRuntimeForTesting();
	UCardInstance* FirstPlayed = First.FindInstanceByCardId(FirstDeck, TEXT("C0RandomRepeat"));
	UCardInstance* SecondPlayed = Second.FindInstanceByCardId(SecondDeck, TEXT("C0RandomRepeat"));
	if (!TestNotNull(TEXT("First random card exists"), FirstPlayed)
		|| !TestNotNull(TEXT("Second random card exists"), SecondPlayed))
	{
		return false;
	}

	TestTrue(TEXT("First play accepted"), First.Battle->RequestPlayCard(FirstPlayed, nullptr).IsAcceptedForResolution());
	TestTrue(TEXT("Second play accepted"), Second.Battle->RequestPlayCard(SecondPlayed, nullptr).IsAcceptedForResolution());
	First.Battle->FlushScheduledReadStateReadyForTesting();
	Second.Battle->FlushScheduledReadStateReadyForTesting();

	const TArray<FName> FirstExhaustIds = First.GetExhaustCardIds();
	const TArray<FName> SecondExhaustIds = Second.GetExhaustCardIds();
	TestEqual(TEXT("Both same-seed runs exhaust exactly two cards"), FirstExhaustIds.Num(), 2);
	TestEqual(TEXT("Second same-seed run exhausts exactly two cards"), SecondExhaustIds.Num(), 2);
	TestTrue(TEXT("Same seed and ordered candidates choose the same canonical set"), FirstExhaustIds == SecondExhaustIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0RandomNoCandidateSkipsTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.NoCandidateSkips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0RandomNoCandidateSkipsTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* RandomCard = Fixture.CreateRandomSelectExhaustCard(TEXT("C0RandomOnly"), 3);
	if (!TestTrue(TEXT("Single-card fixture starts"), Fixture.Start({ RandomCard }, 1337)))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("C0RandomOnly"));
	if (!TestNotNull(TEXT("Random card exists"), PlayedCard))
	{
		return false;
	}

	TestTrue(TEXT("No-candidate random play accepted"), Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	TestFalse(TEXT("No pending player selection is created"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestEqual(TEXT("No card is exhausted without candidates"), Deck->GetExhaustCount(), 0);
	TestFalse(TEXT("No-candidate path does not fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
