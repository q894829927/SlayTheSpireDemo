#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/DeferredSelectionAction.h"
#include "CardExpansionWave1CTestTypes.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Deck/DeckRuntime.h"
#include "Selection/SelectionCandidateSource.h"
#include "Selection/SelectionResolver.h"

namespace UnifiedCardSelectionBoundaryContractTest
{
	UCardData* MakeCard(UObject* Outer, const TCHAR* CardId)
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

	USelectionResolver* MakeResolver(UObject* Outer, UBattleActionQueue* Queue)
	{
		USelectionResolver* Resolver = NewObject<USelectionResolver>(Outer);
		Resolver->Initialize(FSelectionResolverQueueAccess::CreateLambda(
			[Queue](const USelectionResolver*)
			{
				return Queue;
			}));
		return Resolver;
	}
}

using namespace UnifiedCardSelectionBoundaryContractTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedPlayerSelectionRequiresInteractiveBoundaryTest,
	"SlayTheSpireDemo.CardSelection.Unified.Contract.PlayerRequiresInteractiveBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedPlayerSelectionRequiresInteractiveBoundaryTest::RunTest(const FString& Parameters)
{
	UCardData* Card = MakeCard(GetTransientPackage(), TEXT("BoundaryRequiredCandidate"));
	TArray<TObjectPtr<UCardData>> Definitions{ Card };
	UDeckRuntime* Deck = NewObject<UDeckRuntime>(GetTransientPackage());
	Deck->InitializeFromDefinitions(Definitions, 7101);
	UCardInstance* HandCard = nullptr;
	if (!TestTrue(TEXT("Setup draw creates one current-Hand candidate"), Deck->TryDrawTopCardCommit(HandCard).bCommitted)
		|| !TestNotNull(TEXT("Current-Hand candidate exists"), HandCard))
	{
		return false;
	}

	UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(GetTransientPackage());
	USelectionResolver* Resolver = MakeResolver(GetTransientPackage(), Queue);
	UCurrentHandSelectionSource* Source = NewObject<UCurrentHandSelectionSource>(Queue);
	Source->Initialize(Deck);
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Queue);
	Continuation->Configure(nullptr, TEXT("BoundaryRequired"), 0);

	UDeferredSelectionAction* Action = NewObject<UDeferredSelectionAction>(Queue);
	Action->Initialize(
		Source,
		Resolver,
		Continuation,
		1,
		ESelectionCancelPolicy::Forbidden,
		TEXT("BoundaryRequired"),
		EDeferredSelectionMode::Player,
		FSelectionInteractiveBoundaryAccess{});

	TestTrue(TEXT("Boundary-contract Action enqueues"), Queue->AddToBack(Action));
	AddExpectedError(
		TEXT("Resolution fault requested: DeferredSelection Player mode requires a Resolver and interactive Presentation boundary."),
		EAutomationExpectedErrorFlags::Contains,
		1);
	AddExpectedError(
		TEXT("Resolution faulted. Reason=DeferredSelection Player mode requires a Resolver and interactive Presentation boundary."),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestTrue(TEXT("Boundary-contract queue starts"), Queue->StartProcessing());
	TestTrue(TEXT("Non-empty Player selection without boundary faults"), Queue->IsResolutionFaulted());
	TestFalse(TEXT("Missing boundary never creates pending Selection"), Resolver->HasPendingSelection());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedEmptyPlayerSelectionNeedsNoBoundaryTest,
	"SlayTheSpireDemo.CardSelection.Unified.Contract.EmptyPlayerSelectionNeedsNoBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedEmptyPlayerSelectionNeedsNoBoundaryTest::RunTest(const FString& Parameters)
{
	UDeckRuntime* Deck = NewObject<UDeckRuntime>(GetTransientPackage());
	UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(GetTransientPackage());
	USelectionResolver* Resolver = MakeResolver(GetTransientPackage(), Queue);
	UCurrentHandSelectionSource* Source = NewObject<UCurrentHandSelectionSource>(Queue);
	Source->Initialize(Deck);
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Queue);
	Continuation->Configure(nullptr, TEXT("EmptyBoundary"), 0);

	UDeferredSelectionAction* Action = NewObject<UDeferredSelectionAction>(Queue);
	Action->Initialize(
		Source,
		Resolver,
		Continuation,
		1,
		ESelectionCancelPolicy::Forbidden,
		TEXT("EmptyBoundary"),
		EDeferredSelectionMode::Player,
		FSelectionInteractiveBoundaryAccess{});

	TestTrue(TEXT("Empty Player selection Action enqueues"), Queue->AddToBack(Action));
	TestTrue(TEXT("Empty Player selection queue starts"), Queue->StartProcessing());
	TestTrue(TEXT("Empty Player selection finishes as no-op"), Action->IsFinished());
	TestFalse(TEXT("Empty Player selection does not fault"), Queue->IsResolutionFaulted());
	TestFalse(TEXT("Empty Player selection creates no pending request"), Resolver->HasPendingSelection());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
