#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleAction.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/SelectionRequestAction.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Selection/ExhaustSelectedContinuation.h"
#include "Selection/SelectionResolver.h"
#include "Selection/SelectionTypes.h"
#include "Engine/World.h"

namespace CardExpansionWave1CC0SelectExhaustTest
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
			Card->Description = FText::FromString(TEXT("C0 test card."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* CreatePlayerSelectExhaustCard(const TCHAR* CardId, int32 SelectionCount)
		{
			UCardData* Card = CreatePlainCard(CardId);
			USelectExhaustHandCardEffect* Effect = NewObject<USelectExhaustHandCardEffect>(Card);
			Effect->BaseSelectionMode = ESelectExhaustSelectionMode::Player;
			Effect->BaseSelectionCount = SelectionCount;
			Effect->UpgradedSelectionMode = ESelectExhaustSelectionMode::Player;
			Effect->UpgradedSelectionCount = SelectionCount;
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
	};
}

using namespace CardExpansionWave1CC0SelectExhaustTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0SelectExhaustAuthoredConfigTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.SelectExhaust.AuthoredConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0SelectExhaustAuthoredConfigTest::RunTest(const FString& Parameters)
{
	USelectExhaustHandCardEffect* Effect = NewObject<USelectExhaustHandCardEffect>();
	if (!TestNotNull(TEXT("Effect exists"), Effect))
	{
		return false;
	}

	TestTrue(TEXT("Default Base mode preserves Burning Pact Player behavior"), Effect->BaseSelectionMode == ESelectExhaustSelectionMode::Player);
	TestEqual(TEXT("Default Base count preserves Burning Pact single-select behavior"), Effect->BaseSelectionCount, 1);
	TestTrue(TEXT("Default Upgraded mode preserves Burning Pact Player behavior"), Effect->UpgradedSelectionMode == ESelectExhaustSelectionMode::Player);
	TestEqual(TEXT("Default Upgraded count preserves Burning Pact single-select behavior"), Effect->UpgradedSelectionCount, 1);

	Effect->BaseSelectionMode = ESelectExhaustSelectionMode::Random;
	Effect->BaseSelectionCount = 2;
	Effect->UpgradedSelectionMode = ESelectExhaustSelectionMode::Player;
	Effect->UpgradedSelectionCount = 3;
	TestTrue(TEXT("Base effective mode is explicit Random"), Effect->GetEffectiveSelectionMode(false) == ESelectExhaustSelectionMode::Random);
	TestEqual(TEXT("Base effective count is explicit 2"), Effect->GetEffectiveSelectionCount(false), 2);
	TestTrue(TEXT("Upgraded effective mode is explicit Player"), Effect->GetEffectiveSelectionMode(true) == ESelectExhaustSelectionMode::Player);
	TestEqual(TEXT("Upgraded effective count is explicit 3"), Effect->GetEffectiveSelectionCount(true), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0SelectionRejectsDuplicateObjectsTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.DuplicateObjectsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0SelectionRejectsDuplicateObjectsTest::RunTest(const FString& Parameters)
{
	USelectionResolver* Resolver = NewObject<USelectionResolver>();
	USelectionRequestAction* PendingAction = NewObject<USelectionRequestAction>();
	UExhaustSelectedContinuation* Continuation = NewObject<UExhaustSelectedContinuation>();
	UCardInstance* CandidateA = NewObject<UCardInstance>();
	UCardInstance* CandidateB = NewObject<UCardInstance>();
	if (!TestNotNull(TEXT("Resolver exists"), Resolver)
		|| !TestNotNull(TEXT("Pending Action exists"), PendingAction)
		|| !TestNotNull(TEXT("Continuation exists"), Continuation)
		|| !TestNotNull(TEXT("Candidate A exists"), CandidateA)
		|| !TestNotNull(TEXT("Candidate B exists"), CandidateB))
	{
		return false;
	}

	FSelectionRequest Request;
	Request.SelectionSource = TEXT("Wave1CC0DuplicateGuard");
	Request.MinCount = 2;
	Request.MaxCount = 2;
	FSelectionCandidate EntryA;
	EntryA.RuntimeObject = CandidateA;
	EntryA.RuntimeSequence = 1;
	Request.Candidates.Add(EntryA);
	FSelectionCandidate EntryB;
	EntryB.RuntimeObject = CandidateB;
	EntryB.RuntimeSequence = 2;
	Request.Candidates.Add(EntryB);

	TestTrue(TEXT("Two-candidate request begins"), Resolver->BeginSelection(Request, Continuation, PendingAction));
	FSelectionResult DuplicateResult;
	DuplicateResult.Status = ESelectionStatus::Resolved;
	DuplicateResult.SelectedObjects.Add(CandidateA);
	DuplicateResult.SelectedObjects.Add(CandidateA);
	TArray<UBattleAction*> OutActions;
	TestFalse(TEXT("Duplicate selected object is rejected by Gameplay resolver"), Resolver->TryResolveSelection(DuplicateResult, OutActions));
	TestEqual(TEXT("Duplicate rejection builds no continuation Actions"), OutActions.Num(), 0);
	TestFalse(TEXT("Invalid duplicate result clears the pending request"), Resolver->HasPendingSelection());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0PlayerMultiExhaustTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.SelectExhaust.PlayerMultiExhaust",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0PlayerMultiExhaustTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* SelectExhaust = Fixture.CreatePlayerSelectExhaustCard(TEXT("C0Multi"), 2);
	UCardData* OtherA = Fixture.CreatePlainCard(TEXT("C0MultiA"));
	UCardData* OtherB = Fixture.CreatePlainCard(TEXT("C0MultiB"));
	UCardData* OtherC = Fixture.CreatePlainCard(TEXT("C0MultiC"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ SelectExhaust, OtherA, OtherB, OtherC })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("C0Multi"));
	UCardInstance* CardA = Fixture.FindInstanceByCardId(Deck, TEXT("C0MultiA"));
	UCardInstance* CardB = Fixture.FindInstanceByCardId(Deck, TEXT("C0MultiB"));
	UCardInstance* CardC = Fixture.FindInstanceByCardId(Deck, TEXT("C0MultiC"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard)
		|| !TestNotNull(TEXT("A exists"), CardA)
		|| !TestNotNull(TEXT("B exists"), CardB)
		|| !TestNotNull(TEXT("C exists"), CardC))
	{
		return false;
	}

	TestTrue(TEXT("Play accepted"), Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution());
	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	const FSelectionRequest* Request = IsValid(Resolver) ? Resolver->GetPendingRequest() : nullptr;
	if (!TestNotNull(TEXT("Two-card selection request is pending"), Request))
	{
		return false;
	}
	TestEqual(TEXT("Three valid Hand candidates remain"), Request->Candidates.Num(), 3);
	TestEqual(TEXT("Player request MinCount is exactly 2"), Request->MinCount, 2);
	TestEqual(TEXT("Player request MaxCount is exactly 2"), Request->MaxCount, 2);
	TestTrue(TEXT("Player multi-select remains mandatory"), Request->CancelPolicy == ESelectionCancelPolicy::Forbidden);

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(CardB);
	Result.SelectedObjects.Add(CardA);
	TestTrue(TEXT("Two distinct selected cards submit"), Resolver->SubmitResult(Result));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	TestEqual(TEXT("Exactly two cards enter ExhaustPile"), Deck->GetExhaustCount(), 2);
	TestFalse(TEXT("A is removed from Hand"), Deck->IsCardInHand(CardA));
	TestFalse(TEXT("B is removed from Hand"), Deck->IsCardInHand(CardB));
	TestTrue(TEXT("Unselected C remains in Hand"), Deck->IsCardInHand(CardC));
	TestFalse(TEXT("Multi-exhaust resolution has no fault"), Queue->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0PlayerCountClampsToCandidatesTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.SelectExhaust.CountClampsToCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0PlayerCountClampsToCandidatesTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* SelectExhaust = Fixture.CreatePlayerSelectExhaustCard(TEXT("C0Clamp"), 5);
	UCardData* OtherA = Fixture.CreatePlainCard(TEXT("C0ClampA"));
	UCardData* OtherB = Fixture.CreatePlainCard(TEXT("C0ClampB"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ SelectExhaust, OtherA, OtherB })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("C0Clamp"));
	UCardInstance* CardA = Fixture.FindInstanceByCardId(Deck, TEXT("C0ClampA"));
	UCardInstance* CardB = Fixture.FindInstanceByCardId(Deck, TEXT("C0ClampB"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard)
		|| !TestNotNull(TEXT("A exists"), CardA)
		|| !TestNotNull(TEXT("B exists"), CardB))
	{
		return false;
	}

	TestTrue(TEXT("Play accepted"), Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution());
	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	const FSelectionRequest* Request = IsValid(Resolver) ? Resolver->GetPendingRequest() : nullptr;
	if (!TestNotNull(TEXT("Clamped selection request is pending"), Request))
	{
		return false;
	}
	TestEqual(TEXT("Only two valid candidates exist"), Request->Candidates.Num(), 2);
	TestEqual(TEXT("MinCount clamps from authored 5 to 2"), Request->MinCount, 2);
	TestEqual(TEXT("MaxCount clamps from authored 5 to 2"), Request->MaxCount, 2);

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(CardA);
	Result.SelectedObjects.Add(CardB);
	TestTrue(TEXT("All candidates submit"), Resolver->SubmitResult(Result));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	TestEqual(TEXT("All candidates are exhausted"), Deck->GetExhaustCount(), 2);
	TestFalse(TEXT("No resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
