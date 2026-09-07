#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/SelectionRequestAction.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleSelectionRequest.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/GainBlockCardEffect.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Selection/ExhaustSelectedContinuation.h"
#include "Selection/SelectionResolver.h"
#include "Engine/World.h"

namespace CardExpansionWave1CC0FinalCoverageTest
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
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

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
			if (IsValid(World)) World->DestroyWorld(false);
		}

		UCardData* CreatePlainCard(const TCHAR* CardId)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(CardId);
			Card->DisplayName = FText::FromString(CardId);
			Card->Description = FText::FromString(TEXT("C0 final coverage card."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* CreateSelectExhaustCard(const TCHAR* CardId, ESelectExhaustSelectionMode Mode, int32 Count)
		{
			UCardData* Card = CreatePlainCard(CardId);
			USelectExhaustHandCardEffect* Effect = NewObject<USelectExhaustHandCardEffect>(Card);
			Effect->BaseSelectionMode = Mode;
			Effect->BaseSelectionCount = Count;
			Effect->UpgradedSelectionMode = Mode;
			Effect->UpgradedSelectionCount = Count;
			Card->Effects.Add(Effect);
			return Card;
		}

		bool Start(const TArray<UCardData*>& Definitions, int32 Seed = 1337)
		{
			if (!IsValid(Battle) || Definitions.Num() == 0) return false;
			Battle->DebugStartingDeck.Reset();
			for (UCardData* Definition : Definitions) Battle->DebugStartingDeck.Add(Definition);
			Battle->DeckDebugSeed = Seed;
			Battle->OpeningHandDrawCount = Definitions.Num();
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			return IsValid(Battle->GetActionQueueForTesting())
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn
				&& Battle->GetDeckRuntimeForTesting()->GetHandCount() == Definitions.Num();
		}

		UCardInstance* FindHandCard(FName CardId) const
		{
			UDeckRuntime* Deck = IsValid(Battle) ? Battle->GetDeckRuntimeForTesting() : nullptr;
			if (!IsValid(Deck)) return nullptr;
			for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
			{
				if (IsValid(Card.Get()) && Card->GetCardId() == CardId) return Card.Get();
			}
			return nullptr;
		}
	};
}

using namespace CardExpansionWave1CC0FinalCoverageTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0CountZeroNoOpContinuesTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.SelectExhaust.CountZeroNoOpContinues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC0CountZeroNoOpContinuesTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* Card = Fixture.CreateSelectExhaustCard(TEXT("C0Zero"), ESelectExhaustSelectionMode::Random, 0);
	Card->TargetType = ECardTargetType::Self;
	Card->Description = FText::FromString(TEXT("Gain {Block} Block."));
	UGainBlockCardEffect* Block = NewObject<UGainBlockCardEffect>(Card);
	Block->BaseAmount = 4;
	Block->UpgradedAmount = 4;
	Card->Effects.Add(Block);
	UCardData* Other = Fixture.CreatePlainCard(TEXT("C0ZeroOther"));

	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ Card, Other }, 2468))) return false;
	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* Played = Fixture.FindHandCard(TEXT("C0Zero"));
	if (!TestNotNull(TEXT("Count-zero card exists"), Played)) return false;

	TArray<TObjectPtr<UCardData>> ControlDefinitions;
	ControlDefinitions.Add(Card);
	ControlDefinitions.Add(Other);
	UDeckRuntime* Control = NewObject<UDeckRuntime>(Fixture.World);
	Control->InitializeFromDefinitions(ControlDefinitions, 2468);

	TestTrue(TEXT("Count-zero card play accepted"), Fixture.Battle->RequestPlayCard(Played, Fixture.Player).IsAcceptedForResolution());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	TestFalse(TEXT("Count zero creates no pending selection"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestEqual(TEXT("Count zero exhausts nothing"), Deck->GetExhaustCount(), 0);
	TestEqual(TEXT("Later authored Block effect still resolves"), Fixture.Player->Block, 4);
	TestTrue(TEXT("Played card finishes to Discard"), Deck->GetDiscardCards().Contains(Played));
	TestFalse(TEXT("Count-zero resolution has no fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());

	int32 BattleNext = INDEX_NONE;
	int32 ControlNext = INDEX_NONE;
	TestTrue(TEXT("Battle RNG remains usable"), Deck->TryChooseRandomIndex(7, BattleNext));
	TestTrue(TEXT("Control RNG remains usable"), Control->TryChooseRandomIndex(7, ControlNext));
	TestEqual(TEXT("Count-zero Random effect does not consume battle RNG"), BattleNext, ControlNext);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0FacadeWrongCountRejectedTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.WrongCountRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC0FacadeWrongCountRejectedTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* EffectCard = Fixture.CreateSelectExhaustCard(TEXT("C0WrongCount"), ESelectExhaustSelectionMode::Player, 2);
	UCardData* A = Fixture.CreatePlainCard(TEXT("C0WrongCountA"));
	UCardData* B = Fixture.CreatePlainCard(TEXT("C0WrongCountB"));
	UCardData* C = Fixture.CreatePlainCard(TEXT("C0WrongCountC"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ EffectCard, A, B, C }))) return false;

	UCardInstance* Played = Fixture.FindHandCard(TEXT("C0WrongCount"));
	if (!TestNotNull(TEXT("Effect card exists"), Played)) return false;
	TestTrue(TEXT("Play accepted"), Fixture.Battle->RequestPlayCard(Played, nullptr).IsAcceptedForResolution());

	FPendingCardSelectionReadView View;
	if (!TestTrue(TEXT("Read view available"), BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Fixture.Battle, View))) return false;
	TestEqual(TEXT("RequiredCount is exactly two"), View.RequiredCount, 2);
	TestFalse(TEXT("One RuntimeId is rejected"), BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, { View.CandidateRuntimeIds[0] }));
	TestTrue(TEXT("Wrong-count rejection keeps request pending"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestFalse(TEXT("Three RuntimeIds are rejected"), BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, View.CandidateRuntimeIds));
	TestTrue(TEXT("Oversized rejection keeps request pending"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());

	TArray<int32> Valid{ View.CandidateRuntimeIds[0], View.CandidateRuntimeIds[1] };
	TestTrue(TEXT("Exact count still submits after rejected attempts"), BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, Valid));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	TestFalse(TEXT("Completed exact selection leaves no fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0FacadeStaleCandidateRejectedTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.StaleCandidateRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC0FacadeStaleCandidateRejectedTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* A = Fixture.CreatePlainCard(TEXT("C0StaleA"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ A }))) return false;
	UCardInstance* Card = Fixture.FindHandCard(TEXT("C0StaleA"));
	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	if (!TestNotNull(TEXT("Card exists"), Card) || !TestNotNull(TEXT("Resolver exists"), Resolver) || !TestNotNull(TEXT("Queue exists"), Queue)) return false;

	FSelectionRequest Request;
	Request.SelectionSource = TEXT("C0StaleCandidate");
	Request.MinCount = 1;
	Request.MaxCount = 1;
	Request.CancelPolicy = ESelectionCancelPolicy::Allowed;
	FSelectionCandidate Candidate;
	Candidate.RuntimeObject = Card;
	Candidate.RuntimeSequence = Card->GetRuntimeId() + 1000;
	Candidate.SelectionKey = Card->GetCardId();
	Request.Candidates.Add(Candidate);

	UExhaustSelectedContinuation* Continuation = NewObject<UExhaustSelectedContinuation>(Queue);
	USelectionRequestAction* PendingAction = NewObject<USelectionRequestAction>(Queue);
	TestTrue(TEXT("Generic resolver accepts structurally valid request"), Resolver->BeginSelection(Request, Continuation, PendingAction));

	FPendingCardSelectionReadView View;
	TestFalse(TEXT("Card facade rejects stale RuntimeId/object mapping"), BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Fixture.Battle, View));
	TestFalse(TEXT("Card facade refuses stale request submit"), BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, { Card->GetRuntimeId() }));
	TestTrue(TEXT("Rejected stale request remains pending until explicitly cleared"), Resolver->HasPendingSelection());
	TestTrue(TEXT("Test cleanup cancels allowed stale request"), Resolver->CancelSelection());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0RandomSingleSelectTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.SingleSelect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC0RandomSingleSelectTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* EffectCard = Fixture.CreateSelectExhaustCard(TEXT("C0RandomSingle"), ESelectExhaustSelectionMode::Random, 1);
	UCardData* A = Fixture.CreatePlainCard(TEXT("C0RandomSingleA"));
	UCardData* B = Fixture.CreatePlainCard(TEXT("C0RandomSingleB"));
	UCardData* C = Fixture.CreatePlainCard(TEXT("C0RandomSingleC"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ EffectCard, A, B, C }, 777))) return false;
	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* Played = Fixture.FindHandCard(TEXT("C0RandomSingle"));
	if (!TestNotNull(TEXT("Random card exists"), Played)) return false;

	TestTrue(TEXT("Random single play accepted"), Fixture.Battle->RequestPlayCard(Played, nullptr).IsAcceptedForResolution());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	TestFalse(TEXT("Random single creates no pending player selection"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestEqual(TEXT("Random single exhausts exactly one candidate"), Deck->GetExhaustCount(), 1);
	TestTrue(TEXT("Played card itself is never the exhausted candidate"), Deck->GetExhaustCards().Num() == 1 && Deck->GetExhaustCards()[0].Get() != Played);
	TestFalse(TEXT("Random single resolution has no fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0RandomCountClampTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.CountClampsToCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC0RandomCountClampTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* EffectCard = Fixture.CreateSelectExhaustCard(TEXT("C0RandomClamp"), ESelectExhaustSelectionMode::Random, 5);
	UCardData* A = Fixture.CreatePlainCard(TEXT("C0RandomClampA"));
	UCardData* B = Fixture.CreatePlainCard(TEXT("C0RandomClampB"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ EffectCard, A, B }, 321))) return false;
	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* Played = Fixture.FindHandCard(TEXT("C0RandomClamp"));
	if (!TestNotNull(TEXT("Random clamp card exists"), Played)) return false;

	TestTrue(TEXT("Random clamp play accepted"), Fixture.Battle->RequestPlayCard(Played, nullptr).IsAcceptedForResolution());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	TestFalse(TEXT("Random clamp creates no pending player selection"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestEqual(TEXT("Count five clamps to both available candidates"), Deck->GetExhaustCount(), 2);
	TestFalse(TEXT("Random clamp resolution has no fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
