#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/DeferredSelectionAction.h"
#include "Actions/DrawCardAction.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleSelectionRequest.h"
#include "Battle/BattleTextResolver.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/SelectHandCardToDrawPileTopEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/PresentationCardView.h"
#include "Presentation/PresentationTypes.h"
#include "Selection/MoveSelectedHandCardsToDrawPileTopContinuation.h"
#include "Selection/SelectionCandidateSource.h"
#include "Selection/SelectionResolver.h"
#include "Engine/World.h"

namespace CardExpansionWave1CC1DrawPileTopTest
{
	UCardData* MakePlainCard(UObject* Outer, const TCHAR* CardId)
	{
		UCardData* Card = NewObject<UCardData>(Outer);
		Card->CardId = FName(CardId);
		Card->DisplayName = FText::FromString(CardId);
		Card->BaseCost = 0;
		Card->UpgradedCost = 0;
		Card->CardType = ECardType::Skill;
		Card->TargetType = ECardTargetType::None;
		Card->DefaultDestination = ECardDestination::Discard;
		return Card;
	}

	FPresentationCardSnapshot MakeCardSnapshot(int32 RuntimeId, const TCHAR* CardId)
	{
		FPresentationCardSnapshot Card;
		Card.RuntimeId = RuntimeId;
		Card.CardId = FName(CardId);
		Card.DisplayName = FText::FromString(CardId);
		Card.Description = FText::FromString(TEXT("C1 reducer card."));
		return Card;
	}

	FSelectionInteractiveBoundaryAccess MakeNoPresentationBoundary()
	{
		return FSelectionInteractiveBoundaryAccess::CreateLambda(
			[](const UBattleAction*)
			{
				return FPresentationRecordWriter{};
			});
	}

	UDeferredSelectionAction* MakeCurrentHandPlayerSelection(
		UBattleActionQueue* Queue,
		UDeckRuntime* Deck,
		USelectionResolver* Resolver,
		UAuthoredContinuation* Continuation,
		int32 RequestedCount,
		FName SelectionSource)
	{
		UCurrentHandSelectionSource* CandidateSource = NewObject<UCurrentHandSelectionSource>(Queue);
		CandidateSource->Initialize(Deck);
		UDeferredSelectionAction* Deferred = NewObject<UDeferredSelectionAction>(Queue);
		Deferred->Initialize(
			CandidateSource,
			Resolver,
			Continuation,
			RequestedCount,
			ESelectionCancelPolicy::Forbidden,
			SelectionSource,
			EDeferredSelectionMode::Player,
			MakeNoPresentationBoundary());
		return Deferred;
	}

	struct FBattleFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

		FBattleFixture()
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

		~FBattleFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		UCardData* CreateEffectCard(const TCHAR* CardId, int32 BaseCount, int32 UpgradedCount)
		{
			UCardData* Card = MakePlainCard(World, CardId);
			USelectHandCardToDrawPileTopEffect* Effect = NewObject<USelectHandCardToDrawPileTopEffect>(Card);
			Effect->BaseSelectionCount = BaseCount;
			Effect->UpgradedSelectionCount = UpgradedCount;
			Card->Effects.Add(Effect);
			return Card;
		}

		bool Start(const TArray<UCardData*>& Definitions)
		{
			if (!IsValid(Battle) || Definitions.Num() == 0) return false;
			Battle->DebugStartingDeck.Reset();
			for (UCardData* Definition : Definitions) Battle->DebugStartingDeck.Add(Definition);
			Battle->OpeningHandDrawCount = Definitions.Num();
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			return Battle->BattleState == EBattleState::PlayerTurn
				&& IsValid(Battle->GetDeckRuntimeForTesting())
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

using namespace CardExpansionWave1CC1DrawPileTopTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC1HandToDrawTopCommitTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop.Move.HandToDrawTopCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC1HandToDrawTopCommitTest::RunTest(const FString& Parameters)
{
	UCardData* A = MakePlainCard(GetTransientPackage(), TEXT("C1MoveA"));
	UCardData* B = MakePlainCard(GetTransientPackage(), TEXT("C1MoveB"));
	UCardData* C = MakePlainCard(GetTransientPackage(), TEXT("C1MoveC"));
	TArray<TObjectPtr<UCardData>> Definitions{ A, B, C };
	UDeckRuntime* Deck = NewObject<UDeckRuntime>(GetTransientPackage());
	Deck->InitializeFromDefinitions(Definitions, 101);

	UCardInstance* DrawnA = nullptr;
	UCardInstance* DrawnB = nullptr;
	if (!TestTrue(TEXT("First setup draw commits"), Deck->TryDrawTopCardCommit(DrawnA).bCommitted)
		|| !TestTrue(TEXT("Second setup draw commits"), Deck->TryDrawTopCardCommit(DrawnB).bCommitted)
		|| !TestNotNull(TEXT("First drawn card exists"), DrawnA)
		|| !TestNotNull(TEXT("Second drawn card exists"), DrawnB))
	{
		return false;
	}

	UCardInstance* Selected = Deck->GetHandCards()[0].Get();
	const int32 ExpectedToIndex = Deck->GetDrawCount();
	const int32 HandCountBefore = Deck->GetHandCount();
	const FCardZoneMutationResult Result = Deck->TryMoveHandCardToDrawPileTopCommit(Selected);
	TestTrue(TEXT("Exact Hand->DrawPileTop mutation commits"), Result.bCommitted);
	TestEqual(TEXT("From zone is Hand"), Result.FromZone, ECardZone::Hand);
	TestEqual(TEXT("To zone is DrawPile"), Result.ToZone, ECardZone::DrawPile);
	TestEqual(TEXT("FromIndex is exact current Hand index"), Result.FromIndex, 0);
	TestEqual(TEXT("ToIndex is pre-insertion DrawCount"), Result.ToIndex, ExpectedToIndex);
	TestEqual(TEXT("Hand removes exactly one card"), Deck->GetHandCount(), HandCountBefore - 1);
	TestTrue(TEXT("Moved exact CardInstance is DrawPile top"), Deck->GetDrawCards().Last().Get() == Selected);

	UCardInstance* NextDraw = nullptr;
	TestTrue(TEXT("Immediate draw from top commits"), Deck->TryDrawTopCardCommit(NextDraw).bCommitted);
	TestTrue(TEXT("Immediate next draw returns the exact moved CardInstance"), NextDraw == Selected);

	const int32 DrawCountBeforeRejects = Deck->GetDrawCount();
	const int32 HandCountBeforeRejects = Deck->GetHandCount();
	TestFalse(TEXT("Null exact target is rejected"), Deck->TryMoveHandCardToDrawPileTopCommit(nullptr).bCommitted);
	UCardInstance* Foreign = NewObject<UCardInstance>(GetTransientPackage());
	Foreign->Initialize(A, 9999, false);
	TestFalse(TEXT("Foreign non-Hand target is rejected"), Deck->TryMoveHandCardToDrawPileTopCommit(Foreign).bCommitted);
	TestEqual(TEXT("Rejected moves preserve DrawPile"), Deck->GetDrawCount(), DrawCountBeforeRejects);
	TestEqual(TEXT("Rejected moves preserve Hand"), Deck->GetHandCount(), HandCountBeforeRejects);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC1DeferredUsesCurrentHandTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop.Selection.UsesPostPriorActionHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC1DeferredUsesCurrentHandTest::RunTest(const FString& Parameters)
{
	UCardData* A = MakePlainCard(GetTransientPackage(), TEXT("C1DeferredA"));
	UCardData* B = MakePlainCard(GetTransientPackage(), TEXT("C1DeferredB"));
	UCardData* C = MakePlainCard(GetTransientPackage(), TEXT("C1DeferredC"));
	TArray<TObjectPtr<UCardData>> Definitions{ A, B, C };
	UDeckRuntime* Deck = NewObject<UDeckRuntime>(GetTransientPackage());
	Deck->InitializeFromDefinitions(Definitions, 202);

	UCardInstance* InitialHandCard = nullptr;
	if (!TestTrue(TEXT("Setup draw commits"), Deck->TryDrawTopCardCommit(InitialHandCard).bCommitted)
		|| !TestNotNull(TEXT("Initial Hand card exists"), InitialHandCard)
		|| !TestTrue(TEXT("One card remains available to draw"), Deck->GetDrawCount() >= 1))
	{
		return false;
	}
	UCardInstance* ExpectedNewlyDrawn = Deck->GetDrawCards().Last().Get();

	UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(GetTransientPackage());
	USelectionResolver* Resolver = NewObject<USelectionResolver>(GetTransientPackage());
	FSelectionResolverQueueAccess QueueAccess;
	QueueAccess.BindLambda([Queue](const USelectionResolver*) { return Queue; });
	Resolver->Initialize(QueueAccess);

	UMoveSelectedHandCardsToDrawPileTopContinuation* Continuation = NewObject<UMoveSelectedHandCardsToDrawPileTopContinuation>(Queue);
	Continuation->Initialize(Deck, nullptr);
	UDrawCardAction* Draw = NewObject<UDrawCardAction>(Queue);
	Draw->Initialize(Deck);
	UDeferredSelectionAction* Deferred = MakeCurrentHandPlayerSelection(
		Queue, Deck, Resolver, Continuation, 2, FName(TEXT("C1DeferredCurrentHand")));

	TArray<UBattleAction*> Batch{ Draw, Deferred };
	if (!TestTrue(TEXT("Draw then Deferred batch enqueues"), Queue->AddBatchToBackPreserveOrder(Batch))
		|| !TestTrue(TEXT("Queue starts"), Queue->StartProcessing()))
	{
		return false;
	}

	const FSelectionRequest* Request = Resolver->GetPendingRequest();
	if (!TestNotNull(TEXT("Deferred action creates pending selection"), Request)) return false;
	TestEqual(TEXT("Deferred request is exact two"), Request->MinCount, 2);
	TestEqual(TEXT("Deferred request max is exact two"), Request->MaxCount, 2);
	TestEqual(TEXT("Deferred request sees two current Hand candidates"), Request->Candidates.Num(), 2);
	TestTrue(TEXT("Newly drawn exact CardInstance is eligible because candidates were read at Execute time"),
		Request->Candidates.ContainsByPredicate([ExpectedNewlyDrawn](const FSelectionCandidate& Candidate)
		{
			return Candidate.RuntimeObject.Get() == ExpectedNewlyDrawn;
		}));

	FSelectionResult Selection;
	Selection.Status = ESelectionStatus::Resolved;
	for (const FSelectionCandidate& Candidate : Request->Candidates) Selection.SelectedObjects.Add(Candidate.RuntimeObject.Get());
	UCardInstance* FirstCanonical = Cast<UCardInstance>(Selection.SelectedObjects[0].Get());
	UCardInstance* SecondCanonical = Cast<UCardInstance>(Selection.SelectedObjects[1].Get());
	TestTrue(TEXT("Resolved selection submits"), Resolver->SubmitResult(Selection));
	TestFalse(TEXT("Queue finishes without resolution fault"), Queue->IsResolutionFaulted());
	TestFalse(TEXT("Selection clears after exact result"), Resolver->HasPendingSelection());
	TestEqual(TEXT("Both selected cards leave Hand"), Deck->GetHandCount(), 0);
	TestTrue(TEXT("Last canonical selected card becomes DrawPile top"), Deck->GetDrawCards().Last().Get() == SecondCanonical);

	UCardInstance* DrawTop = nullptr;
	UCardInstance* DrawSecond = nullptr;
	TestTrue(TEXT("First verification draw commits"), Deck->TryDrawTopCardCommit(DrawTop).bCommitted);
	TestTrue(TEXT("First verification draw returns last canonical selected"), DrawTop == SecondCanonical);
	TestTrue(TEXT("Second verification draw commits"), Deck->TryDrawTopCardCommit(DrawSecond).bCommitted);
	TestTrue(TEXT("Second verification draw returns first canonical selected"), DrawSecond == FirstCanonical);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC1DeferredCountBoundsTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop.Selection.CountZeroAndClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC1DeferredCountBoundsTest::RunTest(const FString& Parameters)
{
	UCardData* A = MakePlainCard(GetTransientPackage(), TEXT("C1BoundsA"));
	UCardData* B = MakePlainCard(GetTransientPackage(), TEXT("C1BoundsB"));
	TArray<TObjectPtr<UCardData>> Definitions{ A, B };

	UDeckRuntime* ZeroDeck = NewObject<UDeckRuntime>(GetTransientPackage());
	ZeroDeck->InitializeFromDefinitions(Definitions, 303);
	UCardInstance* ZeroA = nullptr;
	UCardInstance* ZeroB = nullptr;
	ZeroDeck->TryDrawTopCardCommit(ZeroA);
	ZeroDeck->TryDrawTopCardCommit(ZeroB);
	UBattleActionQueue* ZeroQueue = NewObject<UBattleActionQueue>(GetTransientPackage());
	USelectionResolver* ZeroResolver = NewObject<USelectionResolver>(GetTransientPackage());
	ZeroResolver->Initialize(FSelectionResolverQueueAccess::CreateLambda([ZeroQueue](const USelectionResolver*) { return ZeroQueue; }));
	UMoveSelectedHandCardsToDrawPileTopContinuation* ZeroContinuation = NewObject<UMoveSelectedHandCardsToDrawPileTopContinuation>(ZeroQueue);
	ZeroContinuation->Initialize(ZeroDeck, nullptr);
	UDeferredSelectionAction* Zero = MakeCurrentHandPlayerSelection(
		ZeroQueue, ZeroDeck, ZeroResolver, ZeroContinuation, 0, FName(TEXT("C1Zero")));
	TestTrue(TEXT("Count-zero deferred action enqueues"), ZeroQueue->AddToBack(Zero));
	TestTrue(TEXT("Count-zero queue starts"), ZeroQueue->StartProcessing());
	TestFalse(TEXT("Count zero creates no pending selection"), ZeroResolver->HasPendingSelection());
	TestEqual(TEXT("Count zero leaves Hand unchanged"), ZeroDeck->GetHandCount(), 2);
	TestEqual(TEXT("Count zero leaves DrawPile unchanged"), ZeroDeck->GetDrawCount(), 0);

	UDeckRuntime* ClampDeck = NewObject<UDeckRuntime>(GetTransientPackage());
	ClampDeck->InitializeFromDefinitions(Definitions, 404);
	UCardInstance* ClampA = nullptr;
	UCardInstance* ClampB = nullptr;
	ClampDeck->TryDrawTopCardCommit(ClampA);
	ClampDeck->TryDrawTopCardCommit(ClampB);
	UBattleActionQueue* ClampQueue = NewObject<UBattleActionQueue>(GetTransientPackage());
	USelectionResolver* ClampResolver = NewObject<USelectionResolver>(GetTransientPackage());
	ClampResolver->Initialize(FSelectionResolverQueueAccess::CreateLambda([ClampQueue](const USelectionResolver*) { return ClampQueue; }));
	UMoveSelectedHandCardsToDrawPileTopContinuation* ClampContinuation = NewObject<UMoveSelectedHandCardsToDrawPileTopContinuation>(ClampQueue);
	ClampContinuation->Initialize(ClampDeck, nullptr);
	UDeferredSelectionAction* Clamp = MakeCurrentHandPlayerSelection(
		ClampQueue, ClampDeck, ClampResolver, ClampContinuation, 5, FName(TEXT("C1Clamp")));
	TestTrue(TEXT("Clamp deferred action enqueues"), ClampQueue->AddToBack(Clamp));
	TestTrue(TEXT("Clamp queue starts"), ClampQueue->StartProcessing());
	const FSelectionRequest* ClampRequest = ClampResolver->GetPendingRequest();
	if (!TestNotNull(TEXT("Clamp creates pending selection"), ClampRequest)) return false;
	TestEqual(TEXT("Requested five clamps to two current candidates"), ClampRequest->MinCount, 2);
	TestEqual(TEXT("Clamp remains exact-N"), ClampRequest->MaxCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC1EffectConfigAndFinishTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop.Effect.ConfigurableExactNAndFinish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC1EffectConfigAndFinishTest::RunTest(const FString& Parameters)
{
	FBattleFixture Fixture;
	UCardData* EffectCard = Fixture.CreateEffectCard(TEXT("C1Effect"), 2, 3);
	UCardData* A = MakePlainCard(Fixture.World, TEXT("C1EffectA"));
	UCardData* B = MakePlainCard(Fixture.World, TEXT("C1EffectB"));
	UCardData* C = MakePlainCard(Fixture.World, TEXT("C1EffectC"));
	if (!TestTrue(TEXT("Battle fixture starts"), Fixture.Start({ EffectCard, A, B, C }))) return false;

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* Played = Fixture.FindHandCard(TEXT("C1Effect"));
	if (!TestNotNull(TEXT("Effect card exists in Hand"), Played)) return false;
	TestTrue(TEXT("Effect card play is accepted"), Fixture.Battle->RequestPlayCard(Played, nullptr).IsAcceptedForResolution());

	FPendingCardSelectionReadView View;
	if (!TestTrue(TEXT("Effect exposes current-Hand pending selection"),
		BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Fixture.Battle, View))) return false;
	TestEqual(TEXT("Base Blueprint count requires exactly two"), View.RequiredCount, 2);
	if (!TestTrue(TEXT("At least two candidate RuntimeIds are available"), View.CandidateRuntimeIds.Num() >= 2)) return false;

	const int32 FirstSelectedRuntimeId = View.CandidateRuntimeIds[0];
	const int32 SecondSelectedRuntimeId = View.CandidateRuntimeIds[1];
	TestTrue(TEXT("Exact two RuntimeIds submit through Gameplay facade"),
		BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, { FirstSelectedRuntimeId, SecondSelectedRuntimeId }));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	TestFalse(TEXT("Effect resolution has no fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	TestTrue(TEXT("Played effect card finishes normally to Discard"), Deck->GetDiscardCards().Contains(Played));
	TestEqual(TEXT("Two selected cards are now on DrawPile"), Deck->GetDrawCount(), 2);
	TestEqual(TEXT("Only one unselected candidate remains in Hand"), Deck->GetHandCount(), 1);
	TestEqual(TEXT("Top card is the second canonical selected RuntimeId"), Deck->GetDrawCards().Last()->GetRuntimeId(), SecondSelectedRuntimeId);
	TestEqual(TEXT("Card directly below top is the first canonical selected RuntimeId"),
		Deck->GetDrawCards()[Deck->GetDrawCards().Num() - 2]->GetRuntimeId(), FirstSelectedRuntimeId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC1DescriptionBaseUpgradeTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop.Description.BaseUpgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC1DescriptionBaseUpgradeTest::RunTest(const FString& Parameters)
{
	UCardData* Definition = MakePlainCard(GetTransientPackage(), TEXT("C1Description"));
	USelectHandCardToDrawPileTopEffect* Effect = NewObject<USelectHandCardToDrawPileTopEffect>(Definition);
	Effect->BaseSelectionCount = 1;
	Effect->UpgradedSelectionCount = 3;
	Definition->Effects.Add(Effect);
	TestEqual(TEXT("Description argument has semantic non-None default"), Effect->DescriptionArgumentName, FName(TEXT("DrawPileTopCount")));

	UCardInstance* Base = NewObject<UCardInstance>(GetTransientPackage());
	Base->Initialize(Definition, 1, false);
	UCardInstance* Upgraded = NewObject<UCardInstance>(GetTransientPackage());
	Upgraded->Initialize(Definition, 2, true);
	TestEqual(TEXT("Base auto description uses BaseSelectionCount"),
		FBattleTextResolver::ResolveCardDescription(Base, nullptr).ToString(),
		FString(TEXT("将手牌中的 1 张牌放到你的抽牌堆顶部。")));
	TestEqual(TEXT("Upgraded auto description uses UpgradedSelectionCount"),
		FBattleTextResolver::ResolveCardDescription(Upgraded, nullptr).ToString(),
		FString(TEXT("将手牌中的 3 张牌放到你的抽牌堆顶部。")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC1HandToDrawTopReducerTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop.Presentation.HandToDrawTopReducer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC1HandToDrawTopReducerTest::RunTest(const FString& Parameters)
{
	const FPresentationCardSnapshot A = MakeCardSnapshot(501, TEXT("C1ReducerA"));
	const FPresentationCardSnapshot B = MakeCardSnapshot(502, TEXT("C1ReducerB"));
	const FPresentationCardSnapshot Untouched = MakeCardSnapshot(503, TEXT("C1ReducerUntouched"));

	FPresentationStateSnapshot Baseline;
	Baseline.BattleId = 91;
	Baseline.StateRevision = 30;
	Baseline.BattleState = EBattleState::PlayerTurn;
	Baseline.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(A));
	Baseline.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(B));
	Baseline.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(Untouched));
	Baseline.DrawCount = 4;

	FPresentationResolutionEnvelope Envelope;
	Envelope.BattleId = 91;
	Envelope.ResolutionId = 31;
	auto AddMove = [&Envelope](const FPresentationCardSnapshot& Card, int32 Sequence, int32 FromIndex, int32 ToIndex)
	{
		FPresentationRecord Record;
		Record.Type = EBattlePresentationRecordType::CardZoneChanged;
		Record.BattleId = 91;
		Record.ResolutionId = 31;
		Record.PresentationSequence = Sequence;
		Record.CardZoneChanged.Card = Card;
		Record.CardZoneChanged.FromZone = ECardZone::Hand;
		Record.CardZoneChanged.ToZone = ECardZone::DrawPile;
		Record.CardZoneChanged.FromIndex = FromIndex;
		Record.CardZoneChanged.ToIndex = ToIndex;
		Envelope.Records.Add(Record);
	};
	AddMove(A, 1, 0, 4);
	AddMove(B, 2, 0, 5);

	Envelope.FinalSnapshot = Baseline;
	Envelope.FinalSnapshot.StateRevision = 31;
	Envelope.FinalSnapshot.HandCards.Reset();
	Envelope.FinalSnapshot.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(Untouched));
	Envelope.FinalSnapshot.DrawCount = 6;
	Envelope.FinalStateRevision = 31;

	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(GetTransientPackage());
	if (!TestNotNull(TEXT("Controller exists"), Controller)) return false;

	FPresentationStateSnapshot Reduced;
	TestTrue(TEXT("Reducer accepts sequential exact Hand->DrawPileTop records"),
		Controller->ReduceEnvelopeForTesting(Baseline, Envelope, Reduced));
	if (TestEqual(TEXT("Only untouched Hand card remains"), Reduced.HandCards.Num(), 1))
	{
		TestEqual(TEXT("Untouched RuntimeId is preserved"), Reduced.HandCards[0].RuntimeId, Untouched.RuntimeId);
	}
	TestEqual(TEXT("Reducer increments DrawCount once per moved card"), Reduced.DrawCount, 6);

	FPresentationResolutionEnvelope BadToIndex = Envelope;
	BadToIndex.Records[1].CardZoneChanged.ToIndex = 4;
	FPresentationStateSnapshot Ignored;
	TestFalse(TEXT("Reducer rejects stale DrawPile destination index"), Controller->ReduceEnvelopeForTesting(Baseline, BadToIndex, Ignored));
	FPresentationResolutionEnvelope BadFromIndex = Envelope;
	BadFromIndex.Records[1].CardZoneChanged.FromIndex = 1;
	TestFalse(TEXT("Reducer rejects stale Hand source index after prior removal"), Controller->ReduceEnvelopeForTesting(Baseline, BadFromIndex, Ignored));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
