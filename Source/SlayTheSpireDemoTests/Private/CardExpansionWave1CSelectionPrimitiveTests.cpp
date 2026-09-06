#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CardExpansionWave1CTestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/SelectionRequestAction.h"
#include "Selection/SelectionResolver.h"
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

	TestTrue(TEXT("Invalid selection still finishes the Action"), Action->IsFinished());
	TestFalse(TEXT("Invalid selection clears pending state"), Fixture.Resolver->HasPendingSelection());
	TestEqual(TEXT("Invalid selection produced no markers"), ExecutionCounter, 0);
	TestFalse(TEXT("Invalid selection does not fault resolution"), Fixture.Queue->IsResolutionFaulted());
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

	FSelectionRequest EmptyRequest = Fixture.BuildRequest(0, 1, 1);
	TestFalse(TEXT("Empty candidate set rejected"), Fixture.Resolver->BeginSelection(EmptyRequest, Continuation));
	TestFalse(TEXT("No pending selection after empty request"), Fixture.Resolver->HasPendingSelection());

	FSelectionRequest InvertedBounds = Fixture.BuildRequest(2, 2, 1);
	TestFalse(TEXT("Inverted min/max bounds rejected"), Fixture.Resolver->BeginSelection(InvertedBounds, Continuation));
	TestFalse(TEXT("No pending selection after inverted bounds"), Fixture.Resolver->HasPendingSelection());

	FSelectionRequest OutOfRange = Fixture.BuildRequest(2, 1, 3);
	TestFalse(TEXT("Max beyond candidate count rejected"), Fixture.Resolver->BeginSelection(OutOfRange, Continuation));
	TestFalse(TEXT("No pending selection after out-of-range bounds"), Fixture.Resolver->HasPendingSelection());

	FSelectionRequest Valid = Fixture.BuildRequest(2, 1, 1);
	TestTrue(TEXT("Valid request accepted"), Fixture.Resolver->BeginSelection(Valid, Continuation));
	TestTrue(TEXT("Pending selection set"), Fixture.Resolver->HasPendingSelection());

	FSelectionRequest Second = Fixture.BuildRequest(2, 1, 1);
	TestFalse(TEXT("Second concurrent request rejected"), Fixture.Resolver->BeginSelection(Second, Continuation));
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

	// Valid count of 2.
	USelectionRequestAction* Second = NewObject<USelectionRequestAction>(Fixture.Queue);
	Second->Initialize(Fixture.Resolver, Request, Continuation);
	Second->SetPresentationRecordWriter(Action->GetPresentationRecordWriter());
	Fixture.Queue->AddToBack(Second);
	Fixture.Queue->StartProcessing();

	FSelectionResult Valid;
	Valid.Status = ESelectionStatus::Resolved;
	Valid.SelectedObjects.Add(Request.Candidates[0].RuntimeObject);
	Valid.SelectedObjects.Add(Request.Candidates[2].RuntimeObject);
	Second->ResolvePendingSelection(Valid);
	TestEqual(TEXT("Valid count of 2 resolves"), ExecutionCounter, 1);
	return true;
}

#endif
