#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interior/InteriorPortalPassageCoordinator.h"
#include "Interior/InteriorPortalMath.h"

namespace
{
	using namespace InteriorPortalPhysics;
	constexpr EAutomationTestFlags CoordinatorTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	struct FCoordinatorFixture
	{
		FBoundaryCommand C;
		FBoundaryState Start;
		FBodyCommitState Solved;
		FPhysicsStepKey Step{7,10,1,1./120};
		FCoordinatorFixture()
		{
			C.Traveller.Handle = {1,2,3}; C.PairGeneration = 4; C.Revision = 5;
			FPrimitive Sphere; Sphere.Radius = 2; C.Traveller.Geometry.Primitives.Add(Sphere);
			C.PermitSupportBypass = C.IsolatedStaticScope = C.PermitTransfer = C.ExitCorridorCertified = true;
			C.Exit = FTransform(FVector(1000,0,0)); C.SupportHalfThicknessCm = 2;
			Start.Pose.SetLocation(FVector(.5,0,0)); Start.LinearVelocity = FVector(-120,0,0);
			Solved.Motion = Start; Solved.Motion.Pose.SetLocation(FVector(-.5,0,0));
			Solved.MassKg = 1; Solved.LocalInertia = FVector(10);
		}
		FPassageCoordinator Coordinator(int32 Capacity = 32) const
		{ return FPassageCoordinator(C.Traveller.Handle,4,7,11,C.Entry,C.Exit,0,Capacity); }
	};
	struct FRecordingAdapter : IPortalTransferAdapter
	{
		bool Succeeds = true;
		int32 Writes = 0;
		FTransferFact Last;
		virtual bool Commit(const FTransferFact& Fact) override
		{ if (!Succeeds) { return false; } ++Writes; Last = Fact; return true; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalCoordinatorAtomicTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsCoordinator.AtomicTransfer", CoordinatorTestFlags)
bool FPortalCoordinatorAtomicTest::RunTest(const FString& Parameters)
{
	FCoordinatorFixture F; auto C = F.Coordinator(); FRecordingAdapter Adapter;
	TestTrue(TEXT("One shared owner authorizes the bounded crossing"), C.EvaluateInterval(F.C,F.Start,F.Solved.Motion,F.Step,0,false).TransferAfterSolve);
	Adapter.Succeeds = false;
	TestTrue(TEXT("Failed adapter leaves endpoint, revision and journal unchanged"), C.CommitSolved(F.C,F.Solved,F.Step,Adapter) == ETransferResult::AdapterRejected
		&& Adapter.Writes == 0 && C.BodyEndpoint() == 0 && C.TransferRevision() == 0 && C.PendingFacts().IsEmpty());
	Adapter.Succeeds = true; ++F.Step.SolverFrame; ++F.Step.EvolutionSerial;
	C.EvaluateInterval(F.C,F.Start,F.Solved.Motion,F.Step,0,false);
	TestTrue(TEXT("Successful transaction has one write, one fact and an exiting phase"), C.CommitSolved(F.C,F.Solved,F.Step,Adapter) == ETransferResult::Committed
		&& Adapter.Writes == 1 && C.TransferRevision() == 1 && C.BodyEndpoint() == 1 && C.Phase() == EPassagePhase::Exiting);
	TestTrue(TEXT("Repeated commit in same interval cannot write twice"), C.CommitSolved(F.C,F.Solved,F.Step,Adapter) == ETransferResult::AlreadyCommitted && Adapter.Writes == 1);
	TestTrue(TEXT("Mapped pose and momentum derive from actual solved state"), Adapter.Last.After.Motion.Pose.GetLocation().Equals(
		InteriorPortalMath::Position(F.Solved.Motion.Pose.GetLocation(),F.C.Entry,F.C.Exit),1.e-6)
		&& Adapter.Last.After.Motion.LinearVelocity.Equals(InteriorPortalMath::Rotation(F.C.Entry,F.C.Exit).RotateVector(F.Solved.Motion.LinearVelocity),1.e-6));
	TestTrue(TEXT("Material and local mass frame are unchanged"), Adapter.Last.After.MassKg == F.Solved.MassKg
		&& Adapter.Last.After.LocalInertia == F.Solved.LocalInertia && Adapter.Last.After.LocalCOM == F.Solved.LocalCOM);
	C.Retire(); C.Retire();
	TestTrue(TEXT("Retirement is permanent but retains committed facts"), C.PendingFacts().Num() == 1
		&& !C.EvaluateInterval(F.C,F.Start,F.Solved.Motion,F.Step,0,false).BypassSupport
		&& C.CommitSolved(F.C,F.Solved,F.Step,Adapter) == ETransferResult::Retired);
	TestFalse(TEXT("Retired binding cannot acquire a hold"), C.AcquireHold(1,{1,8,1},0,FTransform::Identity,FVector::ZeroVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalCoordinatorRejectionTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsCoordinator.RejectionAndCOM", CoordinatorTestFlags)
bool FPortalCoordinatorRejectionTest::RunTest(const FString& Parameters)
{
	FCoordinatorFixture F; FRecordingAdapter Adapter;
	for (int32 Case = 0; Case < 7; ++Case)
	{
		auto C = F.Coordinator(); auto Command = F.C; auto Step = F.Step;
		if (Case == 0) { ++Command.Traveller.Handle.Generation; }
		if (Case == 1) { ++Command.PairGeneration; }
		if (Case == 2) { ++Step.SolverEpoch; }
		if (Case == 3) { Command.Entry.AddToTranslation(FVector(1,0,0)); }
		if (Case == 4) { Command.ExitCorridorCertified = false; }
		if (Case == 5) { Command.MaxTranslationCm = .1; }
		if (Case == 6) { Command.Entry.SetScale3D(FVector(2)); }
		TestFalse(TEXT("Invalid identity/frame/certificate/motion cannot authorize"), C.EvaluateInterval(Command,F.Start,F.Solved.Motion,Step,0,false).BypassSupport);
		TestTrue(TEXT("Rejected interval cannot create a physical write or fact"), C.CommitSolved(Command,F.Solved,Step,Adapter) != ETransferResult::Committed && C.PendingFacts().IsEmpty());
	}
	auto Invalid = F.Coordinator(); Invalid.EvaluateInterval(F.C,F.Start,F.Solved.Motion,F.Step,0,false);
	auto BadState = F.Solved; BadState.MassKg = 0;
	TestTrue(TEXT("Invalid material fails before any adapter write"), Invalid.CommitSolved(F.C,BadState,F.Step,Adapter) == ETransferResult::Rejected && Adapter.Writes == 0);
	auto Changed = F.Coordinator(); Changed.EvaluateInterval(F.C,F.Start,F.Solved.Motion,F.Step,0,false);
	auto ChangedCommand = F.C; ChangedCommand.HalfWidth *= 2;
	TestTrue(TEXT("Reusing a revision cannot mutate the solved interval contract"), Changed.CommitSolved(ChangedCommand,F.Solved,F.Step,Adapter) == ETransferResult::Rejected);
	F.C.LocalAuthorityReference = FVector(1,0,0); F.Solved.LocalCOM = F.C.LocalAuthorityReference;
	F.Start.Pose.SetLocation(FVector(-.5,0,0)); F.Solved.Motion.Pose.SetLocation(FVector(-1.5,0,0));
	auto COM = F.Coordinator();
	TestTrue(TEXT("COM reference can cross while root was already behind plane"),COM.EvaluateInterval(F.C,F.Start,F.Solved.Motion,F.Step,0,false).TransferAfterSolve
		&& COM.CommitSolved(F.C,F.Solved,F.Step,Adapter) == ETransferResult::Committed);
	TestTrue(TEXT("Mapped COM equals transformed original COM without changing local offset"), Adapter.Last.After.Motion.Pose.TransformPositionNoScale(F.Solved.LocalCOM).Equals(
		InteriorPortalMath::Position(F.Solved.Motion.Pose.TransformPositionNoScale(F.Solved.LocalCOM),F.C.Entry,F.C.Exit),1.e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalCoordinatorRouteTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsCoordinator.HolderBodyRelation", CoordinatorTestFlags)
bool FPortalCoordinatorRouteTest::RunTest(const FString& Parameters)
{
	const FTravellerHandle Holder{1,8,1}; const FTransform Desired(FVector(-40,20,0)); const FVector Anchor(0,1,0);
	for (bool HolderFirst : {false,true})
	{
		FCoordinatorFixture F; auto C = F.Coordinator(); FRecordingAdapter Adapter;
		TestTrue(TEXT("Explicit association acquired once"), C.AcquireHold(1,Holder,0,Desired,Anchor));
		if (HolderFirst) { TestTrue(TEXT("Holder-first event updates relation"),C.HolderCrossed(1,Holder,1,0)); }
		C.EvaluateInterval(F.C,F.Start,F.Solved.Motion,F.Step,0,false);
		TestTrue(TEXT("Held transfer uses the same free-body commit"),C.CommitSolved(F.C,F.Solved,F.Step,Adapter) == ETransferResult::Committed);
		if (!HolderFirst)
		{
			TestTrue(TEXT("Body-first route is explicit remote mapping"),C.Hold().Route.Kind == EHoldRoute::SinglePair);
			TestTrue(TEXT("Subsequent holder crossing reconciles same association"),C.HolderCrossed(1,Holder,1,0));
		}
		const auto H = C.Hold();
		TestTrue(TEXT("Both ordering cases become direct with mapped target and unchanged local anchor"),H.Active && H.Route.Kind == EHoldRoute::Direct
			&& H.LocalAnchor == Anchor && H.DesiredHolderPose.GetLocation().Equals(InteriorPortalMath::Position(Desired.GetLocation(),F.C.Entry,F.C.Exit),1.e-6));
		TestTrue(TEXT("Repeated holder event does not advance relation twice"),C.HolderCrossed(1,Holder,1,0) && C.Hold().Route.Revision == H.Route.Revision);
		TestFalse(TEXT("Old-space intent cannot replace post-crossing target"),C.SetHolderIntent(1,Holder,0,Desired));
		TestTrue(TEXT("Current-space intent accepted"),C.SetHolderIntent(1,Holder,1,H.DesiredHolderPose));
		TestTrue(TEXT("Release is idempotent"),C.ReleaseHold(1) && C.ReleaseHold(1));
		TestFalse(TEXT("Released instance cannot be acquired again"),C.AcquireHold(1,Holder,1,Desired,Anchor));
		TestTrue(TEXT("A new explicit instance may acquire"),C.AcquireHold(2,Holder,1,Desired,Anchor));
		TestFalse(TEXT("Wrong holder generation cannot update relation"),C.SetHolderIntent(2,{1,8,2},0,Desired));
		TestFalse(TEXT("Unsupported crossing sequence releases rather than guessing"),C.HolderCrossed(2,Holder,2,1));
		TestFalse(TEXT("Unsupported hold remains released"),C.Hold().Active);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalCoordinatorJournalTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsCoordinator.DurableFacts", CoordinatorTestFlags)
bool FPortalCoordinatorJournalTest::RunTest(const FString& Parameters)
{
	FCoordinatorFixture F; auto C = F.Coordinator(1); FRecordingAdapter Adapter;
	C.EvaluateInterval(F.C,F.Start,F.Solved.Motion,F.Step,0,false); C.CommitSolved(F.C,F.Solved,F.Step,Adapter);
	const FTransferFact First = C.PendingFacts()[0];
	FTransferFactCursor Cursor(F.C.Traveller.Handle,7,11);
	auto Gap = First; Gap.Revision = 2;
	TestTrue(TEXT("Gap leaves consumer cursor unchanged"),Cursor.Consume(Gap) == EFactResult::Gap && Cursor.LastRevision() == 0);
	auto WrongDomain = First; ++WrongDomain.Body.Generation;
	TestTrue(TEXT("Wrong lifetime domain rejected"),Cursor.Consume(WrongDomain) == EFactResult::WrongDomain);
	auto ReboundFact = First; ++ReboundFact.BindingEpoch;
	TestTrue(TEXT("Same body/solver with a different binding is a distinct journal domain"),Cursor.Consume(ReboundFact) == EFactResult::WrongDomain);
	TestTrue(TEXT("Newer intent/topology cannot filter an already committed fact"),Cursor.Consume(First) == EFactResult::Consumed && Cursor.LastRevision() == 1);
	TestTrue(TEXT("Repeated delivery is deduplicated"),Cursor.Consume(First) == EFactResult::Duplicate);
	Swap(F.C.Entry,F.C.Exit); ++F.C.Revision; ++F.Step.SolverFrame; ++F.Step.EvolutionSerial;
	F.Start.Pose.SetLocation(FVector(1000.5,0,0)); F.Solved.Motion.Pose.SetLocation(FVector(999.5,0,0));
	TestFalse(TEXT("Full journal refuses a new permission before commit"),C.EvaluateInterval(F.C,F.Start,F.Solved.Motion,F.Step,0,false).BypassSupport);
	TestTrue(TEXT("Full journal cannot write or silently discard first fact"),C.CommitSolved(F.C,F.Solved,F.Step,Adapter) == ETransferResult::JournalFull
		&& Adapter.Writes == 1 && C.PendingFacts().Num() == 1);
	auto BadAck = Cursor.Acknowledgment(); BadAck.LastConsumedRevision = 2;
	TestFalse(TEXT("Future acknowledgment cannot erase facts"),C.Acknowledge(BadAck));
	BadAck = Cursor.Acknowledgment(); ++BadAck.BindingEpoch;
	TestFalse(TEXT("Old/new binding acknowledgments cannot clear another journal"),C.Acknowledge(BadAck));
	TestTrue(TEXT("Consumed contiguous prefix acknowledged exactly once"),C.Acknowledge(Cursor.Acknowledgment())
		&& C.Acknowledge(Cursor.Acknowledgment()) && C.PendingFacts().IsEmpty());
	++F.Step.SolverFrame; ++F.Step.EvolutionSerial;
	C.EvaluateInterval(F.C,F.Start,F.Solved.Motion,F.Step,0,false);
	TestTrue(TEXT("Acknowledgment frees journal capacity for reversal"),C.CommitSolved(F.C,F.Solved,F.Step,Adapter) == ETransferResult::Committed && C.TransferRevision() == 2);
	TestTrue(TEXT("Consumer accepts next actual interval once"),Cursor.Consume(C.PendingFacts()[0]) == EFactResult::Consumed);
	C.Retire();
	TestTrue(TEXT("Retirement still permits committed-fact acknowledgment"),C.Acknowledge(Cursor.Acknowledgment()) && C.PendingFacts().IsEmpty());
	return true;
}
#endif
