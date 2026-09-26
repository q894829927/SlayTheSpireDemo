#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interior/InteriorPortalPhysicsBoundary.h"
#include "Interior/InteriorPortalMath.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/ContactModification.h"
#include "Chaos/ParticleHandle.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "HAL/ThreadSafeCounter64.h"

namespace
{
	using namespace InteriorPortalPhysics;
	struct FSpikeInput : Chaos::FSimCallbackInput
	{
		FBoundaryCommand Command;
		void Reset() { Command = FBoundaryCommand(); }
	};
	struct FSpikeSample
	{
		FBoundaryReceipt Receipt;
		FBoundaryState Before, Integrated, Solved, Published;
		FVector AppliedForce = FVector::ZeroVector;
		FVector GateImpulse = FVector::ZeroVector;
		FBoundaryDecision Decision;
		uint32 ReuseOrdinal = 0;
		int32 OwnPairs = 0, DisabledPairs = 0, UnrelatedPairs = 0;
		int32 Transfers = 0;
		uint32 Order = 0; // append phase digits: 1=preintegrate, 2=contact, 3=presolve, 4=postsolve.
		bool RepeatedSolverFrame = false;
		bool Retired = false;
		bool UnregistrationObserved = false;
		bool OnGameThread = false;
	};
	struct FSpikeOutput : Chaos::FSimCallbackOutput
	{
		TArray<FSpikeSample> Samples;
		void Reset() { Samples.Reset(); }
	};
	FThreadSafeCounter64 SolverEpochs;
	/** Editor-only experiment. Never attached to a gameplay system or its legacy body writers. */
	class FPortalBoundarySpike final : public Chaos::TSimCallbackObject<FSpikeInput, FSpikeOutput,
		Chaos::ESimCallbackOptions::PreIntegrate | Chaos::ESimCallbackOptions::PostIntegrate
		| Chaos::ESimCallbackOptions::ContactModification | Chaos::ESimCallbackOptions::PreSolve
		| Chaos::ESimCallbackOptions::PostSolve | Chaos::ESimCallbackOptions::ParticleUnregister>
	{
	public:
		const uint64 Epoch = SolverEpochs.Increment();
		void BindBeforeDispatch(Chaos::FSingleParticlePhysicsProxy* InBody,
			Chaos::FSingleParticlePhysicsProxy* EntrySupport, Chaos::FSingleParticlePhysicsProxy* ExitSupport,
			FTravellerHandle BodyToken, uint64 PairToken)
		{ Body = InBody; Supports[0] = EntrySupport; Supports[1] = ExitSupport; BoundBody = BodyToken; BoundPair = PairToken; }
		virtual FName GetFNameForStatId() const override { return TEXT("PortalBoundarySpike"); }
	private:
		Chaos::FSingleParticlePhysicsProxy* Body = nullptr;
		Chaos::FSingleParticlePhysicsProxy* Supports[2] = {nullptr, nullptr};
		bool Retired = false;
		bool UnregistrationObserved = false;
		int32 PortalIndex = 0, Transfers = 0, LastFrame = -1;
		uint64 Serial = 0, Revision = 0;
		uint32 Reuse = 0;
		FBoundaryCommand Current, LastCommand;
		FTravellerHandle BoundBody;
		uint64 BoundPair = 0;
		bool HasInput = false, BindingMatches = false;
		FSpikeSample* Sample = nullptr;
		// UE registers evolution callbacks in its simulation list too. Presimulate
		// is intentionally empty: interval work starts at the actual PreIntegrate hook.
		virtual void OnPreSimulate_Internal() override {}
		FBoundaryState Read(bool Predicted) const
		{
			FBoundaryState S;
			if (!Retired && Body)
			{
				if (const auto* Handle = Body->GetPhysicsThreadAPI())
				{
					const auto* Rigid = Body->GetHandle_LowLevel()->CastToRigidParticle();
					S.Pose = FTransform(Predicted && Rigid ? Rigid->GetQ() : Handle->R(),
						Predicted && Rigid ? Rigid->GetP() : Handle->X(), Current.Traveller.Geometry.BakedScale);
					S.LinearVelocity = Handle->V(); S.AngularVelocity = Handle->W();
				}
			}
			return S;
		}
		virtual void OnPreIntegrate_Internal() override
		{
			const auto* Input = GetConsumerInput_Internal();
			HasInput = Input != nullptr;
			Current = HasInput ? Input->Command : LastCommand;
			if (HasInput) { LastCommand = Current; }
			BindingMatches = Current.Traveller.Handle == BoundBody && Current.PairGeneration == BoundPair;
			// Owner cancellation reaches the same marshalled boundary as physics removal.
			// The native unregister callback is a second lifetime guard, not the sole cancellation signal.
			if (HasInput && !BindingMatches) { Retired = true; }
			if (Current.Revision > Revision) { Revision = Current.Revision; Reuse = 0; }
			if (PortalIndex == 1) { Swap(Current.Entry, Current.Exit); }
			auto& Out = GetProducerOutputData_Internal();
			Sample = &Out.Samples.AddDefaulted_GetRef();
			Sample->Receipt = {Current.Traveller.Handle, Current.PairGeneration, Current.Revision,
				{Epoch, static_cast<Chaos::FPBDRigidsSolver*>(GetSolver())->GetCurrentFrame(), ++Serial, GetDeltaTime_Internal()}};
			Sample->RepeatedSolverFrame = Sample->Receipt.Step.SolverFrame <= LastFrame;
			LastFrame = Sample->Receipt.Step.SolverFrame;
			Sample->ReuseOrdinal = Reuse++; Sample->Retired = Retired;
			Sample->UnregistrationObserved = UnregistrationObserved;
			Sample->OnGameThread = IsInGameThread(); Sample->Order = 1;
			Sample->Before = Read(false);
			if (!Retired && Body && HasInput && BindingMatches && Current.Revision == Revision && Sample->ReuseOrdinal < Current.MaxReuseSteps
				&& Body->GetPhysicsThreadAPI())
			{
				// The spike has no holding route. Never reapply an old-space intent after transfer.
				Sample->AppliedForce = Transfers ? FVector::ZeroVector : BoundedForce(Current);
				Body->GetPhysicsThreadAPI()->AddForce(Sample->AppliedForce);
			}
			if (!Retired && Body && Body->GetPhysicsThreadAPI())
			{
				auto* Handle = Body->GetPhysicsThreadAPI();
				FBoundaryState Predicted = Sample->Before;
				Predicted.Pose.AddToTranslation((Handle->V() + Handle->Acceleration() * Sample->Receipt.Step.DeltaSeconds)
					* Sample->Receipt.Step.DeltaSeconds);
				const auto Preflight = EvaluateBoundary(Current, Sample->Before, Predicted,
					Sample->Receipt.Step, Sample->ReuseOrdinal, false);
				const bool Block = !HasInput || !BindingMatches || Current.Revision != Revision || !Preflight.BypassSupport;
				const auto Span = EvaluatePose(Current.Traveller.Geometry, Sample->Before.Pose, Current.Entry,
					Current.HalfWidth, Current.HalfHeight, Current.MarginCm);
				const double Normal = Current.Entry.InverseTransformPositionNoScale(Sample->Before.Pose.GetLocation()).X;
				if (Block && Normal > 0 && (Span.MinNormal <= Current.SupportHalfThicknessCm
					|| Current.Entry.InverseTransformPositionNoScale(Predicted.Pose.GetLocation()).X <= 0))
				{
					// A cancellation is one physics-boundary transaction. Stop only inward
					// motion before integration, then let the original wall contacts solve.
					const FVector Projected = StaticGateProjection(Handle->V(),Current.Entry);
					Sample->GateImpulse = (Projected - Handle->V()) * Handle->M();
					Handle->SetV(Projected);
					Handle->SetAcceleration(StaticGateProjection(Handle->Acceleration(),Current.Entry));
				}
			}
		}
		virtual void OnPostIntegrate_Internal() override
		{
			Sample->Integrated = Read(true);
			Sample->Decision = EvaluateBoundary(Current, Sample->Before, Sample->Integrated,
				Sample->Receipt.Step, Sample->ReuseOrdinal, false);
			if (!HasInput) { Sample->Decision = { EBoundaryReason::ExpiredInput, false, false }; }
			if (Retired || !BindingMatches || Current.Revision != Revision || Sample->RepeatedSolverFrame)
			{ Sample->Decision = { EBoundaryReason::UnsupportedStep, false, false }; }
		}
		virtual void OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier) override
		{
			if (!Sample) { return; }
			Sample->Order = Sample->Order * 10 + 2;
			bool OtherContact = false;
			for (auto& Pair : Modifier)
			{
				const auto Particles = Pair.GetParticlePair();
				const auto* A = Particles[0]->PhysicsProxy(); const auto* B = Particles[1]->PhysicsProxy();
				const bool HasBody = A == Body || B == Body;
				const bool Own = HasBody && (A == Supports[PortalIndex] || B == Supports[PortalIndex]);
				if (!Own) { ++Sample->UnrelatedPairs; }
				if (HasBody && !Own) { OtherContact = true; }
			}
			if (OtherContact) { Sample->Decision = { EBoundaryReason::OtherContact, false, false }; }
			for (auto& Pair : Modifier)
			{
				const auto Particles = Pair.GetParticlePair();
				const auto* A = Particles[0]->PhysicsProxy(); const auto* B = Particles[1]->PhysicsProxy();
				if ((A == Body || B == Body) && (A == Supports[PortalIndex] || B == Supports[PortalIndex]))
				{
					++Sample->OwnPairs;
					if (Sample->Decision.BypassSupport) { Pair.Disable(); ++Sample->DisabledPairs; }
					// No broad collision-filter change or persistent ignore: a rejected step uses ordinary contacts.
				}
			}
		}
		virtual void OnPreSolve_Internal() override { Sample->Order = Sample->Order * 10 + 3; }
		virtual void OnPostSolve_Internal() override
		{
			Sample->Order = Sample->Order * 10 + 4;
			Sample->Solved = Read(true);
			if (Sample->Decision.TransferAfterSolve && !Retired && Body && Body->GetPhysicsThreadAPI())
			{
				// Experimental end-of-interval commit for the isolated static corridor only.
				// The fixture certifies destination opening/corridor before submitting this command.
				const FQuat Q = InteriorPortalMath::Rotation(Current.Entry, Current.Exit);
				auto* Handle = Body->GetPhysicsThreadAPI();
				Handle->SetX(InteriorPortalMath::Position(Sample->Solved.Pose.GetLocation(), Current.Entry, Current.Exit));
				Handle->SetR(Q * Sample->Solved.Pose.GetRotation());
				Handle->SetV(Q.RotateVector(Sample->Solved.LinearVelocity));
				Handle->SetW(Q.RotateVector(Sample->Solved.AngularVelocity));
				PortalIndex = 1 - PortalIndex; ++Transfers;
			}
			Sample->Transfers = Transfers; Sample->Published = Read(true);
		}
		virtual void OnParticleUnregistered_Internal(TArray<TTuple<Chaos::FUniqueIdx, Chaos::FSingleParticlePhysicsProxy*>>& Proxies) override
		{
			for (const auto& Item : Proxies)
			{
				const auto* Proxy = Item.Get<1>();
				if (Proxy == Body || Proxy == Supports[0] || Proxy == Supports[1])
				{ Retired = true; UnregistrationObserved = true; }
			}
		}
	};

	struct FNativeScene
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FPhysScene* Scene = nullptr;
		UBoxComponent* Body = nullptr;
		UBoxComponent* ProtectedBody = nullptr;
		UBoxComponent* EntryWall = nullptr;
		UBoxComponent* ExitWall = nullptr;
		FTravellerRegistry Registry;
		FPortalBoundarySpike* Callback = nullptr;
		FBoundaryCommand Command;
		FPhysicsStepKey LastConsumed;
		TArray<FSpikeSample> Samples;
		bool ReceiptsValid = true;
		uint64 NextRevision = 1;
		FNativeScene(bool Substeps, bool TaskGraph, bool Permit, bool Transfer = false)
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL()); Scene = World->GetPhysicsScene();
			Scene->GetSolver()->SetThreadingMode_External(TaskGraph ? Chaos::EThreadingModeTemp::TaskGraph : Chaos::EThreadingModeTemp::SingleThread);
			EntryWall = Box(FVector::ZeroVector, FVector(2,100,150), false);
			ExitWall = Box(FVector(1000,0,0), FVector(2,100,150), false);
			Body = Box(FVector(8,0,0), FVector(5), true);
			ProtectedBody = Box(FVector(8,40,0), FVector(5), true);
			Body->SetPhysicsLinearVelocity(FVector(-120,0,0));
			ProtectedBody->SetPhysicsLinearVelocity(FVector(-120,0,0));
			EGeometryResult Reason; Registry.Register(Body, Reason); Registry.Capture(Body, Command.Traveller);
			Command.PairGeneration = 1; Command.PermitSupportBypass = Permit;
			Command.IsolatedStaticScope = true; Command.PermitTransfer = Transfer;
			Command.Exit = FTransform(FVector(1000,0,0));
			Command.MaxStepSeconds = 1. / 60.; Command.MaxTranslationCm = 2.01;
			Command.MaxReuseSteps = Substeps ? 2 : 1;
			Command.SupportHalfThicknessCm = 2;
			// Both authored collision walls are fully covered by the known opening for this box.
			// The isolated fixture has no other object in the mapped remaining-motion corridor.
			Command.ExitCorridorCertified = true;
			Callback = Scene->GetSolver()->CreateAndRegisterSimCallbackObject_External<FPortalBoundarySpike>();
			Callback->BindBeforeDispatch(Body->BodyInstance.GetPhysicsActor(),
				EntryWall->BodyInstance.GetPhysicsActor(), ExitWall->BodyInstance.GetPhysicsActor(),
				Command.Traveller.Handle, Command.PairGeneration);
		}
		~FNativeScene()
		{
			Scene->GetSolver()->UnregisterAndFreeSimCallbackObject_External(Callback);
			Scene->SetUpForFrame(&FVector::ZeroVector, 0, 0, 1.f/60, 1.f/120, 2, false);
			Scene->StartFrame(); Scene->WaitPhysScenes(); Scene->EndFrame(); World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		UBoxComponent* Box(const FVector& Position, const FVector& Extent, bool Dynamic)
		{
			AActor* Owner = World->SpawnActor<AActor>();
			auto* Shape = NewObject<UBoxComponent>(Owner); Owner->SetRootComponent(Shape);
			Owner->AddInstanceComponent(Shape);
			Shape->SetBoxExtent(Extent); Shape->SetMobility(Dynamic ? EComponentMobility::Movable : EComponentMobility::Static);
			Shape->SetCollisionProfileName(TEXT("PhysicsActor")); Shape->BodyInstance.bContactModification = true;
			Owner->SetActorLocation(Position); Shape->RegisterComponent();
			Shape->SetEnableGravity(false); Shape->SetLinearDamping(0); Shape->SetAngularDamping(0);
			if (Dynamic) { Shape->SetSimulatePhysics(true); }
			return Shape;
		}
		void Advance(bool Substeps, bool Submit = true)
		{
			if (Submit)
			{
				Command.Revision = NextRevision++;
				Callback->GetProducerInputData_External()->Command = Command;
			}
			Scene->SetUpForFrame(&FVector::ZeroVector, 1.f/60, 0, 1.f/60, 1.f/120, 2, Substeps);
			Scene->StartFrame(); Scene->WaitPhysScenes(); Scene->EndFrame();
			while (auto Output = Callback->PopOutputData_External())
			{
				for (const auto& S : Output->Samples)
				{
					ReceiptsValid &= CanConsumeReceipt(S.Receipt, Command.Traveller.Handle, Command.PairGeneration,
						Command.Revision, Callback->Epoch, LastConsumed);
					LastConsumed = S.Receipt.Step; Samples.Add(S);
				}
			}
		}
	};
	constexpr EAutomationTestFlags PortalBoundaryTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalBoundaryProtocolTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsBoundary.Protocol", PortalBoundaryTestFlags)
bool FPortalBoundaryProtocolTest::RunTest(const FString& Parameters)
{
	FBoundaryCommand C; C.Traveller.Handle = {1,2,3}; C.PairGeneration = 4; C.Revision = 5;
	C.PermitSupportBypass = true; C.IsolatedStaticScope = true; C.MaxReuseSteps = 2;
	FPrimitive P; P.Radius = 5; C.Traveller.Geometry.Primitives.Add(P);
	FBoundaryState A,B; A.Pose.SetLocation(FVector(8,0,0)); B.Pose.SetLocation(FVector(7,0,0));
	FPhysicsStepKey Step {7,10,1,1./120};
	TestTrue(TEXT("Certified fixed-pose bounded interval permits only its support pair"), EvaluateBoundary(C,A,B,Step,0,false).BypassSupport);
	TestTrue(TEXT("Input cannot authorize beyond its declared reuse window"), EvaluateBoundary(C,A,B,Step,2,false).Reason == EBoundaryReason::ExpiredInput);
	TestFalse(TEXT("Unrelated body contact revokes support bypass"), EvaluateBoundary(C,A,B,Step,0,true).BypassSupport);
	B.AngularVelocity = FVector(0,0,1);
	TestTrue(TEXT("Angular motion outside proved interval fails closed"), EvaluateBoundary(C,A,B,Step,0,false).Reason == EBoundaryReason::UnsupportedRotation);
	B.AngularVelocity = FVector::ZeroVector; B.Pose.SetLocation(FVector(-8,0,0));
	TestTrue(TEXT("Large/high-speed movement outside proved interval fails closed"), EvaluateBoundary(C,A,B,Step,0,false).Reason == EBoundaryReason::UnsupportedMotion);
	A.Pose.SetLocation(FVector(.5,0,0)); B.Pose.SetLocation(FVector(-.5,0,0));
	TestTrue(TEXT("Uncertified crossing cannot leave support collision disabled"), EvaluateBoundary(C,A,B,Step,0,false).Reason == EBoundaryReason::TransferNotCertified);
	C.PermitTransfer = true; C.ExitCorridorCertified = true;
	TestTrue(TEXT("Restricted certified crossing requests one end-of-interval commit"), EvaluateBoundary(C,A,B,Step,0,false).TransferAfterSolve);
	C.ForceIntent = FVector(300,400,0); C.MaxForce = 10;
	TestTrue(TEXT("Adapter force intent has explicit norm cap"), FMath::IsNearlyEqual(BoundedForce(C).Size(),10.,1.e-6));
	const FVector Incoming(-120,30,40), Projected = StaticGateProjection(Incoming,FTransform::Identity);
	TestTrue(TEXT("Static reaction preserves tangent motion and removes only inward normal velocity"), Projected == FVector(0,30,40));
	TestTrue(TEXT("Static reaction cannot add kinetic energy"), Projected.SizeSquared() <= Incoming.SizeSquared());
	FBoundaryReceipt R {C.Traveller.Handle,C.PairGeneration,C.Revision,Step}; FPhysicsStepKey Last;
	TestTrue(TEXT("Current receipt can be consumed"), CanConsumeReceipt(R,C.Traveller.Handle,4,5,7,Last));
	Last = Step;
	TestFalse(TEXT("Same solver interval cannot be consumed twice"), CanConsumeReceipt(R,C.Traveller.Handle,4,5,7,Last));
	Last = FPhysicsStepKey();
	TestFalse(TEXT("Stale body generation rejected after handoff"), CanConsumeReceipt(R,{1,2,4},4,5,7,Last));
	TestFalse(TEXT("Stale topology rejected after handoff"), CanConsumeReceipt(R,C.Traveller.Handle,6,5,7,Last));
	TestFalse(TEXT("Replaced solver epoch rejects old callback output"), CanConsumeReceipt(R,C.Traveller.Handle,4,5,8,Last));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalNativeContactBoundaryTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsBoundary.NativeContactAndSubsteps", PortalBoundaryTestFlags)
bool FPortalNativeContactBoundaryTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (bool TaskGraph : {false,true})
	{
		FNativeScene F(Substeps,TaskGraph,true);
		for (int32 I=0;I<3;++I) { F.Advance(Substeps); }
		TestTrue(TEXT("Native results arrive through timestamp-gated callback output queue"), F.ReceiptsValid && F.Samples.Num() == (Substeps ? 6 : 3));
		int32 Disabled = 0, Unrelated = 0;
		for (const auto& S : F.Samples)
		{
			TestTrue(TEXT("Contact modification precedes solve and result publication"), S.Order == 1234);
			TestFalse(TEXT("Solver step key is not a repeated game-frame identity"), S.RepeatedSolverFrame);
			TestTrue(TEXT("Callback duration is actual configured solver interval"), FMath::IsNearlyEqual(S.Receipt.Step.DeltaSeconds,Substeps ? 1./120 : 1./60,1.e-8));
			Disabled += S.DisabledPairs; Unrelated += S.UnrelatedPairs;
		}
		TestTrue(TEXT("Actual intended contact pair was disabled"), Disabled > 0);
		TestTrue(TEXT("Other body's support contacts remain observable and untouched"), Unrelated > 0);
		TestTrue(TEXT("Protected body remains blocked at support while selected body advances"),
			F.ProtectedBody->GetComponentLocation().X > 6.8 && F.Body->GetComponentLocation().X < 3);
		F.Command.PermitSupportBypass = false; F.Advance(Substeps);
		TestTrue(TEXT("Revocation restores ordinary wall response without a broad ignore"), F.Body->GetComponentLocation().X > 6.8);
		for (int32 I=0;I<3;++I) { F.Advance(Substeps,false); }
		TestTrue(TEXT("Reused old producer data eventually expires"), F.Samples.Last().Decision.Reason == EBoundaryReason::ExpiredInput);
		AddInfo(FString::Printf(TEXT("Contact profile substeps=%d taskgraph=%d samples=%d disabled=%d unrelated=%d protectedX=%.6f restoredX=%.6f"),
			Substeps,TaskGraph,F.Samples.Num(),Disabled,Unrelated,F.ProtectedBody->GetComponentLocation().X,F.Body->GetComponentLocation().X));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalNativeTransferBoundaryTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsBoundary.NativeTransferAndRetirement", PortalBoundaryTestFlags)
bool FPortalNativeTransferBoundaryTest::RunTest(const FString& Parameters)
{
	FNativeScene F(true,true,true,true);
	for (int32 I=0;I<8;++I) { F.Advance(true); }
	int32 Commits = 0; const FSpikeSample* Commit = nullptr;
	for (const auto& S : F.Samples) if (S.Decision.TransferAfterSolve) { ++Commits; Commit = &S; }
	TestTrue(TEXT("Native crossing commits once across successive substeps"), Commits == 1 && F.Samples.Last().Transfers == 1);
	if (Commit)
	{
		TestTrue(TEXT("Committed endpoint pose is mapped, not offset to clear a wall"),
			Commit->Published.Pose.GetLocation().Equals(InteriorPortalMath::Position(Commit->Solved.Pose.GetLocation(),F.Command.Entry,F.Command.Exit),1.e-6));
		TestTrue(TEXT("Transfer maps velocity and preserves its magnitude"),
			Commit->Published.LinearVelocity.Equals(InteriorPortalMath::Rotation(F.Command.Entry,F.Command.Exit)
				.RotateVector(Commit->Solved.LinearVelocity),1.e-6)
			&& FMath::IsNearlyEqual(Commit->Published.LinearVelocity.Size(),Commit->Solved.LinearVelocity.Size(),1.e-6));
	}
	TestTrue(TEXT("Next substep starts in destination and continues outbound"), F.Body->GetComponentLocation().X > 1007 && F.Body->GetPhysicsLinearVelocity().X > 119);
	TestTrue(TEXT("Transfer receipts retain coherent solver identities"), F.ReceiptsValid);
	const auto OldBodyToken = F.Command.Traveller.Handle;
	F.Registry.Unregister(F.Body); F.Command.Traveller.Handle = {};
	AActor* BodyOwner = F.Body->GetOwner(); F.Body->DestroyComponent(); BodyOwner->Destroy(); F.Advance(true);
	TestTrue(TEXT("Owner cancellation retires binding at the first affected physics boundary"), F.Samples.Last().Retired);
	TestFalse(TEXT("Retired binding cannot revive contact permission"), F.Samples.Last().Decision.BypassSupport);
	F.Command.Traveller.Handle = OldBodyToken; F.Advance(true);
	TestTrue(TEXT("Native unregister is observed before deallocated proxy could be consumed again"), F.Samples.Last().UnregistrationObserved);
	TestTrue(TEXT("A later replay cannot revive the permanently retired binding"), F.Samples.Last().Retired && !F.Samples.Last().Decision.BypassSupport);
	AddInfo(FString::Printf(TEXT("Native transfer commits=%d samples=%d retired=%d"),Commits,F.Samples.Num(),F.Samples.Last().Retired));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalNativeForceBoundaryTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsBoundary.NativeBoundedForce", PortalBoundaryTestFlags)
bool FPortalNativeForceBoundaryTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true})
	{
		FNativeScene F(Substeps,true,false);
		F.Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		F.Command.ForceIntent = FVector(100000,0,0); F.Command.MaxForce = 1200;
		const double Mass = F.Body->GetMass();
		for (int32 I=0;I<4;++I) { F.Advance(Substeps); }
		for (const auto& S : F.Samples)
		{
			TestTrue(TEXT("Preintegrate applies only capped adapter force"), FMath::IsNearlyEqual(S.AppliedForce.Size(),1200.,1.e-5));
			const double ExpectedDV = 1200. / Mass * S.Receipt.Step.DeltaSeconds;
			TestTrue(TEXT("Actual Chaos impulse matches one bounded force application per substep"),
				FMath::IsNearlyEqual(S.Integrated.LinearVelocity.X-S.Before.LinearVelocity.X,ExpectedDV,1.e-4));
		}
		AddInfo(FString::Printf(TEXT("Native bounded force substeps=%d massKg=%.6f maxForce=1200 finalVX=%.6f samples=%d"),
			Substeps,Mass,F.Body->GetPhysicsLinearVelocity().X,F.Samples.Num()));
	}
	return true;
}
#endif
