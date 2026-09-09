#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/DeferredSelectionAction.h"
#include "CardExpansionWave1CTestTypes.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleSelectionRequest.h"
#include "Phase6UIA2D5TestTypes.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DrawCardEffect.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Cards/Effects/SelectHandCardToDrawPileTopEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Presentation/BattlePresentationController.h"
#include "Selection/AuthoredContinuation.h"
#include "Selection/SelectionCandidateSource.h"
#include "Selection/SelectionResolver.h"
#include "UI/BattleHUDViewModel.h"
#include "Engine/World.h"

namespace UnifiedCardSelectionTest
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
			Player->PresentationId = TEXT("UnifiedPlayer");
			Enemy->PresentationId = TEXT("UnifiedEnemy");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
		}

		~FFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		UCardData* Plain(const TCHAR* Id)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(Id);
			Card->DisplayName = FText::FromString(Id);
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* DrawThenExhaust(const TCHAR* Id, ESelectExhaustSelectionMode Mode, int32 Count)
		{
			UCardData* Card = Plain(Id);
			UDrawCardEffect* Draw = NewObject<UDrawCardEffect>(Card);
			Draw->DrawCount = 1;
			Draw->UpgradedDrawCount = 1;
			Card->Effects.Add(Draw);

			USelectExhaustHandCardEffect* Select = NewObject<USelectExhaustHandCardEffect>(Card);
			Select->BaseSelectionMode = Mode;
			Select->UpgradedSelectionMode = Mode;
			Select->BaseSelectionCount = Count;
			Select->UpgradedSelectionCount = Count;
			Card->Effects.Add(Select);
			return Card;
		}

		bool Start(const TArray<UCardData*>& Definitions, bool bEnableRecording = true, int32 OpeningCount = INDEX_NONE)
		{
			if (!IsValid(Battle) || Definitions.Num() == 0) return false;
			Battle->bEnableCommittedPresentationRecording = bEnableRecording;
			Battle->OpeningHandDrawCount = OpeningCount == INDEX_NONE ? Definitions.Num() : OpeningCount;
			Battle->DebugStartingDeck.Reset();
			for (UCardData* Definition : Definitions) Battle->DebugStartingDeck.Add(Definition);
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			return Battle->BattleState == EBattleState::PlayerTurn
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->GetDeckRuntimeForTesting()->GetHandCount() == Battle->OpeningHandDrawCount;
		}

		UCardInstance* FindHand(FName Id) const
		{
			UDeckRuntime* Deck = IsValid(Battle) ? Battle->GetDeckRuntimeForTesting() : nullptr;
			if (!IsValid(Deck)) return nullptr;
			for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
			{
				if (IsValid(Card.Get()) && Card->GetCardId() == Id) return Card.Get();
			}
			return nullptr;
		}
	};

}

using namespace UnifiedCardSelectionTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedDrawBeforePlayerExhaustCapturesPostDrawHand,
	"SlayTheSpireDemo.CardSelection.Unified.DrawBeforePlayerExhaust.CapturesPostDrawHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedDrawBeforePlayerExhaustCapturesPostDrawHand::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* Play = Fixture.DrawThenExhaust(TEXT("UnifiedDrawPlayer"), ESelectExhaustSelectionMode::Player, 1);
	UCardData* Drawn = Fixture.Plain(TEXT("UnifiedDrawn"));
	if (!TestTrue(TEXT("Fixture starts with no other Hand candidate before Draw"), Fixture.Start({ Play, Drawn }))) return false;

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayCard = Fixture.FindHand(TEXT("UnifiedDrawPlayer"));
	UCardInstance* DrawnCard = Fixture.FindHand(TEXT("UnifiedDrawn"));
	if (!TestNotNull(TEXT("Played card exists"), PlayCard)
		|| !TestNotNull(TEXT("Draw target exists"), DrawnCard)) return false;

	// Put one known card in DrawPile so the authored Draw really changes Hand.
	TestTrue(TEXT("Draw target moves to DrawPile"), Deck->TryMoveHandCardToDrawPileTopCommit(DrawnCard).bCommitted);
	TestTrue(TEXT("Draw then Player Exhaust play is accepted"), Fixture.Battle->RequestPlayCard(PlayCard, nullptr).IsAcceptedForResolution());

	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	const FSelectionRequest* Request = IsValid(Resolver) ? Resolver->GetPendingRequest() : nullptr;
	if (!TestNotNull(TEXT("Player selection is pending after the Draw commit"), Request)) return false;
	TestEqual(TEXT("Exactly one card is requested"), Request->MinCount, 1);
	TestTrue(
		TEXT("The card drawn by the preceding Effect is a candidate"),
		Request->Candidates.ContainsByPredicate([DrawnCard](const FSelectionCandidate& Candidate)
		{
			return Candidate.RuntimeObject.Get() == DrawnCard;
		}));

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(DrawnCard);
	TestTrue(TEXT("Newly drawn card resolves through the shared Player path"), Resolver->SubmitResult(Result));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	TestTrue(TEXT("Drawn card is exhausted by the authored continuation"), Deck->GetExhaustCards().Contains(DrawnCard));
	TestFalse(TEXT("Draw-before-selection has no resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedDrawBeforeRandomExhaustCapturesPostDrawHand,
	"SlayTheSpireDemo.CardSelection.Unified.DrawBeforeRandomExhaust.CapturesPostDrawHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedDrawBeforeRandomExhaustCapturesPostDrawHand::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* Play = Fixture.DrawThenExhaust(TEXT("UnifiedDrawRandom"), ESelectExhaustSelectionMode::Random, 3);
	UCardData* SurvivorA = Fixture.Plain(TEXT("UnifiedRandomA"));
	UCardData* SurvivorB = Fixture.Plain(TEXT("UnifiedRandomB"));
	UCardData* Drawn = Fixture.Plain(TEXT("UnifiedRandomDrawn"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ Play, SurvivorA, SurvivorB, Drawn }))) return false;

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayCard = Fixture.FindHand(TEXT("UnifiedDrawRandom"));
	UCardInstance* DrawnCard = Fixture.FindHand(TEXT("UnifiedRandomDrawn"));
	if (!TestNotNull(TEXT("Played card exists"), PlayCard)
		|| !TestNotNull(TEXT("Draw target exists"), DrawnCard)) return false;
	TestTrue(TEXT("Draw target moves to DrawPile"), Deck->TryMoveHandCardToDrawPileTopCommit(DrawnCard).bCommitted);

	TestTrue(TEXT("Draw then Random Exhaust play is accepted"), Fixture.Battle->RequestPlayCard(PlayCard, nullptr).IsAcceptedForResolution());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	TestFalse(TEXT("Random mode never creates a pending Player selection"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestEqual(TEXT("Count three includes both survivors and the newly drawn card"), Deck->GetExhaustCount(), 3);
	TestTrue(TEXT("Newly drawn card participates in Random selection"), Deck->GetExhaustCards().Contains(DrawnCard));
	TestFalse(TEXT("Draw-before-random-selection has no resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedDeferredSelectionCountAndCandidateStatuses,
	"SlayTheSpireDemo.CardSelection.Unified.CountPolicyAndCandidateStatuses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedDeferredSelectionCountAndCandidateStatuses::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("Test world exists"), Fixture.World)) return false;

	UCurrentHandSelectionSource* InvalidSource = NewObject<UCurrentHandSelectionSource>(Fixture.World);
	TArray<FSelectionCandidate> Candidates;
	TestTrue(TEXT("Null Deck is distinguished as InvalidRuntimeDependency"), InvalidSource->BuildCandidates(Candidates) == ESelectionCandidateBuildStatus::InvalidRuntimeDependency);

	UDeckRuntime* EmptyDeck = NewObject<UDeckRuntime>(Fixture.World);
	InvalidSource->Initialize(EmptyDeck);
	TestTrue(TEXT("Valid empty Hand is distinguished as NoCandidates"), InvalidSource->BuildCandidates(Candidates) == ESelectionCandidateBuildStatus::NoCandidates);

	UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
	UDeferredSelectionAction* NoOp = NewObject<UDeferredSelectionAction>(Queue);
	bool bBoundaryCalled = false;
	FSelectionInteractiveBoundaryAccess Boundary = FSelectionInteractiveBoundaryAccess::CreateLambda(
		[&bBoundaryCalled](const UBattleAction*)
		{
			bBoundaryCalled = true;
			return FPresentationRecordWriter{};
		});
	NoOp->Initialize(nullptr, nullptr, nullptr, 0, ESelectionCancelPolicy::Forbidden, TEXT("UnifiedZero"), EDeferredSelectionMode::Player, Boundary);
	TestTrue(TEXT("Zero-count deferred selection enqueues"), Queue->AddToBack(NoOp));
	TestTrue(TEXT("Zero-count deferred selection processes"), Queue->StartProcessing());
	TestTrue(TEXT("RequestedCount <= 0 is a legal finished no-op"), NoOp->IsFinished());
	TestFalse(TEXT("Zero-count no-op does not establish an interactive boundary"), bBoundaryCalled);
	TestFalse(TEXT("Zero-count no-op does not fault the queue"), Queue->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedNoHistoryKeepsGameplaySelectionAuthoritative,
	"SlayTheSpireDemo.CardSelection.Unified.NoHistoryKeepsPendingGameplaySelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedNoHistoryKeepsGameplaySelectionAuthoritative::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* Play = Fixture.DrawThenExhaust(TEXT("UnifiedNoHistory"), ESelectExhaustSelectionMode::Player, 1);
	UCardData* Survivor = Fixture.Plain(TEXT("UnifiedNoHistorySurvivor"));
	if (!TestTrue(TEXT("Fixture starts with recording disabled"), Fixture.Start({ Play, Play }, false, 1))) return false;

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	if (!TestTrue(TEXT("No-history ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true))) return false;
	if (!TestTrue(TEXT("No-history PresentationController initializes"), Controller->Initialize(Fixture.Battle, ViewModel, nullptr))) return false;
	UBattleHUDViewModel* DirectView = NewObject<UBattleHUDViewModel>(Fixture.World);
	TestTrue(TEXT("Direct no-history display initializes"), DirectView->Initialize(Fixture.Battle, false));
	const int64 RevisionBeforePlay = ViewModel->StateRevision;
	UCardInstance* PlayCard = Fixture.FindHand(TEXT("UnifiedNoHistory"));
	if (!TestNotNull(TEXT("No-history played card exists"), PlayCard)) return false;

	TestTrue(TEXT("No-history Draw then Player Exhaust play is accepted"), Fixture.Battle->RequestPlayCard(PlayCard, nullptr).IsAcceptedForResolution());
	TestFalse(TEXT("No-history choice is hidden until its new baseline is displayed"), ViewModel->HasPendingCardSelection());
	TestFalse(TEXT("Direct display also hides selection before frozen delivery"), DirectView->HasPendingCardSelection());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	TestTrue(TEXT("Gameplay selection remains pending without recorded history"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	FPresentationStateSnapshot Baseline;
	TestTrue(TEXT("No-history mode still freezes a read baseline"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline));
	TestTrue(TEXT("Decision boundary advances the no-history revision"), Baseline.StateRevision > RevisionBeforePlay);
	TestTrue(TEXT("No-history ViewModel exposes the authoritative pending choice"), ViewModel->HasPendingCardSelection());
	TestTrue(TEXT("Direct display exposes selection after frozen delivery"), DirectView->HasPendingCardSelection());
	TestEqual(TEXT("No-history boundary does not queue a sealed Presentation delivery"), Fixture.Battle->GetPendingPresentationDeliveryCountForTesting(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedConsecutiveIdenticalSelectionsResetTransientInput,
	"SlayTheSpireDemo.CardSelection.Unified.ConsecutiveIdenticalSelectionsResetInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedConsecutiveIdenticalSelectionsResetTransientInput::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* A = Fixture.Plain(TEXT("UnifiedSameA"));
	UCardData* B = Fixture.Plain(TEXT("UnifiedSameB"));
	if (!TestTrue(TEXT("Fixture starts in recording-disabled mode"), Fixture.Start({ A, B }, false))) return false;

	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	if (!TestNotNull(TEXT("Battle queue exists"), Queue)
		|| !TestNotNull(TEXT("Selection resolver exists"), Resolver)) return false;

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCurrentHandSelectionSource* SourceA = NewObject<UCurrentHandSelectionSource>(Queue);
	UCurrentHandSelectionSource* SourceB = NewObject<UCurrentHandSelectionSource>(Queue);
	SourceA->Initialize(Deck);
	SourceB->Initialize(Deck);
	int32 MarkerCount = 0;
	UWave1CTestContinuation* ContinuationA = NewObject<UWave1CTestContinuation>(Queue);
	UWave1CTestContinuation* ContinuationB = NewObject<UWave1CTestContinuation>(Queue);
	ContinuationA->Configure(&MarkerCount, TEXT("SameSignature"), 1);
	ContinuationB->Configure(&MarkerCount, TEXT("SameSignature"), 1);

	FSelectionInteractiveBoundaryAccess Boundary;
	Boundary.BindUObject(Fixture.Battle, &ABattleManager::AdvancePresentationAtInteractiveSelectionBoundary);
	UDeferredSelectionAction* First = NewObject<UDeferredSelectionAction>(Queue);
	UDeferredSelectionAction* Second = NewObject<UDeferredSelectionAction>(Queue);
	First->Initialize(SourceA, Resolver, ContinuationA, 2, ESelectionCancelPolicy::Forbidden,
		TEXT("UnifiedSameSignature"), EDeferredSelectionMode::Player, Boundary);
	Second->Initialize(SourceB, Resolver, ContinuationB, 2, ESelectionCancelPolicy::Forbidden,
		TEXT("UnifiedSameSignature"), EDeferredSelectionMode::Player, Boundary);
	TestTrue(TEXT("First deferred selection enqueues"), Queue->AddToBack(First));
	TestTrue(TEXT("Second deferred selection enqueues"), Queue->AddToBack(Second));

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	if (!TestTrue(TEXT("Selection ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true))) return false;
	if (!TestTrue(TEXT("Selection PresentationController initializes"), Controller->Initialize(Fixture.Battle, ViewModel, nullptr))) return false;
	TestTrue(TEXT("Selection queue starts"), Queue->StartProcessing());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	FPendingCardSelectionReadView FirstView;
	if (!TestTrue(TEXT("First selection is visible"), ViewModel->TryGetPendingCardSelectionReadView(FirstView))) return false;
	TestEqual(TEXT("First request has two candidates"), FirstView.CandidateRuntimeIds.Num(), 2);
	const int32 FirstCandidate = FirstView.CandidateRuntimeIds[0];
	const int64 FirstRevision = ViewModel->StateRevision;
	TestTrue(TEXT("First click enters transient selected state"), ViewModel->SubmitPendingCardSelectionByRuntimeId(FirstCandidate));
	TestEqual(TEXT("First choice retains partial input"), ViewModel->GetPendingCardSelectionSelectedCount(), 1);
	TestTrue(TEXT("Authoritative submission completes first choice independently of UI partial state"),
		BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, FirstView.CandidateRuntimeIds));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	FPendingCardSelectionReadView SecondView;
	if (!TestTrue(TEXT("Second identical selection is visible"), ViewModel->TryGetPendingCardSelectionReadView(SecondView))) return false;
	TestTrue(TEXT("Second request keeps the same candidate set"), SecondView.CandidateRuntimeIds == FirstView.CandidateRuntimeIds);
	TestEqual(TEXT("Second request keeps the same source"), SecondView.SelectionSource, FirstView.SelectionSource);
	TestTrue(TEXT("Consecutive identical decisions receive distinct revisions"), ViewModel->StateRevision > FirstRevision);
	TestEqual(TEXT("Previous selection highlight is cleared on revision change"), ViewModel->GetPendingCardSelectionSelectedCount(), 0);
	TestTrue(TEXT("Second choice resolves after reset"), ViewModel->SubmitPendingCardSelectionByRuntimeIds(SecondView.CandidateRuntimeIds));
	TestEqual(TEXT("Both no-op continuations executed"), MarkerCount, 2);
	TestFalse(TEXT("Consecutive identical selections do not fault"), Queue->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedPresentationDegradationKeepsPendingSelection,
	"SlayTheSpireDemo.CardSelection.Unified.PresentationDegradationKeepsPendingSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedPresentationDegradationKeepsPendingSelection::RunTest(const FString& Parameters)
{
	{
		FFixture Fixture;
		UCardData* Play = Fixture.DrawThenExhaust(TEXT("UnifiedSkipCatchup"), ESelectExhaustSelectionMode::Player, 1);
		UCardData* Drawn = Fixture.Plain(TEXT("UnifiedSkipDrawn"));
		if (!TestTrue(TEXT("Skip fixture starts"), Fixture.Start({ Play, Play }, true, 1))) return false;
		UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
		UCardInstance* PlayCard = Fixture.FindHand(TEXT("UnifiedSkipCatchup"));
		UCardInstance* DrawnCard = Deck->GetDrawCards()[0].Get();
		if (!TestNotNull(TEXT("Skip fixture played card exists"), PlayCard)
			|| !TestNotNull(TEXT("Skip fixture Draw target exists"), DrawnCard)) return false;


		UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
		UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
		UPhase6UIA2D5PlaybackWidget* Widget = NewObject<UPhase6UIA2D5PlaybackWidget>(Fixture.World);
		if (!TestTrue(TEXT("Skip fixture ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true))
			|| !TestTrue(TEXT("Skip fixture Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, Widget))) return false;
		Widget->SetViewModel(ViewModel);
		Widget->SetPresentationController(Controller);

		TestTrue(TEXT("Skip fixture play is accepted"), Fixture.Battle->RequestPlayCard(PlayCard, nullptr).IsAcceptedForResolution());
		TestTrue(TEXT("Skip fixture Gameplay selection is pending immediately"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
		Fixture.Battle->FlushScheduledReadStateReadyForTesting();
		TestTrue(TEXT("Skip fixture has an active Presentation playback"), Controller->IsWaitingForCompletionForTesting());
		Controller->SkipPresentation();
		TestFalse(TEXT("Skip catch-up completes the active Presentation"), Controller->IsWaitingForCompletionForTesting());
		TestTrue(TEXT("Skip catch-up exposes the still-pending authoritative choice"), ViewModel->HasPendingCardSelection());
		TestTrue(TEXT("Skip catch-up does not fault Gameplay"), !Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	}

	{
		FFixture Fixture;
		UCardData* Play = Fixture.DrawThenExhaust(TEXT("UnifiedUnavailable"), ESelectExhaustSelectionMode::Player, 1);
		UCardData* Candidate = Fixture.Plain(TEXT("UnifiedUnavailableCandidate"));
		if (!TestTrue(TEXT("Unavailable fixture starts"), Fixture.Start({ Play, Candidate }, true))) return false;
		UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
		UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
		if (!TestTrue(TEXT("Unavailable fixture ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true))
			|| !TestTrue(TEXT("Unavailable fixture Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, nullptr))) return false;
		Fixture.Battle->SetForcePresentationFreezeFailureForTesting(true);
		UCardInstance* PlayCard = Fixture.FindHand(TEXT("UnifiedUnavailable"));
		if (!TestNotNull(TEXT("Unavailable fixture played card exists"), PlayCard)) return false;

		TestTrue(TEXT("Unavailable fixture play is accepted"), Fixture.Battle->RequestPlayCard(PlayCard, nullptr).IsAcceptedForResolution());
		TestTrue(TEXT("Presentation freeze failure marks PresentationUnavailable"), !Fixture.Battle->IsPresentationAvailable());
		TestTrue(TEXT("PresentationUnavailable preserves mandatory Gameplay pending state"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
		Fixture.Battle->FlushScheduledReadStateReadyForTesting();
		TestEqual(TEXT("PresentationUnavailable is visible in the ViewModel"), ViewModel->InteractionState, EBattleHUDInteractionState::PresentationUnavailable);
		TestFalse(TEXT("PresentationUnavailable keeps the mandatory choice hidden under the existing disabled-input policy"), ViewModel->HasPendingCardSelection());
		TestFalse(TEXT("Presentation failure alone does not fault Gameplay"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
		(void)Controller;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedSelectionEnqueueFailureTest,
	"SlayTheSpireDemo.CardSelection.Unified.Failure.RequiredSelectionInsertion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedSelectionEnqueueFailureTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ Fixture.Plain(TEXT("InsertionCandidate")) }, false))) return false;
	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	UCurrentHandSelectionSource* Source = NewObject<UCurrentHandSelectionSource>(Queue);
	Source->Initialize(Fixture.Battle->GetDeckRuntimeForTesting());
	int32 TailRuns = 0;
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Queue);
	Continuation->Configure(nullptr, TEXT("Insertion"), 0);
	FSelectionInteractiveBoundaryAccess Boundary = FSelectionInteractiveBoundaryAccess::CreateLambda(
		[Queue](const UBattleAction*)
		{
			// Inject a Gameplay fault between capture and insertion; the next
			// required Action must be rejected and the original tail must not run.
			Queue->RequestResolutionFault(TEXT("Injected selection insertion failure."));
			return FPresentationRecordWriter{};
		});
	UDeferredSelectionAction* Action = NewObject<UDeferredSelectionAction>(Queue);
	Action->Initialize(Source, Fixture.Battle->GetSelectionResolver(), Continuation, 1,
		ESelectionCancelPolicy::Forbidden, TEXT("Insertion"), EDeferredSelectionMode::Player, Boundary);
	UWave1CTestMarkerAction* Tail = NewObject<UWave1CTestMarkerAction>(Queue);
	Tail->Initialize(&TailRuns, TEXT("ForbiddenTail"));
	Queue->AddToBack(Action);
	Queue->AddToBack(Tail);
	AddExpectedError(TEXT("Resolution fault requested:"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 2);
	Queue->StartProcessing();
	TestTrue(TEXT("Insertion rejection reaches fault"), Queue->IsResolutionFaulted());
	TestEqual(TEXT("Later authored work never runs"), TailRuns, 0);
	TestFalse(TEXT("Rejected insertion leaves no pending choice"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedRecordedMultiBoundaryTest,
	"SlayTheSpireDemo.CardSelection.Unified.RecordedMultiBoundaryCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedRecordedMultiBoundaryTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* Play = Fixture.DrawThenExhaust(TEXT("UnifiedTwoDecisions"), ESelectExhaustSelectionMode::Player, 1);
	USelectHandCardToDrawPileTopEffect* Top = NewObject<USelectHandCardToDrawPileTopEffect>(Play);
	Top->BaseSelectionCount = Top->UpgradedSelectionCount = 1;
	Play->Effects.Add(Top);
	if (!TestTrue(TEXT("Two-decision fixture starts"), Fixture.Start({ Play, Play, Play }, true, 2))) return false;
	TArray<FPresentationResolutionEnvelope> Envelopes;
	Fixture.Battle->OnPresentationResolutionReady.AddLambda([&Envelopes](const FPresentationResolutionEnvelope& Envelope) { Envelopes.Add(Envelope); });
	UCardInstance* Played = Fixture.FindHand(TEXT("UnifiedTwoDecisions"));
	const int32 PlayedId = Played->GetRuntimeId();
	TestTrue(TEXT("Multi-decision play accepted"), Fixture.Battle->RequestPlayCard(Played, nullptr).IsAcceptedForResolution());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	FPendingCardSelectionReadView View;
	if (!TestTrue(TEXT("Exhaust choice pending"), BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Fixture.Battle, View))) return false;
	TestTrue(TEXT("First choice resolves"), BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, { View.CandidateRuntimeIds[0] }));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	if (!TestTrue(TEXT("Top choice pending"), BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Fixture.Battle, View))) return false;
	TestTrue(TEXT("Second choice resolves"), BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, { View.CandidateRuntimeIds[0] }));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	if (!TestEqual(TEXT("Two boundaries plus final cleanup form three envelopes"), Envelopes.Num(), 3)) return false;
	TestTrue(TEXT("Decision revisions strictly advance"), Envelopes[1].FinalStateRevision > Envelopes[0].FinalStateRevision);
	TestTrue(TEXT("Continuation segments have increasing identities"), Envelopes[0].ResolutionId < Envelopes[1].ResolutionId && Envelopes[1].ResolutionId < Envelopes[2].ResolutionId);
	TestTrue(TEXT("Final segment retains played-card cleanup"), Envelopes[2].Records.ContainsByPredicate([PlayedId](const FPresentationRecord& Record)
	{
		return Record.Type == EBattlePresentationRecordType::CardZoneChanged
			&& Record.CardZoneChanged.Card.RuntimeId == PlayedId
			&& Record.CardZoneChanged.FromZone == ECardZone::PlayArea
			&& Record.CardZoneChanged.ToZone == ECardZone::DiscardPile;
	}));
	TestFalse(TEXT("Multi-boundary chain completes without fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedCandidateFailureDispositionTest,
	"SlayTheSpireDemo.CardSelection.Unified.Failure.EmptyVersusBrokenSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedCandidateFailureDispositionTest::RunTest(const FString& Parameters)
{
	for (bool bBroken : { false, true })
	{
		FFixture Fixture;
		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
		USelectionResolver* Resolver = NewObject<USelectionResolver>(Fixture.World);
		Resolver->Initialize(FSelectionResolverQueueAccess::CreateLambda([Queue](const USelectionResolver*) { return Queue; }));
		UCurrentHandSelectionSource* Source = NewObject<UCurrentHandSelectionSource>(Queue);
		if (!bBroken) Source->Initialize(NewObject<UDeckRuntime>(Fixture.World));
		UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Queue);
		Continuation->Configure(nullptr, TEXT("Empty"), 0);
		UDeferredSelectionAction* Action = NewObject<UDeferredSelectionAction>(Queue);
		Action->Initialize(Source, Resolver, Continuation, 1, ESelectionCancelPolicy::Forbidden, TEXT("EmptyOrBroken"));
		int32 TailRuns = 0;
		UWave1CTestMarkerAction* Tail = NewObject<UWave1CTestMarkerAction>(Queue);
		Tail->Initialize(&TailRuns, TEXT("Tail"));
		Queue->AddToBack(Action);
		Queue->AddToBack(Tail);
		if (bBroken)
		{
			AddExpectedError(TEXT("Resolution fault requested:"), EAutomationExpectedErrorFlags::Contains, 1);
			AddExpectedError(TEXT("Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
		}
		Queue->StartProcessing();
		TestEqual(TEXT("Only broken source faults"), Queue->IsResolutionFaulted(), bBroken);
		TestEqual(TEXT("Only legal empty choice continues tail"), TailRuns, bBroken ? 0 : 1);
		TestFalse(TEXT("No phantom pending request"), Resolver->HasPendingSelection());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
