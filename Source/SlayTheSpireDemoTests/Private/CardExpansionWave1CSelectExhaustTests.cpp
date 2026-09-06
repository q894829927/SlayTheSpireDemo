#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Events/BattleEventDispatcher.h"
#include "Selection/SelectionResolver.h"
#include "Selection/SelectionTypes.h"
#include "Engine/World.h"

namespace CardExpansionWave1CSelectExhaustTest
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
			Battle->bEnableCommittedPresentationRecording = true;
		}

		~FFixture()
		{
			UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
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
			Card->Description = FText::FromString(TEXT("Plain hand card."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* CreateSelectExhaustCard(const TCHAR* CardId)
		{
			UCardData* Card = CreatePlainCard(CardId);
			USelectExhaustHandCardEffect* Effect = NewObject<USelectExhaustHandCardEffect>(Card);
			Card->Effects.Add(Effect);
			return Card;
		}

		bool Start(const TArray<UCardData*>& Definitions)
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
			Battle->OpeningHandDrawCount = Definitions.Num();
			Battle->StartBattle();
			Flush();
			return IsValid(Battle->GetActionQueueForTesting())
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn
				&& Battle->GetDeckRuntimeForTesting()->GetHandCount() == Definitions.Num();
		}

		void Flush() const
		{
			if (IsValid(Battle))
			{
				Battle->FlushScheduledReadStateReadyForTesting();
			}
		}

		const UCardInstance* FindInstanceByCardId(const UDeckRuntime* Deck, FName CardId) const
		{
			for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
			{
				if (Card->GetCardId() == CardId)
				{
					return Card.Get();
				}
			}
			return nullptr;
		}
	};
}

using namespace CardExpansionWave1CSelectExhaustTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectExhaustResolveExhaustsChosenTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.SelectExhaust.ExhaustsChosenCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectExhaustResolveExhaustsChosenTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* SelectExhaust = Fixture.CreateSelectExhaustCard(TEXT("Wave1CBurn"));
	UCardData* OtherA = Fixture.CreatePlainCard(TEXT("Wave1COtherA"));
	UCardData* OtherB = Fixture.CreatePlainCard(TEXT("Wave1COtherB"));
	if (!TestTrue(TEXT("Fixture starts with three Hand cards"), Fixture.Start({ SelectExhaust, OtherA, OtherB })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	const UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CBurn"));
	const UCardInstance* ChosenCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1COtherA"));
	const UCardInstance* UntouchedCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1COtherB"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard)
		|| !TestNotNull(TEXT("Chosen card exists"), ChosenCard)
		|| !TestNotNull(TEXT("Untouched card exists"), UntouchedCard))
	{
		return false;
	}

	TestTrue(
		TEXT("Select-exhaust play accepted"),
		Fixture.Battle->RequestPlayCard(const_cast<UCardInstance*>(PlayedCard), nullptr).IsAcceptedForResolution()
	);

	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	if (!TestNotNull(TEXT("Resolver exists"), Resolver))
	{
		return false;
	}
	TestTrue(TEXT("Selection is pending after play"), Resolver->HasPendingSelection());

	const FSelectionRequest* Request = Resolver->GetPendingRequest();
	if (!TestNotNull(TEXT("Pending request exposed"), Request))
	{
		return false;
	}
	TestEqual(TEXT("Exactly two candidates (not the played card)"), Request->Candidates.Num(), 2);

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(const_cast<UCardInstance*>(ChosenCard));
	TestTrue(TEXT("Selection submits successfully"), Resolver->SubmitResult(Result));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	TestEqual(TEXT("Exactly one card enters ExhaustPile"), Deck->GetExhaustCount(), 1);
	TestFalse(TEXT("Chosen card is no longer in Hand"), Deck->IsCardInHand(const_cast<UCardInstance*>(ChosenCard)));
	TestTrue(TEXT("Untouched card remains in Hand"), Deck->IsCardInHand(const_cast<UCardInstance*>(UntouchedCard)));
	TestFalse(TEXT("No resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectExhaustSkipsWhenNoCandidateTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.SelectExhaust.SkipsWhenNoCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectExhaustSkipsWhenNoCandidateTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* SelectExhaust = Fixture.CreateSelectExhaustCard(TEXT("Wave1CBurnOnly"));
	if (!TestTrue(TEXT("Fixture starts with a single Hand card"), Fixture.Start({ SelectExhaust })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	const UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CBurnOnly"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard))
	{
		return false;
	}

	TestTrue(
		TEXT("Select-exhaust play accepted"),
		Fixture.Battle->RequestPlayCard(const_cast<UCardInstance*>(PlayedCard), nullptr).IsAcceptedForResolution()
	);

	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	TestNotNull(TEXT("Resolver exists"), Resolver);
	TestFalse(TEXT("No selection pending when no other card exists"), Resolver->HasPendingSelection());
	TestEqual(TEXT("Nothing is exhausted"), Deck->GetExhaustCount(), 0);
	TestFalse(TEXT("No resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

#endif
