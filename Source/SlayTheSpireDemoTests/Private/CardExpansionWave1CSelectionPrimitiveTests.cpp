#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CardExpansionWave1CTestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/SelectionRequestAction.h"
#include "Actions/RandomSelectionAction.h"
#include "Selection/SelectionResolver.h"
#include "Selection/ExhaustSelectedContinuation.h"
#include "Engine/World.h"

namespace CardExpansionWave1CTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		UBattleActionQueue* Queue = nullptr;
		USelectionResolver* Resolver = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			Queue = NewObject<UBattleActionQueue>(World);
			Resolver = NewObject<USelectionResolver>(World);
			Resolver->Initialize(
				FSelectionResolverQueueAccess::CreateLambda(
					[this](const USelectionResolver*) -> UBattleActionQueue*
					{
						return Queue;
					}
				)
			);
		}

		~FFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		FSelectionRequest BuildRequest(int32 CandidateCount, int32 MinCount, int32 MaxCount)
		{
			FSelectionRequest Request;
			Request.SelectionSource = TEXT("Wave1C.Primitive");
			Request.MinCount = MinCount;
			Request.MaxCount = MaxCount;
			for (int32 Index = 0; Index < CandidateCount; ++Index)
			{
				FSelectionCandidate Candidate;
				Candidate.RuntimeObject = NewObject<UWave1CTestCandidateObject>(World);
				Candidate.RuntimeSequence = Index;
				Candidate.SelectionKey = FName(*FString::Printf(TEXT("Candidate%d"), Index));
				Request.Candidates.Add(Candidate);
			}
			return Request;
		}
	};
}

using namespace CardExpansionWave1CTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectionPrimitiveResolveTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.Selection.ResolveBuildsContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectionPrimitiveResolveTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("Queue exists"), Fixture.Queue)
		|| !TestNotNull(TEXT("Resolver exists"), Fixture.Resolver))
	{
		return false;
	}

	FSelectionRequest Request = Fixture.BuildRequest(3, 1, 1);
	int32 ExecutionCounter = 0;
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	Continuation->Configure(&ExecutionCounter, TEXT("Dependent"), 2);

	USelectionRequestAction* Action = NewObject<USelectionRequestAction>(Fixture.Queue);
	Action->Initialize(Fixture.Resolver, Request, Continuation);
	if (!TestTrue(TEXT("Selection Action enqueued"), Fixture.Queue->AddToBack(Action))
		|| !TestTrue(TEXT("Queue starts processing"), Fixture.Queue->StartProcessing()))
	{
		return false;
	}

	TestFalse(TEXT("Selection Action finished during Execute"), Action->IsFinished());
	TestTrue(TEXT("Action awaits selection"), Action->IsAwaitingSelection());
	TestTrue(TEXT("Resolver has a pending selection"), Fixture.Resolver->HasPendingSelection());
	TestNotNull(TEXT("Pending request is exposed"), Fixture.Resolver->GetPendingRequest());
	TestTrue(TEXT("Queue stays busy while awaiting selection"), Fixture.Queue->IsBusy());
	TestEqual(TEXT("No dependent marker ran before resolve"), ExecutionCounter, 0);

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(Request.Candidates[1].RuntimeObject);
	Action->ResolvePendingSelection(Result);

	TestTrue(TEXT("Action finished after resolve"), Action->IsFinished());
	TestFalse(TEXT("Resolver cleared pending selection"), Fixture.Resolver->HasPendingSelection());
	TestEqual(TEXT("Continuation markers executed"), ExecutionCounter, 2);
	TestFalse(TEXT("No resolution fault"), Fixture.Queue->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectionPrimitiveInvalidRejectTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.Selection.InvalidSelectionRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectionPrimitiveInvalidRejectTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("Queue exists"), Fixture.Queue)
		|| !TestNotNull(TEXT("Resolver exists"), Fixture.Resolver))
	{
		return false;
	}

	FSelectionRequest Request = Fixture.BuildRequest(3, 1, 1);
	int32 ExecutionCounter = 0;
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	Continuation->Configure(&ExecutionCounter, TEXT("Dependent"), 1);

	USelectionRequestAction* Action = NewObject<USelectionRequestAction>(Fixture.Queue);
	Action->Initialize(Fixture.Resolver, Request, Continuation);
	Fixture.Queue->AddToBack(Action);
	Fixture.Queue->StartProcessing();

	// A result whose object is not in the candidate set must be rejected.
	UWave1CTestCandidateObject* ForeignObject = NewObject<UWave1CTestCandidateObject>(Fixture.World);
	FSelectionResult InvalidResult;
	InvalidResult.Status = ESelectionStatus::Resolved;
	InvalidResult.SelectedObjects.Add(ForeignObject);
	Action->ResolvePendingSelection(InvalidResult);

	TestFalse(TEXT("Invalid selection keeps the Action awaiting input"), Action->IsFinished());
	TestTrue(TEXT("Invalid selection keeps pending state"), Fixture.Resolver->HasPendingSelection());
	TestEqual(TEXT("Invalid selection produced no markers"), ExecutionCounter, 0);
	TestFalse(TEXT("Invalid selection does not fault resolution"), Fixture.Queue->IsResolutionFaulted());

	FSelectionResult ValidResult;
	ValidResult.Status = ESelectionStatus::Resolved;
	ValidResult.SelectedObjects.Add(Request.Candidates[0].RuntimeObject);
	Action->ResolvePendingSelection(ValidResult);
	TestTrue(TEXT("Valid resubmission resolves after invalid input"), Action->IsFinished());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectionPrimitiveCancelTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.Selection.CancelIsLegalPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectionPrimitiveCancelTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("Queue exists"), Fixture.Queue)
		|| !TestNotNull(TEXT("Resolver exists"), Fixture.Resolver))
	{
		return false;
	}

	FSelectionRequest Request = Fixture.BuildRequest(2, 1, 1);
	int32 ExecutionCounter = 0;
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	Continuation->Configure(&ExecutionCounter, TEXT("Dependent"), 1);

	USelectionRequestAction* Action = NewObject<USelectionRequestAction>(Fixture.Queue);
	Action->Initialize(Fixture.Resolver, Request, Continuation);
	Fixture.Queue->AddToBack(Action);
	Fixture.Queue->StartProcessing();

	Action->CancelPendingSelection();

	TestTrue(TEXT("Cancel finishes the Action"), Action->IsFinished());
	TestFalse(TEXT("Cancel clears pending state"), Fixture.Resolver->HasPendingSelection());
	TestEqual(TEXT("Cancel produced no dependent markers"), ExecutionCounter, 0);
	TestFalse(TEXT("Cancel does not fault resolution"), Fixture.Queue->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectionPrimitiveRequestValidationTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.Selection.MalformedRequestRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectionPrimitiveRequestValidationTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("Resolver exists"), Fixture.Resolver))
	{
		return false;
	}

	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	Continuation->Configure(nullptr, TEXT("Dependent"), 1);
	USelectionRequestAction* DummyAction = NewObject<USelectionRequestAction>(Fixture.World);

	FSelectionRequest EmptyRequest = Fixture.BuildRequest(0, 1, 1);
	TestFalse(TEXT("Empty candidate set rejected"), Fixture.Resolver->BeginSelection(EmptyRequest, Continuation, DummyAction));
	TestFalse(TEXT("No pending selection after empty request"), Fixture.Resolver->HasPendingSelection());

	FSelectionRequest InvertedBounds = Fixture.BuildRequest(2, 2, 1);
	TestFalse(TEXT("Inverted min/max bounds rejected"), Fixture.Resolver->BeginSelection(InvertedBounds, Continuation, DummyAction));
	TestFalse(TEXT("No pending selection after inverted bounds"), Fixture.Resolver->HasPendingSelection());

	FSelectionRequest OutOfRange = Fixture.BuildRequest(2, 1, 3);
	TestFalse(TEXT("Max beyond candidate count rejected"), Fixture.Resolver->BeginSelection(OutOfRange, Continuation, DummyAction));
	TestFalse(TEXT("No pending selection after out-of-range bounds"), Fixture.Resolver->HasPendingSelection());

	FSelectionRequest Valid = Fixture.BuildRequest(2, 1, 1);
	TestTrue(TEXT("Valid request accepted"), Fixture.Resolver->BeginSelection(Valid, Continuation, DummyAction));
	TestTrue(TEXT("Pending selection set"), Fixture.Resolver->HasPendingSelection());

	FSelectionRequest Second = Fixture.BuildRequest(2, 1, 1);
	TestFalse(TEXT("Second concurrent request rejected"), Fixture.Resolver->BeginSelection(Second, Continuation, DummyAction));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectionPrimitiveCountBoundsTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.Selection.CountBoundsEnforced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectionPrimitiveCountBoundsTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("Queue exists"), Fixture.Queue)
		|| !TestNotNull(TEXT("Resolver exists"), Fixture.Resolver))
	{
		return false;
	}

	FSelectionRequest Request = Fixture.BuildRequest(3, 2, 2);
	int32 ExecutionCounter = 0;
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	Continuation->Configure(&ExecutionCounter, TEXT("Dependent"), 1);

	USelectionRequestAction* Action = NewObject<USelectionRequestAction>(Fixture.Queue);
	Action->Initialize(Fixture.Resolver, Request, Continuation);
	Fixture.Queue->AddToBack(Action);
	Fixture.Queue->StartProcessing();

	// Too few selections: outside [2,2].
	FSelectionResult TooFew;
	TooFew.Status = ESelectionStatus::Resolved;
	TooFew.SelectedObjects.Add(Request.Candidates[0].RuntimeObject);
	Action->ResolvePendingSelection(TooFew);
	TestEqual(TEXT("Too-few count rejected (no markers)"), ExecutionCounter, 0);

	// Valid count of 2 on the same still-pending Action.
	FSelectionResult Valid;
	Valid.Status = ESelectionStatus::Resolved;
	Valid.SelectedObjects.Add(Request.Candidates[0].RuntimeObject);
	Valid.SelectedObjects.Add(Request.Candidates[2].RuntimeObject);
	Action->ResolvePendingSelection(Valid);
	TestEqual(TEXT("Valid count of 2 resolves"), ExecutionCounter, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectionPrimitiveBeginFailureFaultTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.Selection.BeginFailureRequestsFault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectionPrimitiveBeginFailureFaultTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("Queue exists"), Fixture.Queue)
		|| !TestNotNull(TEXT("Resolver exists"), Fixture.Resolver))
	{
		return false;
	}

	FSelectionRequest EmptyRequest = Fixture.BuildRequest(0, 1, 1);
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	Continuation->Configure(nullptr, TEXT("BeginFailure"), 1);
	USelectionRequestAction* Action = NewObject<USelectionRequestAction>(Fixture.Queue);
	Action->Initialize(Fixture.Resolver, EmptyRequest, Continuation);

	TestTrue(TEXT("Malformed selection Action enqueues"), Fixture.Queue->AddToBack(Action));
	AddExpectedError(TEXT("Resolution fault requested:"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("Malformed selection Action starts"), Fixture.Queue->StartProcessing());
	TestTrue(TEXT("Begin failure enters Queue resolution fault"), Fixture.Queue->IsResolutionFaulted());
	TestTrue(TEXT("Malformed selection Action finishes"), Action->IsFinished());
	TestFalse(TEXT("Begin failure leaves no pending selection"), Fixture.Resolver->HasPendingSelection());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectionPrimitiveForbiddenCancelTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.Selection.ForbiddenCancelRetainsPending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectionPrimitiveForbiddenCancelTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("Queue exists"), Fixture.Queue)
		|| !TestNotNull(TEXT("Resolver exists"), Fixture.Resolver))
	{
		return false;
	}

	FSelectionRequest Request = Fixture.BuildRequest(2, 1, 1);
	Request.CancelPolicy = ESelectionCancelPolicy::Forbidden;
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	Continuation->Configure(nullptr, TEXT("ForbiddenCancel"), 1);
	USelectionRequestAction* Action = NewObject<USelectionRequestAction>(Fixture.Queue);
	Action->Initialize(Fixture.Resolver, Request, Continuation);

	TestTrue(TEXT("Mandatory selection Action enqueues"), Fixture.Queue->AddToBack(Action));
	TestTrue(TEXT("Mandatory selection Action starts"), Fixture.Queue->StartProcessing());
	Action->CancelPendingSelection();
	TestFalse(TEXT("Forbidden cancellation does not finish Action"), Action->IsFinished());
	TestTrue(TEXT("Forbidden cancellation retains pending request"), Fixture.Resolver->HasPendingSelection());
	TestFalse(TEXT("Forbidden cancellation does not fault Queue"), Fixture.Queue->IsResolutionFaulted());

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(Request.Candidates[0].RuntimeObject);
	Action->ResolvePendingSelection(Result);
	TestTrue(TEXT("Valid result remains able to resolve"), Action->IsFinished());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedSelectionInternalFailureTest,
	"SlayTheSpireDemo.CardSelection.Unified.Failure.PendingDependencyAndContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedSelectionInternalFailureTest::RunTest(const FString& Parameters)
{
	for (bool bInvalidateCandidate : { false, true })
	{
		FFixture Fixture;
		FSelectionRequest Request = Fixture.BuildRequest(1, 1, 1);
		Request.CancelPolicy = ESelectionCancelPolicy::Forbidden;
		// Uninitialized Deck intentionally makes an otherwise valid result fail
		// in the authored continuation, independently of input validation.
		UExhaustSelectedContinuation* Continuation = NewObject<UExhaustSelectedContinuation>(Fixture.World);
		USelectionRequestAction* Action = NewObject<USelectionRequestAction>(Fixture.Queue);
		Action->Initialize(Fixture.Resolver, Request, Continuation);
		Fixture.Queue->AddToBack(Action);
		Fixture.Queue->StartProcessing();
		FSelectionResult Result;
		Result.Status = ESelectionStatus::Resolved;
		Result.SelectedObjects.Add(Request.Candidates[0].RuntimeObject);
		if (bInvalidateCandidate) Request.Candidates[0].RuntimeObject->MarkAsGarbage();
		AddExpectedError(bInvalidateCandidate ? TEXT("Pending runtime dependency failed:") : TEXT("Continuation failed to build dependent actions."), EAutomationExpectedErrorFlags::Contains, 1);
		AddExpectedError(TEXT("Resolution fault requested:"), EAutomationExpectedErrorFlags::Contains, 1);
		AddExpectedError(TEXT("Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("Internal failure is not an accepted result"), Fixture.Resolver->SubmitResult(Result));
		TestTrue(TEXT("Internal failure faults queue"), Fixture.Queue->IsResolutionFaulted());
		TestFalse(TEXT("Internal failure releases pending request"), Fixture.Resolver->HasPendingSelection());
		TestTrue(TEXT("Internal failure finishes awaiting Action"), Action->IsFinished());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedRandomAllSelectedNoRngTest,
	"SlayTheSpireDemo.CardSelection.Unified.Random.AllSelectedConsumesNoRng",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedRandomAllSelectedNoRngTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	FSelectionRequest Request = Fixture.BuildRequest(3, 3, 3);
	int32 RngCalls = 0;
	int32 Completed = 0;
	FSelectionRandomIndexChooser Chooser = FSelectionRandomIndexChooser::CreateLambda(
		[&RngCalls](int32 Count, int32& Index) { ++RngCalls; Index = 0; return true; });
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	Continuation->Configure(&Completed, TEXT("AllSelected"), 1);
	URandomSelectionAction* Action = NewObject<URandomSelectionAction>(Fixture.Queue);
	Action->Initialize(Request, Continuation, Chooser);
	Fixture.Queue->AddToBack(Action);
	Fixture.Queue->StartProcessing();
	TestEqual(TEXT("All selected consumes no RNG"), RngCalls, 0);
	TestEqual(TEXT("Continuation executes once"), Completed, 1);
	TestFalse(TEXT("Random does not create pending choice"), Fixture.Resolver->HasPendingSelection());
	TestFalse(TEXT("Random succeeds without fault"), Fixture.Queue->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnifiedDuplicateBeginFaultTest,
	"SlayTheSpireDemo.CardSelection.Unified.Failure.DuplicateBeginReleasesCurrentAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnifiedDuplicateBeginFaultTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	FSelectionRequest Request = Fixture.BuildRequest(1, 1, 1);
	UWave1CTestContinuation* Continuation = NewObject<UWave1CTestContinuation>(Fixture.World);
	Continuation->Configure(nullptr, TEXT("Duplicate"), 0);
	USelectionRequestAction* First = NewObject<USelectionRequestAction>(Fixture.Queue);
	First->Initialize(Fixture.Resolver, Request, Continuation);
	Fixture.Queue->AddToBack(First);
	Fixture.Queue->StartProcessing();
	USelectionRequestAction* Unexpected = NewObject<USelectionRequestAction>(Fixture.Queue);
	Unexpected->Initialize(Fixture.Resolver, Request, Continuation);
	AddExpectedError(TEXT("Resolution fault requested:"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
	// Deliberate invalid dispatch exercises defensive framework cleanup.
	Unexpected->Execute(Fixture.Queue);
	TestTrue(TEXT("Duplicate dispatch reaches fault safe point"), Fixture.Queue->IsResolutionFaulted());
	TestTrue(TEXT("Old awaiting Action finishes"), First->IsFinished());
	TestFalse(TEXT("Old Action no longer holds current ownership"), Fixture.Queue->IsCurrentAction(First));
	TestFalse(TEXT("No abandoned pending request remains"), Fixture.Resolver->HasPendingSelection());
	return true;
}

#endif
