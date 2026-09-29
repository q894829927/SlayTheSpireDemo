#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interior/InteriorPortalPhysicsBoundary.h"
#include "Interior/InteriorPortalHoldSolver.h"
#include "Interior/InteriorPortalPassageCoordinator.h"
#include "Interior/InteriorPortalChaosTransferAdapter.h"
#include "Interior/InteriorPortalChaosStaticClearance.h"
#include "Interior/InteriorPortalChaosSpeedCap.h"
#include "Interior/InteriorPortalChaosPassageSession.h"
#include "Interior/InteriorPortalPhysicsBinding.h"
#include "Interior/InteriorPortalPhysicsBindingBridge.h"
#include "Interior/InteriorPortalWorldPassageQuery.h"
#include "Interior/InteriorPortalMath.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/ContactModification.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/Island/IslandManager.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "HAL/ThreadSafeCounter64.h"
#include <limits>

namespace
{
	using namespace InteriorPortalPhysics;
	struct FSpikeInput : Chaos::FSimCallbackInput
	{
		FBoundaryCommand Command;
		bool HoldEnabled = false;
		FHoldCommand Hold;
		FTransferAcknowledgment TransferAck;
		void Reset() { Command = FBoundaryCommand(); HoldEnabled = false; Hold = FHoldCommand(); TransferAck = {}; }
	};
	struct FSpikeSample
	{
		FBoundaryReceipt Receipt;
		FBoundaryState Before, Integrated, Solved, Published;
		FVector AppliedForce = FVector::ZeroVector;
		FVector AppliedTorque = FVector::ZeroVector;
		FHoldTarget HoldTarget;
		FVector LocalInertia = FVector::ZeroVector;
		TArray<FTransferFact> Facts;
		FHoldRelationSnapshot Relation;
		bool MaterialPreserved = true;
		bool AdapterRejectionsAtomic = true;
		bool ProofProtocolValid = true;
		int32 ClearanceScans = 0;
		EStaticClearanceReason PreClearance = EStaticClearanceReason::InvalidInterval;
		EStaticClearanceReason IntegratedClearance = EStaticClearanceReason::InvalidInterval;
		EStaticClearanceReason SolvedClearance = EStaticClearanceReason::InvalidInterval;
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
		bool ProbeAdapterRejections = false; // configured only before dispatch
		bool NativeClearanceEnabled = false, ProbeClearanceProtocol = false;
		FPassageSessionHandoff LastHandoff;
		TFunction<void()> AfterIntegrateMutation, AfterSolveMutation; // editor fixture injections, configured before dispatch
		void ConfigureClearanceBeforeDispatch(const FPreparedPhysicsBinding& Prepared)
		{
			check(Session->AttachClearance_Internal(Prepared));
			Clearance = Session->Clearance();
			NativeClearanceEnabled = true;
		}
		void ConfigureHoldBeforeDispatch(uint64 RouteRevision, uint64 RegionRevision)
		{ HoldRouteRevision = RouteRevision; HoldRegionRevision = RegionRevision; }
		bool ConfigureRelationBeforeDispatch(const FTransform& Desired, const FVector& Anchor)
		{ return Coordinator->AcquireHold(17,{1,71,1},0,Desired,Anchor); }
		void BindBeforeDispatch(Chaos::FSingleParticlePhysicsProxy* InBody,
			Chaos::FSingleParticlePhysicsProxy* EntrySupport, Chaos::FSingleParticlePhysicsProxy* ExitSupport,
			const FBoundaryCommand& BindingCommand)
		{
			Body = InBody; Supports[0] = EntrySupport; Supports[1] = ExitSupport;
			BoundBody = BindingCommand.Traveller.Handle; BoundPair = BindingCommand.PairGeneration;
			Session = MakeUnique<FChaosPassageSession>(static_cast<Chaos::FPBDRigidsSolver*>(GetSolver()),
				Body,Supports[0],Supports[1],BindingCommand,Epoch,Epoch);
			check(!Session->IsRetired());
			Coordinator = Session->Passage(); Adapter = Session->Transfer();
		}
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
		uint64 HoldRouteRevision = 0, HoldRegionRevision = 0;
		TUniquePtr<FChaosPassageSession> Session;
		FPassageCoordinator* Coordinator = nullptr;
		FChaosTransferAdapter* Adapter = nullptr;
		FChaosStaticClearance* Clearance = nullptr;
		TOptional<FStaticClearanceProof> PreviousProof;
		bool NativeIntervalEligible = false;
		bool HasInput = false, BindingMatches = false;
		FSpikeSample* Sample = nullptr;
		void RetireBinding()
		{
			Retired = true;
			LastHandoff = Session->Retire_Internal();
		}
		// UE registers evolution callbacks in its simulation list too. Presimulate
		// is intentionally empty: interval work starts at the actual PreIntegrate hook.
		virtual void OnPreSimulate_Internal() override {}
		bool ProveClearance(EClearanceStage Stage, const FBoundaryState& End)
		{
			if (!NativeClearanceEnabled) { return true; }
			Current.IsolatedStaticScope = false; Current.ExitCorridorCertified = false;
			if (Retired) { return false; }
			const auto Proof = Clearance->Certify_Internal(Current,Sample->Before,End,Sample->Receipt.Step,Stage,PortalIndex);
			++Sample->ClearanceScans;
			if (Stage == EClearanceStage::PreIntegrate) { Sample->PreClearance = Proof.Reason(); }
			if (Stage == EClearanceStage::PostIntegrate) { Sample->IntegratedClearance = Proof.Reason(); }
			if (Stage == EClearanceStage::PostSolve) { Sample->SolvedClearance = Proof.Reason(); }
			if (Proof.Reason() == EStaticClearanceReason::BindingChanged || Proof.Reason() == EStaticClearanceReason::Retired)
			{
				RetireBinding(); Sample->Retired = true;
			}
			if (ProbeClearanceProtocol && Proof.Reason() == EStaticClearanceReason::Clear)
			{
				auto WrongStep = Sample->Receipt.Step; ++WrongStep.EvolutionSerial;
				auto WrongCommand = Current; ++WrongCommand.Revision;
				auto WrongEnd = End; WrongEnd.Pose.AddToTranslation(FVector(1,0,0));
				Sample->ProofProtocolValid &= !Clearance->Consume_Internal(Proof,Current,Sample->Before,End,WrongStep,Stage)
					&& !Clearance->Consume_Internal(Proof,WrongCommand,Sample->Before,End,Sample->Receipt.Step,Stage)
					&& !Clearance->Consume_Internal(Proof,Current,Sample->Before,WrongEnd,Sample->Receipt.Step,Stage)
					&& !Clearance->Consume_Internal(Proof,Current,Sample->Before,End,Sample->Receipt.Step,
						Stage == EClearanceStage::PreIntegrate ? EClearanceStage::PostIntegrate : EClearanceStage::PreIntegrate);
				if (PreviousProof.IsSet())
				{ Sample->ProofProtocolValid &= !Clearance->Consume_Internal(PreviousProof.GetValue(),Current,Sample->Before,End,Sample->Receipt.Step,Stage); }
			}
			bool Allowed = Clearance->Consume_Internal(Proof,Current,Sample->Before,End,Sample->Receipt.Step,Stage);
			if (ProbeClearanceProtocol && Allowed)
			{ Sample->ProofProtocolValid &= !Clearance->Consume_Internal(Proof,Current,Sample->Before,End,Sample->Receipt.Step,Stage); }
			PreviousProof = Proof;
			if (Stage == EClearanceStage::PreIntegrate) { NativeIntervalEligible = Allowed; }
			else { Allowed &= NativeIntervalEligible; }
			Current.IsolatedStaticScope = Allowed; Current.ExitCorridorCertified = Allowed;
			return Allowed;
		}
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
			if (HasInput && !BindingMatches) { RetireBinding(); }
			if (Retired) { LastHandoff = Session->Retire_Internal(); }
			if (HasInput) { Coordinator->Acknowledge(Input->TransferAck); }
			PortalIndex = Coordinator->BodyEndpoint();
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
			if (!Retired && Body && HasInput && BindingMatches && !Sample->RepeatedSolverFrame && Current.Revision == Revision && Sample->ReuseOrdinal < Current.MaxReuseSteps
				&& Body->GetPhysicsThreadAPI())
			{
				// The spike has no holding route. Never reapply an old-space intent after transfer.
				Sample->AppliedForce = Transfers ? FVector::ZeroVector : BoundedForce(Current);
				if (Input->HoldEnabled)
				{
					Sample->AppliedForce = FVector::ZeroVector;
					auto Hold = Input->Hold;
					const auto Relation = Coordinator->Hold();
					if (Relation.Active)
					{
						Hold.Route = Relation.Route; Hold.LocalGrabAnchor = Relation.LocalAnchor;
						Hold.DesiredHolderPose = Relation.DesiredHolderPose;
					}
					if ((!Transfers || Relation.Active) && Hold.Traveller.Handle == BoundBody && Hold.Traveller.Geometry == Current.Traveller.Geometry)
					{
						const auto* Handle = Body->GetPhysicsThreadAPI();
						const FVector InvInertia = FVector(Handle->InvI());
						FHoldPhysicalState State;
						State.Body = Sample->Before; State.MassKg = Handle->M(); State.LocalCOM = Handle->CenterOfMass();
						for (int32 I = 0; I < 3; ++I) { State.LocalInertia[I] = InvInertia[I] > 0 ? 1. / InvInertia[I] : 0; }
						State.RotationOfMass = Handle->RotationOfMass(); Sample->LocalInertia = State.LocalInertia;
						const auto Drive = EvaluateHoldDrive(Hold, State, Sample->Receipt.Step, BoundBody, BoundPair,
							Relation.Active ? Relation.Route.Revision : HoldRouteRevision, HoldRegionRevision);
						Sample->HoldTarget = Drive.Target; Sample->AppliedForce = Drive.Force; Sample->AppliedTorque = Drive.Torque;
					}
				}
				Body->GetPhysicsThreadAPI()->AddForce(Sample->AppliedForce);
				Body->GetPhysicsThreadAPI()->AddTorque(Sample->AppliedTorque);
			}
			if (!Retired && Body && Body->GetPhysicsThreadAPI())
			{
				auto* Handle = Body->GetPhysicsThreadAPI();
				FBoundaryState Predicted = Sample->Before;
				Predicted.Pose.AddToTranslation((Handle->V() + Handle->Acceleration() * Sample->Receipt.Step.DeltaSeconds)
					* Sample->Receipt.Step.DeltaSeconds);
				ProveClearance(EClearanceStage::PreIntegrate,Predicted);
				const auto Preflight = EvaluateBoundary(Current, Sample->Before, Predicted,
					Sample->Receipt.Step, Sample->ReuseOrdinal, false);
				const bool Block = !HasInput || !BindingMatches || Sample->RepeatedSolverFrame || Current.Revision != Revision || !Preflight.BypassSupport;
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
			if (AfterIntegrateMutation) { AfterIntegrateMutation(); }
			Sample->Integrated = Read(true);
			ProveClearance(EClearanceStage::PostIntegrate,Sample->Integrated);
			Sample->Decision = Coordinator->EvaluateInterval(Current, Sample->Before, Sample->Integrated,
				Sample->Receipt.Step, Sample->ReuseOrdinal, false);
			if (!HasInput) { Sample->Decision = { EBoundaryReason::ExpiredInput, false, false }; }
			if (Retired || !BindingMatches || Current.Revision != Revision || Sample->RepeatedSolverFrame)
			{ Sample->Decision = { EBoundaryReason::UnsupportedStep, false, false }; }
			if (!Sample->Decision.BypassSupport) { Coordinator->RevokeInterval(Sample->Decision.Reason); }
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
			if (OtherContact)
			{ Sample->Decision = { EBoundaryReason::OtherContact, false, false }; Coordinator->RevokeInterval(EBoundaryReason::OtherContact); }
			if (NativeClearanceEnabled && Clearance && !Clearance->ContactsRemainIndependent_Internal(Modifier))
			{ Sample->Decision = { EBoundaryReason::OtherContact, false, false }; Coordinator->RevokeInterval(EBoundaryReason::OtherContact); }
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
			if (AfterSolveMutation) { AfterSolveMutation(); }
			Sample->Order = Sample->Order * 10 + 4;
			Sample->Solved = Read(true);
			if (!ProveClearance(EClearanceStage::PostSolve,Sample->Solved))
			{ Sample->Decision = {EBoundaryReason::TransferNotCertified,false,false}; Coordinator->RevokeInterval(EBoundaryReason::TransferNotCertified); }
			if (!Retired) { Adapter->BeginSolvedStep_Internal(Sample->Receipt.Step); }
			if (Sample->Decision.TransferAfterSolve && !Retired && Body && Body->GetPhysicsThreadAPI())
			{
				// Experimental end-of-interval commit for the isolated static corridor only.
				// The fixture certifies destination opening/corridor before submitting this command.
				auto* Handle = Body->GetPhysicsThreadAPI();
				FBodyCommitState Solved; Solved.Motion = Sample->Solved; Solved.MassKg = Handle->M();
				const FVector InvI = FVector(Handle->InvI());
				for (int32 I=0; I<3; ++I) { Solved.LocalInertia[I] = InvI[I] > 0 ? 1. / InvI[I] : 0; }
				Solved.LocalCOM = Handle->CenterOfMass(); Solved.RotationOfMass = Handle->RotationOfMass();
				Solved.Sleeping = Handle->ObjectState() == Chaos::EObjectStateType::Sleeping;
				struct FCheckedAdapter final : IPortalTransferAdapter
				{
					FChaosTransferAdapter* Native = nullptr;
					FPortalBoundarySpike* Owner = nullptr;
					virtual bool Commit(const FTransferFact& Fact) override
					{
						if (Owner->ProbeAdapterRejections)
						{
							const auto Reject = [&](const FTransferFact& Bad)
							{
								const auto Before = Owner->Read(true);
								const bool Rejected = !Native->Commit(Bad);
								const auto After = Owner->Read(true);
								Owner->Sample->AdapterRejectionsAtomic &= Rejected && Before.Pose.Equals(After.Pose,1.e-6)
									&& Before.LinearVelocity == After.LinearVelocity && Before.AngularVelocity == After.AngularVelocity;
							};
							FTransferFact Bad = Fact; ++Bad.BindingEpoch; Reject(Bad);
							Bad = Fact; ++Bad.Step.EvolutionSerial; Reject(Bad);
							Bad = Fact; Bad.Before.Motion.Pose.AddToTranslation(FVector(1,0,0)); Reject(Bad);
							Bad = Fact; Bad.After.MassKg += 1; Reject(Bad);
							Bad = Fact; Bad.After.Motion.LinearVelocity.X = std::numeric_limits<double>::infinity(); Reject(Bad);
						}
						const bool Committed = Native->Commit(Fact);
						if (Committed && Owner->ProbeAdapterRejections)
						{
							const auto Published = Owner->Read(true);
							Owner->Sample->AdapterRejectionsAtomic &= !Native->Commit(Fact)
								&& Published.Pose.Equals(Owner->Read(true).Pose,1.e-6);
						}
						return Committed;
					}
					} Checked;
					Checked.Native = Adapter; Checked.Owner = this;
					Coordinator->CommitSolved(Current,Solved,Sample->Receipt.Step,Checked);
				Sample->MaterialPreserved = Handle->M() == Solved.MassKg && FVector(Handle->InvI()) == InvI
					&& Handle->CenterOfMass() == Solved.LocalCOM && Handle->RotationOfMass() == Solved.RotationOfMass
					&& (Handle->ObjectState() == Chaos::EObjectStateType::Sleeping) == Solved.Sleeping;
			}
			Transfers = static_cast<int32>(Coordinator->TransferRevision()); PortalIndex = Coordinator->BodyEndpoint();
			Sample->Facts = Coordinator->PendingFacts(); Sample->Relation = Coordinator->Hold();
			Sample->Transfers = Transfers; Sample->Published = Read(true);
		}
		virtual void OnParticleUnregistered_Internal(TArray<TTuple<Chaos::FUniqueIdx, Chaos::FSingleParticlePhysicsProxy*>>& Proxies) override
		{
			for (const auto& Item : Proxies)
			{
				const auto* Proxy = Item.Get<1>();
				if (Session->ReferencesProxy(Proxy))
				{
					UnregistrationObserved = true; RetireBinding();
				}
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
		bool HoldEnabled = false;
		FHoldCommand Hold;
		FPhysicsStepKey LastConsumed;
		TArray<FSpikeSample> Samples;
		bool ReceiptsValid = true;
		TUniquePtr<FTransferFactCursor> FactCursor;
		bool FactsValid = true;
		bool DelayFacts = false;
		FVector SceneGravity = FVector::ZeroVector;
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
				Command);
			FactCursor = MakeUnique<FTransferFactCursor>(Command.Traveller.Handle,Callback->Epoch,Callback->Epoch);
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
				auto* Input = Callback->GetProducerInputData_External();
				Input->Command = Command; Input->HoldEnabled = HoldEnabled; Input->Hold = Hold;
				Input->TransferAck = FactCursor->Acknowledgment();
			}
			Scene->SetUpForFrame(&SceneGravity, 1.f/60, 0, 1.f/60, 1.f/120, 2, Substeps);
			Scene->StartFrame(); Scene->WaitPhysScenes(); Scene->EndFrame();
			while (auto Output = Callback->PopOutputData_External())
			{
				for (const auto& S : Output->Samples)
				{
					ReceiptsValid &= CanConsumeReceipt(S.Receipt, Command.Traveller.Handle, Command.PairGeneration,
						Command.Revision, Callback->Epoch, LastConsumed);
					LastConsumed = S.Receipt.Step; Samples.Add(S);
					if (!DelayFacts) { ConsumeFacts(S.Facts); }
				}
			}
		}
		void FlushRegistration()
		{
			Scene->SetUpForFrame(&FVector::ZeroVector, 0, 0, 1.f/60, 1.f/120, 2, false);
			Scene->StartFrame(); Scene->WaitPhysScenes(); Scene->EndFrame();
		}
		FWorldPassageRequest PassageRequest() const
		{
			FWorldPassageRequest R;
			R.World = World; R.Body = Body; R.EntrySupport = EntryWall; R.ExitSupport = ExitWall;
			R.Traveller = Command.Traveller; R.PairGeneration = Command.PairGeneration; R.Revision = NextRevision;
			R.Entry = Command.Entry; R.Exit = Command.Exit;
			R.From = Body->GetComponentTransform(); R.To = R.From; R.To.AddToTranslation(FVector(-2,0,0));
			return R;
		}
		void InstallNativeTravellerCap()
		{
			// Complete any pending body/filter rebuild before binding the native cap.
			FlushRegistration();
			check(FChaosTravellerSpeedCap::Install_GameThread(Body->BodyInstance,250));
			FlushRegistration();
		}
		void EnableNativeClearance(bool RemoveOtherBody = true)
		{
			if (RemoveOtherBody) { ProtectedBody->DestroyComponent(); ProtectedBody = nullptr; }
			InstallNativeTravellerCap();
			FPhysicsBindingRequest Request;
			Request.World = World; Request.Body = Body; Request.Supports[0] = EntryWall; Request.Supports[1] = ExitWall;
			Request.Registry = &Registry; Request.Command = Command; Request.Command.Revision = NextRevision;
			Request.EndpointApertures[0] = FVector2D(Command.HalfWidth,Command.HalfHeight);
			Request.EndpointApertures[1] = Request.EndpointApertures[0];
			FPreparedPhysicsBinding Prepared;
			check(PreparePhysicsBinding_GameThread(Request,Prepared) == EPhysicsBindingResult::Ready);
			Command = Prepared.Command;
			Callback->ConfigureClearanceBeforeDispatch(Prepared);
			// Authoritative proof must replace these authored fixture claims at every stage.
			Command.IsolatedStaticScope = false; Command.ExitCorridorCertified = false;
		}
		void ConsumeFacts(const TArray<FTransferFact>& Facts)
		{
			for (const auto& Fact : Facts)
			{
				const auto R = FactCursor->Consume(Fact);
				FactsValid &= R == EFactResult::Consumed || R == EFactResult::Duplicate;
			}
		}
	};
	struct FNativeSleepingContacts
	{
		FNativeScene Scene;
		UBoxComponent* Floor = nullptr;
		TArray<UBoxComponent*> Members;
		bool NativeSleepingIsland = false;
		int32 NativeIslandSize = 0, NativeMidphases = 0;
		FNativeSleepingContacts(bool Substeps, bool Stack, bool AtExit = false, bool KinematicFloor = false)
			: Scene(Substeps,true,false,false)
		{
			Scene.ProtectedBody->DestroyComponent(); Scene.ProtectedBody = nullptr;
			Scene.Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Scene.SceneGravity = FVector(0,0,-980);
			const FVector Origin = AtExit ? FVector(1013,0,0) : FVector(1500,400,0);
			const double Extent = AtExit ? .25 : 5.;
			Floor = Scene.Box(Origin-FVector(0,0,Extent+.5),FVector(25,25,1),KinematicFloor);
			if (KinematicFloor) { Floor->SetSimulatePhysics(false); }
			// The floor contacts furniture but not the traveller. Its pose must still
			// participate in the native contact lease, even outside traveller queries.
			Floor->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Ignore);
			Members.Add(Scene.Box(Origin,FVector(Extent),true));
			if (Stack) { Members.Add(Scene.Box(Origin+FVector(0,0,9),FVector(Extent),true)); }
			for (auto* Member : Members)
			{ Member->SetCollisionObjectType(ECC_WorldDynamic); Member->SetEnableGravity(true); }
			for (int32 I=0; I<8; ++I) { Scene.Advance(Substeps); }
			for (auto* Member : Members) { Member->PutAllRigidBodiesToSleep(); }
			for (int32 I=0; I<2; ++I) { Scene.Advance(Substeps); }
			auto* Proxy = Members[0]->BodyInstance.GetPhysicsActor();
			Scene.Scene->GetSolver()->EnqueueCommandImmediate([this,Proxy]()
			{
				const auto* P = Proxy->GetHandle_LowLevel();
				const auto* Island = Scene.Scene->GetSolver()->GetEvolution()->GetIslandManager().GetParticleIsland(P);
				NativeSleepingIsland = Island && Island->IsSleeping();
				NativeIslandSize = Island ? Island->GetNumParticles() : 0;
				NativeMidphases = P->ParticleCollisions().Num();
			});
			Scene.FlushRegistration(); Scene.Samples.Reset();
			Scene.Command.PermitSupportBypass = true; Scene.Command.PermitTransfer = true;
			Scene.EnableNativeClearance(false);
			Scene.Body->SetPhysicsLinearVelocity(FVector(-120,0,0));
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

namespace
{
	FHoldCommand MakeHold(const FTravellerSnapshot& Traveller, const FTransform& Desired)
	{
		FHoldCommand C; C.Traveller = Traveller; C.DesiredHolderPose = Desired;
		C.Route.Revision = 1; C.Region.Revision = 1; C.Region.CertifiedStaticCoverage = true;
		C.Region.Planes.Add({1, FVector::ForwardVector, FVector(2,0,0)});
		return C;
	}
	void BeginHold(FNativeScene& F, const FTransform& Desired)
	{
		F.HoldEnabled = true; F.Hold = MakeHold(F.Command.Traveller, Desired);
		F.Callback->ConfigureHoldBeforeDispatch(1, 1);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalHoldGeometryTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsHold.GeometryAndRoutes", PortalBoundaryTestFlags)
bool FPortalHoldGeometryTest::RunTest(const FString& Parameters)
{
	FTravellerSnapshot Traveller; Traveller.Handle = {1,2,3};
	FBoundaryState Body; Body.Pose.SetLocation(FVector(80,0,0));
	const auto CheckShape = [&](const FPrimitive& Shape, double ExpectedX, const TCHAR* Message)
	{
		Traveller.Geometry.Primitives = {Shape};
		auto C = MakeHold(Traveller, FTransform(Body.Pose.GetRotation(), FVector(-30,20,0)));
		const auto Target = SolveHoldTarget(C,Body,Traveller.Handle,1,1,1);
		TestTrue(Message, Target.Usable() && FMath::IsNearlyEqual(Target.Pose.GetLocation().X,ExpectedX,1.e-6)
			&& FMath::IsNearlyEqual(Target.Pose.GetLocation().Y,20.,1.e-6));
	};
	FPrimitive Sphere; Sphere.Radius = 2; Sphere.Center = FVector(3,0,0);
	CheckShape(Sphere,1.5,TEXT("Offset sphere support retains tangential target"));
	FPrimitive Capsule; Capsule.Shape = EShape::Capsule; Capsule.Radius = 2; Capsule.HalfSegment = FVector(20,0,0);
	CheckShape(Capsule,24.5,TEXT("Capsule span is used rather than a fixed holding radius"));
	FPrimitive Box; Box.Shape = EShape::Box;
	for (int32 X : {-1,1}) for (int32 Y : {-1,1}) for (int32 Z : {-1,1}) { Box.Vertices.Add(FVector(20*X,3*Y,4*Z)); }
	Body.Pose.SetRotation(FQuat(FVector::UpVector,UE_PI/2));
	CheckShape(Box,5.5,TEXT("Rotated long box uses oriented collision support"));
	Body.Pose.SetRotation(FQuat::Identity);
	FPrimitive Convex; Convex.Shape = EShape::Convex; Convex.Vertices = {FVector(-7,0,0),FVector(2,0,0),FVector(0,4,0),FVector(0,0,4)};
	CheckShape(Convex,9.5,TEXT("Authored convex vertices constrain target"));
	Traveller.Geometry.Primitives = {Box,Sphere};
	auto C = MakeHold(Traveller,FTransform(FVector(-30,20,0)));
	TestTrue(TEXT("Compound takes support of all primitives"), FMath::IsNearlyEqual(
		SolveHoldTarget(C,Body,Traveller.Handle,1,1,1).Pose.GetLocation().X,22.5,1.e-6));
	Body.Pose.SetLocation(FVector(23,0,0)); C.DesiredHolderPose.SetRotation(FQuat(FVector::UpVector,UE_PI/2));
	const auto Limited = SolveHoldTarget(C,Body,Traveller.Handle,1,1,1);
	TestTrue(TEXT("Unsafe intermediate rotation retains orientation and tangential translation"),
		Limited.Usable() && Limited.RotationProjected && Limited.Pose.GetRotation().Equals(Body.Pose.GetRotation(),1.e-6)
		&& Limited.Pose.GetLocation().Y == 20);
	Body.Pose.SetLocation(FVector(21,0,0));
	TestTrue(TEXT("Initial overlap does not silently manufacture a safe pose"),
		SolveHoldTarget(C,Body,Traveller.Handle,1,1,1).Reason == EHoldReason::InitialOverlap);
	Body.Pose.SetLocation(FVector(80,0,0));
	C.Region.Planes.Add({2,FVector::RightVector,FVector(0,5,0)});
	TestTrue(TEXT("Every plane validates the initial full shape"),
		SolveHoldTarget(C,Body,Traveller.Handle,1,1,1).Reason == EHoldReason::InitialOverlap);
	C.Region.Planes.Reset(); C.Route.Kind = EHoldRoute::SinglePair; C.Route.PairGeneration = 1;
	C.Route.BodySide = FTransform(FVector(1000,0,0)); C.DesiredHolderPose = FTransform(FVector(10,20,0));
	Body.Pose.SetLocation(FVector(990,-20,0));
	const auto Remote = SolveHoldTarget(C,Body,Traveller.Handle,1,1,1);
	// The canonical rigid half-turn has exact axial signs and no float-angle drift.
	const FVector ExpectedRemote(990,-20,0);
	TestTrue(TEXT("Explicit single-pair route maps position and orientation into body space"), Remote.Usable()
		&& Remote.Pose.GetLocation().Equals(ExpectedRemote,1.e-6)
		&& Remote.Pose.GetRotation().Equals(InteriorPortalMath::Rotation(C.Route.HolderSide,C.Route.BodySide),1.e-6));
	TestTrue(TEXT("Stale portal pair rejected"),SolveHoldTarget(C,Body,Traveller.Handle,2,1,1).Reason == EHoldReason::StaleIdentity);
	TestTrue(TEXT("Stale route revision rejected"),SolveHoldTarget(C,Body,Traveller.Handle,1,2,1).Reason == EHoldReason::StaleIdentity);
	TestTrue(TEXT("Stale region revision rejected"),SolveHoldTarget(C,Body,Traveller.Handle,1,1,2).Reason == EHoldReason::StaleIdentity);
	TestTrue(TEXT("Stale body rejected"),SolveHoldTarget(C,Body,{1,2,4},1,1,1).Reason == EHoldReason::StaleIdentity);
	C.Route.Kind = EHoldRoute::Unsupported;
	TestTrue(TEXT("Multiple hops have an explicit rejection"),SolveHoldTarget(C,Body,Traveller.Handle,1,1,1).Reason == EHoldReason::UnsupportedRoute);
	C.Route.Kind = EHoldRoute::Direct; C.Region.CertifiedStaticCoverage = false;
	TestTrue(TEXT("Unverified world hits are not a free-space certificate"),SolveHoldTarget(C,Body,Traveller.Handle,1,1,1).Reason == EHoldReason::UncertifiedRegion);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalHoldWrenchTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsHold.AnchorAndBounds", PortalBoundaryTestFlags)
bool FPortalHoldWrenchTest::RunTest(const FString& Parameters)
{
	FTravellerSnapshot Traveller; Traveller.Handle = {1,2,3}; FPrimitive Sphere; Sphere.Radius = 5;
	Traveller.Geometry.Primitives = {Sphere};
	FHoldPhysicalState State; State.Body.Pose.SetLocation(FVector(80,0,0)); State.MassKg = 1; State.LocalInertia = FVector(10);
	auto C = MakeHold(Traveller,FTransform(FVector(90,0,0))); C.LocalGrabAnchor = FVector(0,3,0);
	C.Profile.AngularStiffness = 0; C.Profile.AngularDamping = 0; C.Profile.MaxForce = 100; C.Profile.MaxTorque = 40;
	const FPhysicsStepKey Step {1,1,1,1./60};
	const auto Drive = EvaluateHoldDrive(C,State,Step,Traveller.Handle,1,1,1);
	TestTrue(TEXT("Off-center force contributes torque before total torque cap"),Drive.Target.Usable()
		&& Drive.Force.Equals(FVector(100,0,0),1.e-6) && Drive.Torque.Equals(FVector(0,0,-40),1.e-6));
	C.DesiredHolderPose = State.Body.Pose; State.Body.AngularVelocity = FVector(0,0,2);
	const auto Damped = EvaluateHoldDrive(C,State,Step,Traveller.Handle,1,1,1);
	TestTrue(TEXT("Damping uses anchor velocity including angular motion"), Damped.Force.X > 0 && Damped.Torque.Z < 0);
	C.DesiredHolderPose.SetLocation(FVector(1000,0,0));
	const auto Released = EvaluateHoldDrive(C,State,Step,Traveller.Handle,1,1,1);
	TestTrue(TEXT("Excessive raw error requests release without force or torque"), Released.Target.Reason == EHoldReason::ExcessiveError
		&& Released.Force.IsZero() && Released.Torque.IsZero());
	C.DesiredHolderPose = State.Body.Pose; C.Profile.MaxTorque = -1;
	TestFalse(TEXT("Invalid drive profile cannot produce a wrench"), EvaluateHoldDrive(C,State,Step,Traveller.Handle,1,1,1).Target.Usable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalNativeHoldBlockedTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsHold.NativeBlockedTargets", PortalBoundaryTestFlags)
bool FPortalNativeHoldBlockedTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (double Mass : {1.,20.})
	{
		FNativeScene F(Substeps,true,false); F.Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		F.Body->SetMassOverrideInKg(NAME_None,Mass,true);
		BeginHold(F,FTransform(FQuat(FVector::UpVector,UE_PI/2),FVector(-50,20,0)));
		for (int32 I = 0; I < 240; ++I) { F.Advance(Substeps); }
		bool Bounded = true, Safe = true, RotationLimited = true;
		for (const auto& S : F.Samples)
		{
			Bounded &= S.AppliedForce.Size() <= 1200 + 1.e-5 && S.AppliedTorque.Size() <= 1200 + 1.e-5;
			Safe &= S.DisabledPairs == 0 && S.Transfers == 0 && S.Published.Pose.GetLocation().X >= 6.8;
			RotationLimited &= S.HoldTarget.Usable() && S.HoldTarget.RotationProjected;
		}
		const double Error = FVector::Dist(F.Body->GetComponentLocation(),FVector(7.5,20,0));
		const double Speed = F.Body->GetPhysicsLinearVelocity().Size();
		TestTrue(TEXT("Actual light/heavy drive respects wrench caps every interval"),Bounded);
		TestTrue(TEXT("Blocked holding retains collisions without recovery or transfer"),Safe && F.ReceiptsValid);
		TestTrue(TEXT("Near-wall target rotation is explicitly projected throughout"),RotationLimited);
		TestTrue(TEXT("Blocked tangential target settles within declared error/speed"),Error <= 2 && Speed <= 3);
		AddInfo(FString::Printf(TEXT("Hold blocked substeps=%d mass=%.3f samples=%d error=%.6f speed=%.6f x=%.6f"),
			Substeps,F.Body->GetMass(),F.Samples.Num(),Error,Speed,F.Body->GetComponentLocation().X));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalNativeHoldRotationTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsHold.NativeRotationAndEnergy", PortalBoundaryTestFlags)
bool FPortalNativeHoldRotationTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true})
	{
		FNativeScene F(Substeps,true,false); F.Body->SetWorldLocation(FVector(70,0,0));
		F.Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		const FQuat Desired(FVector::ForwardVector,UE_PI/2);
		BeginHold(F,FTransform(Desired,FVector(75,10,0)));
		for (int32 I = 0; I < 240; ++I) { F.Advance(Substeps); }
		bool Bounds = true, Impulses = true, Energy = true, Targets = true;
		const double Mass = F.Body->GetMass();
		for (const auto& S : F.Samples)
		{
			const double Dt = S.Receipt.Step.DeltaSeconds;
			Bounds &= S.AppliedForce.Size() <= 1200 + 1.e-5 && S.AppliedTorque.Size() <= 1200 + 1.e-5;
			Targets &= S.HoldTarget.Usable() && !S.HoldTarget.RotationProjected;
			Impulses &= (S.Integrated.LinearVelocity-S.Before.LinearVelocity).Equals(S.AppliedForce/Mass*Dt,1.e-4)
				&& (S.Integrated.AngularVelocity-S.Before.AngularVelocity).Equals(S.AppliedTorque/S.LocalInertia.X*Dt,1.e-4);
			const double StartE = .5*Mass*S.Before.LinearVelocity.SizeSquared() + .5*S.LocalInertia.X*S.Before.AngularVelocity.SizeSquared();
			const double EndE = .5*Mass*S.Integrated.LinearVelocity.SizeSquared() + .5*S.LocalInertia.X*S.Integrated.AngularVelocity.SizeSquared();
			const double Work = Dt*(FVector::DotProduct(S.AppliedForce,S.Before.LinearVelocity)+FVector::DotProduct(S.AppliedTorque,S.Before.AngularVelocity));
			const double ImpulseE = .5*Dt*Dt*(S.AppliedForce.SizeSquared()/Mass+S.AppliedTorque.SizeSquared()/S.LocalInertia.X);
			Energy &= EndE <= StartE + Work + ImpulseE + .02;
		}
		const double ErrorDegrees = FMath::RadiansToDegrees(F.Body->GetComponentQuat().AngularDistance(Desired));
		const double AngularSpeed = F.Body->GetPhysicsAngularVelocityInRadians().Size();
		TestTrue(TEXT("Actual drive norm caps and feasible rotation hold every interval"),Bounds && Targets);
		TestTrue(TEXT("Chaos linear and angular impulse use actual mass and inertia"),Impulses);
		TestTrue(TEXT("Free-step energy is bounded by actual wrench work and impulse energy"),Energy);
		TestTrue(TEXT("Free rotation settles within declared angle and speed"),ErrorDegrees <= 3 && AngularSpeed <= .1);
		TestTrue(TEXT("Free positional target settles without bypass or transfer"),FVector::Dist(F.Body->GetComponentLocation(),FVector(75,10,0)) <= 2 && F.Samples.Last().Transfers == 0);
		AddInfo(FString::Printf(TEXT("Hold rotation substeps=%d samples=%d angleError=%.6f angularSpeed=%.6f"),Substeps,F.Samples.Num(),ErrorDegrees,AngularSpeed));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalNativeCoordinatorHeldTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsCoordinator.NativeHeldAndFree", PortalBoundaryTestFlags)
bool FPortalNativeCoordinatorHeldTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (bool Held : {false,true})
	{
		FNativeScene F(Substeps,true,true,true);
		if (Held)
		{
			BeginHold(F,FTransform(FVector(-40,0,0)));
			// The authored corridor has no blockers except the pair handled by the
			// exact-contact coordinator. This is not a production world certificate.
			F.Hold.Region.Planes.Reset();
			TestTrue(TEXT("Native fixture installs explicit association before dispatch"),
				F.Callback->ConfigureRelationBeforeDispatch(F.Hold.DesiredHolderPose,F.Hold.LocalGrabAnchor));
		}
		for (int32 I=0; I<24; ++I) { F.Advance(Substeps); }
		int32 Commits = 0; bool Materials = true, RouteReady = true, DriveAfterTransfer = false;
		for (const auto& S : F.Samples)
		{
			if (S.Decision.TransferAfterSolve) { ++Commits; }
			Materials &= S.MaterialPreserved;
			if (Held && S.Transfers)
			{
				RouteReady &= S.Relation.Active && S.Relation.Route.Kind == EHoldRoute::SinglePair;
				if (S.Before.Pose.GetLocation().X > 1000)
				{
					DriveAfterTransfer |= !S.AppliedForce.IsNearlyZero();
					RouteReady &= S.HoldTarget.Usable() && S.HoldTarget.Pose.GetLocation().Equals(FVector(1040,0,0),1.e-5)
						&& S.AppliedForce.Size() <= 1200 + 1.e-5 && S.AppliedTorque.Size() <= 1200 + 1.e-5;
				}
			}
		}
		TestTrue(TEXT("Held/free traversal shares exactly one actual coordinator commit"),Commits == 1 && F.Samples.Last().Transfers == 1);
		TestTrue(TEXT("Native adapter preserves actual mass/inertia/COM/sleep state"),Materials);
		TestTrue(TEXT("Facts use coherent lifetime/step domains and are acknowledged"),F.FactsValid && F.FactCursor->LastRevision() == 1 && F.Samples.Last().Facts.IsEmpty());
		if (Held) { TestTrue(TEXT("Next physics drive consumes rebound route and mapped target"),RouteReady && DriveAfterTransfer); }
		AddInfo(FString::Printf(TEXT("Coordinator native substeps=%d held=%d samples=%d transfers=%d factRevision=%llu x=%.6f"),
			Substeps,Held,F.Samples.Num(),F.Samples.Last().Transfers,F.FactCursor->LastRevision(),F.Body->GetComponentLocation().X));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalNativeCoordinatorDelayedTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsCoordinator.NativeDelayedFacts", PortalBoundaryTestFlags)
bool FPortalNativeCoordinatorDelayedTest::RunTest(const FString& Parameters)
{
	FNativeScene F(true,true,true,true); F.DelayFacts = true;
	for (int32 I=0; I<12; ++I) { F.Advance(true); }
	TestTrue(TEXT("Committed fact survives many newer intent commands without acknowledgment"),
		F.Command.Revision >= 12 && F.Samples.Last().Facts.Num() == 1 && F.FactCursor->LastRevision() == 0);
	F.ConsumeFacts(F.Samples.Last().Facts); F.ConsumeFacts(F.Samples.Last().Facts);
	TestTrue(TEXT("Delayed fact consumed once, duplicate delivery harmless"),F.FactsValid && F.FactCursor->LastRevision() == 1);
	F.Advance(true);
	TestTrue(TEXT("Acknowledgment reaches solver boundary and retires journal prefix"),F.Samples.Last().Facts.IsEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalNativeSessionHandoffTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.RetiredFactHandoff", PortalBoundaryTestFlags)
bool FPortalNativeSessionHandoffTest::RunTest(const FString& Parameters)
{
	FNativeScene F(true,true,true,true); F.DelayFacts = true; F.EnableNativeClearance();
	for (int32 I=0; I<12; ++I) { F.Advance(true); }
	TestTrue(TEXT("A native commit is still pending when topology is cancelled"),
		F.Samples.Last().Transfers == 1 && F.FactCursor->LastRevision() == 0
		&& F.Samples.Last().Facts.Num() == 1);
	const auto OriginalBody = F.Command.Traveller.Handle;
	const uint64 OriginalPair = F.Command.PairGeneration;
	++F.Command.PairGeneration; F.Advance(true);
	const auto Handoff = F.Callback->LastHandoff;
	TestTrue(TEXT("One session retires clearance and transfer writes before stale pair input can act"),
		F.Samples.Last().Retired && !F.Samples.Last().Decision.BypassSupport
		&& Handoff.Body == OriginalBody && Handoff.PendingFacts.Num() == 1
		&& Handoff.PendingFacts[0].PairGeneration == OriginalPair);
	F.ConsumeFacts(Handoff.PendingFacts); F.ConsumeFacts(Handoff.PendingFacts);
	TestTrue(TEXT("Retired-session handoff can be consumed once without replaying body pose"),
		F.FactsValid && F.FactCursor->LastRevision() == 1 && F.Samples.Last().Transfers == 1);
	F.Advance(true);
	TestTrue(TEXT("Acknowledgment drains the retired journal without reviving the binding"),
		F.Callback->LastHandoff.PendingFacts.IsEmpty() && F.Samples.Last().Retired
		&& !F.Samples.Last().Decision.BypassSupport);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalWorldPassageVolumeTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsWorldQuery.VolumesAndClearance", PortalBoundaryTestFlags)
bool FPortalWorldPassageVolumeTest::RunTest(const FString& Parameters)
{
	FNativeScene F(false,true,false); F.FlushRegistration();
	auto R = F.PassageRequest();
	const auto Clear = ObserveWorldPassage(R,F.Registry);
	TestTrue(TEXT("Actual world compound-volume path observes empty legal openings"),Clear.Result == EWorldPassageResult::ClearAtQuery);
	TestTrue(TEXT("Support depth comes from baked collision shape"),Clear.EntryOutwardDepthCm == 2 && Clear.ExitOutwardDepthCm == 2);
	TestTrue(TEXT("Full exit clearance precedes permission, beyond mapped next interval"),Clear.DestinationClear.GetLocation().X > 1009);
	TestTrue(TEXT("A probe cannot move the held body or publish a transfer"),F.Body->GetComponentLocation().X == 8 && F.Samples.IsEmpty());
	auto* Blocker = F.Box(FVector(1013,0,0),FVector(.25),false); F.FlushRegistration();
	FCollisionQueryParams Params; Params.AddIgnoredComponent(F.Body); Params.AddIgnoredComponent(F.ExitWall);
	TestFalse(TEXT("Center ray misses the blocker beyond its endpoint"),F.World->LineTraceTestByChannel(
		Clear.DestinationAtPlane.GetLocation(),Clear.DestinationClear.GetLocation(),F.Body->GetCollisionObjectType(),Params));
	TestTrue(TEXT("Full-volume exit sweep detects blocker invisible to center ray and mapped next endpoint"),
		ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::DestinationBlocked);
	Blocker->SetCollisionResponseToChannel(F.Body->GetCollisionObjectType(),ECR_Ignore); F.FlushRegistration();
	TestTrue(TEXT("Actual collision responses govern blocking"),ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::ClearAtQuery);
	Blocker->SetCollisionResponseToChannel(F.Body->GetCollisionObjectType(),ECR_Block);
	Blocker->SetMobility(EComponentMobility::Movable); Blocker->SetSimulatePhysics(true); F.FlushRegistration();
	TestTrue(TEXT("Currently observed dynamic obstacle blocks; no future-motion certificate inferred"),
		ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::DestinationBlocked);
	Blocker->DestroyComponent(); F.FlushRegistration();
	auto* LocalBlocker = F.Box(FVector(8,0,0),FVector(1),false); F.FlushRegistration();
	TestTrue(TEXT("Initial source overlap blocks before a sweep"),ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::SourceBlocked);
	LocalBlocker->DestroyComponent(); F.FlushRegistration();
	auto* Sibling = NewObject<UBoxComponent>(F.Body->GetOwner()); F.Body->GetOwner()->AddInstanceComponent(Sibling);
	Sibling->SetBoxExtent(FVector(1)); Sibling->SetCollisionProfileName(TEXT("PhysicsActor"));
	Sibling->SetWorldLocation(FVector(8,0,0)); Sibling->RegisterComponent(); F.FlushRegistration();
	TestTrue(TEXT("Ignoring selected component preserves blocking by sibling on same owner"),
		ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::SourceBlocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalWorldPassagePrimitiveTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsWorldQuery.PrimitivesAndCompound", PortalBoundaryTestFlags)
bool FPortalWorldPassagePrimitiveTest::RunTest(const FString& Parameters)
{
	FNativeScene F(false,true,false);
	const auto Register = [&](UPrimitiveComponent* Shape, const FQuat& Rotation)
	{
		Shape->GetOwner()->SetRootComponent(Shape); Shape->GetOwner()->AddInstanceComponent(Shape);
		Shape->SetCollisionProfileName(TEXT("PhysicsActor")); Shape->SetEnableGravity(false);
		Shape->SetWorldTransform(FTransform(Rotation,FVector(8,0,0))); Shape->RegisterComponent(); Shape->SetSimulatePhysics(true);
		EGeometryResult Reason;
		TestTrue(TEXT("Actual shape registers as an independently simulated traveller"),F.Registry.Register(Shape,Reason));
		F.FlushRegistration();
		auto R = F.PassageRequest(); R.Body = Shape; F.Registry.Capture(Shape,R.Traveller);
		R.From = Shape->GetComponentTransform(); R.To = R.From; R.To.AddToTranslation(FVector(-2,0,0));
		return R;
	};
	// Move the fixture's selected body out of the observed paths, without ever stepping it.
	F.Body->SetWorldLocation(FVector(100,70,0));
	auto* Sphere = NewObject<USphereComponent>(F.World->SpawnActor<AActor>()); Sphere->SetSphereRadius(2);
	auto R = Register(Sphere,FQuat::Identity);
	TestTrue(TEXT("Sphere volume has its actual radius and clears legal openings"),ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::ClearAtQuery);
	Sphere->DestroyComponent();
	auto* Capsule = NewObject<UCapsuleComponent>(F.World->SpawnActor<AActor>()); Capsule->SetCapsuleSize(2,12);
	R = Register(Capsule,FQuat(FVector::ForwardVector,UE_PI/2));
	auto* Blocker = F.Box(FVector(1001,10,0),FVector(.25),false); F.FlushRegistration();
	TestTrue(TEXT("Rotated capsule distal spine blocks though center path is clear"),ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::DestinationBlocked);
	Blocker->DestroyComponent(); Capsule->DestroyComponent();
	auto* Compound = NewObject<UStaticMeshComponent>(F.World->SpawnActor<AActor>());
	auto* Mesh = NewObject<UStaticMesh>(); Mesh->CreateBodySetup();
	auto* Setup = Mesh->GetBodySetup(); Setup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	FKSphereElem Center; Center.Radius = 2; Setup->AggGeom.SphereElems.Add(Center);
	FKSphereElem Offset = Center; Offset.Center = FVector(0,30,0); Setup->AggGeom.SphereElems.Add(Offset);
	Compound->SetStaticMesh(Mesh); R = Register(Compound,FQuat::Identity);
	TestTrue(TEXT("All compound elements are captured in collision space"),R.Traveller.Geometry.Primitives.Num() == 2);
	TestTrue(TEXT("Offset compound body clears when both element corridors are empty"),ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::ClearAtQuery);
	Blocker = F.Box(FVector(1001,-30,0),FVector(.25),false); F.FlushRegistration();
	const auto O = ObserveWorldPassage(R,F.Registry);
	TestTrue(TEXT("Mapped offset element blocks and identifies the distal primitive"),O.Result == EWorldPassageResult::DestinationBlocked && O.BlockingPrimitive == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalWorldPassageValidityTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsWorldQuery.IdentityAndSupport", PortalBoundaryTestFlags)
bool FPortalWorldPassageValidityTest::RunTest(const FString& Parameters)
{
	FNativeScene F(false,true,false); F.FlushRegistration();
	auto R = F.PassageRequest(); R.ExitHalfWidth = 3;
	TestTrue(TEXT("Destination containment checked separately before support exclusion"),
		ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::OutsideAperture);
	R = F.PassageRequest(); R.To.SetRotation(FQuat(FVector::ForwardVector,.1));
	TestTrue(TEXT("Rotation is explicitly unsupported rather than silently swept as translation"),
		ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::UnsupportedMotion);
	R = F.PassageRequest(); ++R.Traveller.Handle.Generation;
	TestTrue(TEXT("Mismatched registered body generation rejects"),ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::StaleTraveller);
	R = F.PassageRequest(); F.Body->SetBoxExtent(FVector(6));
	TestTrue(TEXT("Changed actual collision shape cannot reuse an old snapshot"),ObserveWorldPassage(R,F.Registry).Result == EWorldPassageResult::StaleTraveller);
	AActor* Owner = F.World->SpawnActor<AActor>();
	auto* Root = NewObject<USceneComponent>(Owner); Owner->SetRootComponent(Root); Owner->AddInstanceComponent(Root);
	Root->SetMobility(EComponentMobility::Static); Root->RegisterComponent();
	auto* Support = NewObject<UBoxComponent>(Owner); Owner->AddInstanceComponent(Support); Support->SetupAttachment(Root);
	Support->SetMobility(EComponentMobility::Static); Support->SetBoxExtent(FVector(2,100,150));
	Support->SetCollisionProfileName(TEXT("PhysicsActor")); Support->RegisterComponent();
	InteriorPortalPhysics::FGeometry Geometry;
	TestTrue(TEXT("Static support geometry accepts non-root collision component"),ExtractStaticSupportGeometry(Support,Geometry) == EGeometryResult::Fits);
	TestTrue(TEXT("Traveller root/unwelded restriction remains unchanged"),ExtractGeometry(Support,Geometry) == EGeometryResult::UnsupportedComponent);
	Support->SetMobility(EComponentMobility::Movable);
	TestTrue(TEXT("Moving support cannot masquerade as certified static support"),ExtractStaticSupportGeometry(Support,Geometry) == EGeometryResult::UnsupportedComponent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalWorldPassageCancellationTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsWorldQuery.NativeBlockedExitAndCancellation", PortalBoundaryTestFlags)
bool FPortalWorldPassageCancellationTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (bool PartialInsertion : {false,true})
	{
		FNativeScene F(Substeps,true,true,true);
		if (PartialInsertion) { for (int32 I=0; I<3; ++I) { F.Advance(Substeps); } }
		else { F.FlushRegistration(); }
		const double Before = F.Body->GetComponentLocation().X;
		F.Box(FVector(1013,0,0),FVector(.25),false); F.FlushRegistration();
		TestTrue(TEXT("Registration flush does not advance body or create a solver interval"),FMath::IsNearlyEqual(F.Body->GetComponentLocation().X,Before,1.e-6));
		const auto O = ObserveWorldPassage(F.PassageRequest(),F.Registry);
		TestTrue(TEXT("Actual exit obstruction is observed before next physics interval"),O.Result == EWorldPassageResult::DestinationBlocked);
		// Test-only bridge to the fixture's authored static protocol. This is not a production certificate.
		F.Command.PermitSupportBypass = false; F.Command.ExitCorridorCertified = false;
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		TestTrue(TEXT("Ordinary contacts restore blocking without test-side pose Recovery"),F.Body->GetComponentLocation().X > 6.8);
		TestTrue(TEXT("Cancellation publishes no transfer or durable fact"),F.Samples.Last().Transfers == 0 && F.FactCursor->LastRevision() == 0);
		AddInfo(FString::Printf(TEXT("World cancellation substeps=%d partial=%d beforeX=%.6f restoredX=%.6f transfers=%d"),
			Substeps,PartialInsertion,Before,F.Body->GetComponentLocation().X,F.Samples.Last().Transfers));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalNativeAdapterAtomicTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsWorldQuery.NativeAdapterAtomicity", PortalBoundaryTestFlags)
bool FPortalNativeAdapterAtomicTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true})
	{
		FNativeScene F(Substeps,true,true,true); F.Callback->ProbeAdapterRejections = true;
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		bool Atomic = true, Material = true; int32 Crossings = 0;
		for (const auto& S : F.Samples)
		{
			Atomic &= S.AdapterRejectionsAtomic; Material &= S.MaterialPreserved;
			if (S.Decision.TransferAfterSolve) { ++Crossings; }
		}
		TestTrue(TEXT("Wrong binding/step/solved state/material/nonfinite output and replay perform no write"),Atomic && Crossings == 1);
		TestTrue(TEXT("Valid fact commits once in same solved interval after rejected attempts"),F.Samples.Last().Transfers == 1 && F.FactCursor->LastRevision() == 1);
		TestTrue(TEXT("Native solved-state mapping preserves mass/inertia/COM/mass-frame/sleep"),Material);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverStaticCertificateTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.NativeStaticAndCertificate", PortalBoundaryTestFlags)
bool FPortalSolverStaticCertificateTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (bool Held : {false,true})
	{
		FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance(); F.Callback->ProbeClearanceProtocol = true;
		if (Held)
		{
			BeginHold(F,FTransform(FVector(-40,0,0))); F.Hold.Region.Planes.Reset();
			TestTrue(TEXT("Held/free native-certificate fixture uses the same coordinator relation"),
				F.Callback->ConfigureRelationBeforeDispatch(F.Hold.DesiredHolderPose,F.Hold.LocalGrabAnchor));
		}
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		bool Clear = true, Protocol = true, Materials = true; int32 Crossings = 0;
		for (const auto& S : F.Samples)
		{
			if (S.ClearanceScans != 3 || S.PreClearance != EStaticClearanceReason::Clear
				|| S.IntegratedClearance != EStaticClearanceReason::Clear || S.SolvedClearance != EStaticClearanceReason::Clear)
			{
				AddInfo(FString::Printf(TEXT("Certificate stage diagnostic step=%llu pre=%d integrated=%d solved=%d scans=%d retired=%d beforeX=%.6f integratedX=%.6f solvedX=%.6f"),
					S.Receipt.Step.EvolutionSerial,static_cast<int32>(S.PreClearance),static_cast<int32>(S.IntegratedClearance),
					static_cast<int32>(S.SolvedClearance),S.ClearanceScans,S.Retired,S.Before.Pose.GetLocation().X,
					S.Integrated.Pose.GetLocation().X,S.Solved.Pose.GetLocation().X));
			}
			Clear &= S.ClearanceScans == 3 && S.PreClearance == EStaticClearanceReason::Clear
				&& S.IntegratedClearance == EStaticClearanceReason::Clear && S.SolvedClearance == EStaticClearanceReason::Clear;
			Protocol &= S.ProofProtocolValid; Materials &= S.MaterialPreserved;
			if (S.Decision.TransferAfterSolve) { ++Crossings; }
		}
		TestTrue(TEXT("All three actual callback stages issue current native scene proofs"),Clear);
		TestTrue(TEXT("Wrong step/stage/command/sample, prior proof and duplicate consumption reject"),Protocol);
		TestTrue(TEXT("Without authored GT certificates native proof permits exactly one transfer"),
			Crossings == 1 && F.Samples.Last().Transfers == 1 && F.FactCursor->LastRevision() == 1);
		TestTrue(TEXT("Actual native transfer preserves materials and coherent facts"),Materials && F.FactsValid);
		TestTrue(TEXT("Certificates cover distinct one/two-substep intervals"),F.Samples.Num() == (Substeps ? 24 : 12) && F.ReceiptsValid);
		AddInfo(FString::Printf(TEXT("Native certificate substeps=%d held=%d samples=%d x=%.6f transfers=%d"),
			Substeps,Held,F.Samples.Num(),F.Body->GetComponentLocation().X,F.Samples.Last().Transfers));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverPhysicsOnlyTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.NativePhysicsOnlyAndCancellation", PortalBoundaryTestFlags)
bool FPortalSolverPhysicsOnlyTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (bool Partial : {false,true})
	{
		FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
		if (Partial) { for (int32 I=0; I<3; ++I) { F.Advance(Substeps); } }
		else { F.FlushRegistration(); }
		const double Before = F.Body->GetComponentLocation().X;
		if (Partial) { TestTrue(TEXT("The actual body reached partial insertion before obstruction"),FMath::IsNearlyEqual(Before,2.,1.e-6)); }
		auto* Blocker = F.Box(FVector(1013,0,0),FVector(.25),false);
		Blocker->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly); F.FlushRegistration();
		TestTrue(TEXT("GT observer does not see a query-disabled physics blocker"),ObserveWorldPassage(F.PassageRequest(),F.Registry).Result == EWorldPassageResult::ClearAtQuery);
		// Deliberately stale author claims cannot override the native scan.
		F.Command.ExitCorridorCertified = true; F.Command.IsolatedStaticScope = true;
		const int32 First = F.Samples.Num();
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		TestTrue(TEXT("PhysicsOnly exit obstacle is caught before the first affected integration"),
			F.Samples[First].PreClearance == EStaticClearanceReason::DestinationBlocked);
		TestTrue(TEXT("Restored ordinary contacts prevent traversal without GT pose Recovery"),F.Body->GetComponentLocation().X > 6.8);
		TestTrue(TEXT("Stale authored clear claim cannot commit or publish a transfer"),F.Samples.Last().Transfers == 0 && F.FactCursor->LastRevision() == 0);
		AddInfo(FString::Printf(TEXT("Native PhysicsOnly substeps=%d partial=%d beforeX=%.6f restoredX=%.6f transfers=%d"),
			Substeps,Partial,Before,F.Body->GetComponentLocation().X,F.Samples.Last().Transfers));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverGravityContactTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.GravityContactCancellation", PortalBoundaryTestFlags)
bool FPortalSolverGravityContactTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true})
	{
		FNativeScene F(Substeps,true,true,true);
		F.ProtectedBody->DestroyComponent(); F.ProtectedBody = nullptr;
		F.Body->SetEnableGravity(true);
		F.SceneGravity = FVector(0,0,-20000);
		F.Box(FVector(0,0,-7),FVector(50,50,1),false);
		F.FlushRegistration(); F.EnableNativeClearance(false);
		F.Advance(Substeps);
		const auto& S = F.Samples[0];
		AddInfo(FString::Printf(TEXT("Gravity contact substeps=%d pre=%d integrated=%d solved=%d disabled=%d unrelated=%d"),
			Substeps,static_cast<int32>(S.PreClearance),static_cast<int32>(S.IntegratedClearance),
			static_cast<int32>(S.SolvedClearance),S.DisabledPairs,S.UnrelatedPairs));
		TestTrue(TEXT("Gravity-reachable floor rejects at the first affected PreIntegrate"),
			S.PreClearance == EStaticClearanceReason::SourceBlocked);
		TestTrue(TEXT("The body actually integrates downward under native gravity"),
			S.Integrated.Pose.GetLocation().Z < S.Before.Pose.GetLocation().Z);
		bool NoBypassOrTransfer = F.FactCursor->LastRevision() == 0;
		for (const auto& Sample : F.Samples)
		{ NoBypassOrTransfer &= Sample.DisabledPairs == 0 && Sample.Transfers == 0; }
		TestTrue(TEXT("Gravity-driven contact cannot suppress the portal support pair or transfer"),NoBypassOrTransfer);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverGravityPartialCancellationTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.GravityPartialCancellation", PortalBoundaryTestFlags)
bool FPortalSolverGravityPartialCancellationTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (bool Partial : {false,true})
	{
		FNativeScene F(Substeps,true,true,true);
		F.Body->SetEnableGravity(true); F.EnableNativeClearance();
		if (Partial)
		{
			for (int32 I=0; I<3; ++I) { F.Advance(Substeps); }
			TestTrue(TEXT("Native body is partially inserted before gravity/floor obstruction"),
				FMath::IsNearlyEqual(F.Body->GetComponentLocation().X,2.,1.e-6));
		}
		const int32 First = F.Samples.Num();
		F.SceneGravity = FVector(0,0,-980);
		F.Box(FVector(0,0,-7),FVector(50,50,1),false);
		F.FlushRegistration();
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		const auto& FirstAffected = F.Samples[First];
		AddInfo(FString::Printf(TEXT("Ordinary gravity cancellation substeps=%d partial=%d pre=%d gate=%s restored=%s"),
			Substeps,Partial,static_cast<int32>(FirstAffected.PreClearance),
			*FirstAffected.GateImpulse.ToString(),*F.Body->GetComponentLocation().ToString()));
		TestTrue(TEXT("Ordinary gravity and reachable floor revoke before first affected integration"),
			FirstAffected.PreClearance == EStaticClearanceReason::SourceBlocked);
		TestTrue(TEXT("The first affected step really integrates normal gravity"),
			FirstAffected.Integrated.Pose.GetLocation().Z < FirstAffected.Before.Pose.GetLocation().Z);
		bool NoBypassOrFact = F.FactCursor->LastRevision() == 0;
		for (int32 I=First; I<F.Samples.Num(); ++I)
		{ NoBypassOrFact &= F.Samples[I].DisabledPairs == 0 && F.Samples[I].Transfers == 0; }
		TestTrue(TEXT("Cancellation keeps wall contacts and publishes no transfer"),NoBypassOrFact);
		TestTrue(TEXT("Wall and floor resolve inserted body without a game-thread pose rollback"),
			F.Body->GetComponentLocation().X > 6.8);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverContinuousGravityTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.ContinuousGravityPassage", PortalBoundaryTestFlags)
bool FPortalSolverContinuousGravityTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true})
	{
		FNativeScene F(Substeps,true,true,true);
		F.ProtectedBody->DestroyComponent(); F.ProtectedBody = nullptr;
		F.Body->SetEnableGravity(true);
		F.SceneGravity = FVector(0,0,-980);
		F.Command.MaxTranslationCm = 4.5;
		F.Box(FVector(0,0,-70),FVector(50,50,1),false);
		F.Box(FVector(1000,0,-70),FVector(50,50,1),false);
		F.EnableNativeClearance(false);
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		bool Clear = true;
		for (const auto& S : F.Samples)
		{ Clear &= S.PreClearance == EStaticClearanceReason::Clear
			&& S.IntegratedClearance == EStaticClearanceReason::Clear
			&& S.SolvedClearance == EStaticClearanceReason::Clear; }
		AddInfo(FString::Printf(TEXT("Continuous gravity substeps=%d first=%d/%d/%d z=%.3f transfers=%d"),
			Substeps,static_cast<int32>(F.Samples[0].PreClearance),
			static_cast<int32>(F.Samples[0].IntegratedClearance),
			static_cast<int32>(F.Samples[0].SolvedClearance),F.Body->GetComponentLocation().Z,
			F.Samples.Last().Transfers));
		TestTrue(TEXT("Enabled native gravity moves the traveller downward before contacts"),
			F.Samples[0].Integrated.Pose.GetLocation().Z < F.Samples[0].Before.Pose.GetLocation().Z);
		TestTrue(TEXT("Continuous gravity without reachable contacts stays certified across one portal transfer"),
			Clear && F.Samples.Last().Transfers == 1 && F.FactCursor->LastRevision() == 1 && F.FactsValid);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalPhysicsBindingBridgeTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.RealBindingObserveLifecycle", PortalBoundaryTestFlags)
bool FPortalPhysicsBindingBridgeTest::RunTest(const FString& Parameters)
{
	FNativeScene F(false,true,false);
	F.Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	FPhysicsBindingRequest R;
	R.World = F.World; R.Body = F.Body; R.Supports[0] = F.EntryWall; R.Supports[1] = F.ExitWall;
	R.Registry = &F.Registry; R.Command = F.Command; R.Command.Revision = 1;
	FPortalPhysicsBindingBridge Bridge;
	Bridge.Update_GameThread(&R);
	TestTrue(TEXT("Actual registered Chaos proxies acquire one read-only binding"),
		Bridge.HasLiveBinding() && Bridge.BoundPairGeneration() == 1);
	F.Advance(false);
	R.Command.Revision = 2;
	Bridge.Update_GameThread(&R);
	TestTrue(TEXT("Solver substep reaches the game-thread observer without a physics write"),
		Bridge.ObservedPhysicsSteps() > 0 && F.Body->GetPhysicsLinearVelocity().IsNearlyZero());
	R.Command.PairGeneration = 2; R.Command.Revision = 3;
	Bridge.Update_GameThread(&R);
	TestTrue(TEXT("Changed pair cannot replace the old PT binding before retirement"),
		Bridge.BoundPairGeneration() == 1);
	F.Advance(false);
	Bridge.Update_GameThread(&R);
	TestTrue(TEXT("Typed PT retirement permits a new pair binding"),
		Bridge.HasLiveBinding() && Bridge.BoundPairGeneration() == 2);
	Bridge.Update_GameThread(nullptr);
	F.Advance(false);
	Bridge.Update_GameThread(nullptr);
	TestFalse(TEXT("Cancellation returns a PT handoff and removes the callback"),Bridge.HasLiveBinding());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalPhysicsBindingPreparationTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.WorldBindingPreparation", PortalBoundaryTestFlags)
bool FPortalPhysicsBindingPreparationTest::RunTest(const FString& Parameters)
{
	FNativeScene F(true,true,true,true);
	FPhysicsBindingRequest R;
	R.World = F.World; R.Body = F.Body; R.Supports[0] = F.EntryWall; R.Supports[1] = F.ExitWall;
	R.Registry = &F.Registry; R.Command = F.Command; R.Command.Revision = 1;
	FPreparedPhysicsBinding Prepared;
	TestTrue(TEXT("Registered world components produce an unprivileged native binding snapshot"),
		PreparePhysicsBinding_GameThread(R,Prepared) == EPhysicsBindingResult::Ready
		&& Prepared.Command.Traveller.Handle == F.Command.Traveller.Handle
		&& Prepared.Command.SupportHalfThicknessCm >= 2
		&& !Prepared.Command.IsolatedStaticScope && !Prepared.Command.ExitCorridorCertified);
	R.EndpointApertures[1].X += 1;
	TestTrue(TEXT("Unequal endpoint apertures cannot be represented by this single-width certificate"),
		PreparePhysicsBinding_GameThread(R,Prepared) == EPhysicsBindingResult::IncompatibleAperture);
	R.EndpointApertures[1].X -= 1;
	F.ExitWall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Support without native collision cannot become a binding"),
		PreparePhysicsBinding_GameThread(R,Prepared) == EPhysicsBindingResult::UnsupportedSupport);
	F.ExitWall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	F.Body->SetBoxExtent(FVector(6));
	TestTrue(TEXT("Geometry changed after registry capture cannot become a binding"),
		PreparePhysicsBinding_GameThread(R,Prepared) == EPhysicsBindingResult::StaleTraveller);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverUnsupportedSceneTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.UnsupportedSceneAndMotion", PortalBoundaryTestFlags)
bool FPortalSolverUnsupportedSceneTest::RunTest(const FString& Parameters)
{
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance(false);
		auto* Unbounded = F.ProtectedBody->BodyInstance.GetPhysicsActor();
		F.Scene->GetSolver()->EnqueueCommandImmediate([Unbounded]()
		{ Unbounded->GetHandle_LowLevel()->CastToRigidParticle()->SetMaxLinearSpeedSq(TNumericLimits<float>::Max()); });
		F.FlushRegistration();
		F.Advance(true);
		TestTrue(TEXT("Interacting active body without a finite usable speed cap still rejects"),
			F.Samples[0].PreClearance == EStaticClearanceReason::UnsupportedScene);
		TestTrue(TEXT("Unsupported scene cannot bypass or transfer"),F.Samples.Last().DisabledPairs == 0 && F.Samples.Last().Transfers == 0);
	}
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance();
		F.Body->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Ignore);
		F.Advance(true);
		TestTrue(TEXT("GT-installed native cap survives ordinary collision-filter updates"),
			F.Samples[0].PreClearance == EStaticClearanceReason::Clear);
	}
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance();
		F.Body->RecreatePhysicsState(); F.FlushRegistration();
		const auto* Replacement = F.Body->BodyInstance.GetPhysicsActor();
		if (!Replacement) { AddError(TEXT("Physics-state recreation did not create a new traveller body")); return false; }
		double BeforeCap = 0, AfterCap = 0;
		F.Scene->GetSolver()->EnqueueCommandImmediate([Replacement,&BeforeCap]()
		{ BeforeCap = Replacement->GetHandle_LowLevel()->CastToRigidParticle()->MaxLinearSpeedSq(); });
		F.FlushRegistration();
		const bool Reinstalled = FChaosTravellerSpeedCap::Install_GameThread(F.Body->BodyInstance,250);
		F.FlushRegistration();
		F.Scene->GetSolver()->EnqueueCommandImmediate([Replacement,&AfterCap]()
		{ AfterCap = Replacement->GetHandle_LowLevel()->CastToRigidParticle()->MaxLinearSpeedSq(); });
		F.FlushRegistration(); F.Advance(true);
		TestTrue(TEXT("Recreated native body starts unbounded and needs a fresh GT cap install"),
			Reinstalled && BeforeCap > 250*250 && AfterCap == 250*250);
		TestTrue(TEXT("Old clearance binding retires on proxy replacement even after cap reinstall"),
			F.Samples[0].Retired && F.Samples[0].DisabledPairs == 0 && F.Samples.Last().Transfers == 0);
	}
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance();
		F.Body->SetPhysicsLinearVelocity(FVector(-120,1,0)); F.Advance(true);
		TestTrue(TEXT("Bounded lateral translation is certified when the full reach fits the portal"),
			F.Samples[0].PreClearance == EStaticClearanceReason::Clear
			&& F.Samples[0].IntegratedClearance == EStaticClearanceReason::Clear
			&& F.Samples[0].SolvedClearance == EStaticClearanceReason::Clear);
		TestTrue(TEXT("The solver integrates lateral motion without an early transfer"),
			F.Samples[0].Integrated.Pose.GetLocation().Y > F.Samples[0].Before.Pose.GetLocation().Y
			&& F.Samples.Last().Transfers == 0);
	}
	{
		FNativeScene F(true,true,true,true);
		F.Body->SetWorldLocation(FVector(8,54,0));
		F.EnableNativeClearance(); F.Advance(true);
		TestTrue(TEXT("Near-rim body fits now but its bounded next-step reach does not"),
			F.Samples[0].PreClearance == EStaticClearanceReason::OutsideAperture);
		TestTrue(TEXT("Uncertified rim reach keeps wall contacts and cannot transfer"),
			F.Samples[0].DisabledPairs == 0 && F.Samples.Last().Transfers == 0);
	}
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance();
		F.Body->SetPhysicsAngularVelocityInRadians(FVector(0,0,1)); F.Advance(true);
		TestTrue(TEXT("Spin cannot consume a fixed-orientation clearance certificate"),F.Samples[0].PreClearance == EStaticClearanceReason::UnsupportedMotion);
	}
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance();
		BeginHold(F,FTransform(FQuat(FVector::UpVector,UE_PI/2),FVector(-40,0,0))); F.Hold.Region.Planes.Reset();
		F.Advance(true);
		TestTrue(TEXT("Rotation drive actually applies a bounded nonzero torque from zero initial spin"),!F.Samples[0].AppliedTorque.IsNearlyZero());
		TestTrue(TEXT("Angular acceleration rejects before the body begins rotating"),F.Samples[0].PreClearance == EStaticClearanceReason::UnsupportedMotion);
		TestTrue(TEXT("Unsupported rotational drive cannot disable the support pair"),F.Samples[0].DisabledPairs == 0 && F.Samples.Last().Transfers == 0);
	}
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance();
		AActor* Owner = F.World->SpawnActor<AActor>(); auto* Joint = NewObject<UPhysicsConstraintComponent>(Owner);
		Owner->SetRootComponent(Joint); Owner->AddInstanceComponent(Joint);
		Joint->SetWorldLocation(F.Body->GetComponentLocation()); Joint->RegisterComponent();
		Joint->SetConstrainedComponents(F.Body,NAME_None,F.EntryWall,NAME_None); F.Advance(true);
		TestTrue(TEXT("Persistent world joint is rejected as outside independent-body profile"),F.Samples[0].PreClearance == EStaticClearanceReason::UnsupportedScene);
		TestTrue(TEXT("Constrained body cannot consume bypass or transfer permission"),F.Samples[0].DisabledPairs == 0 && F.Samples.Last().Transfers == 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverActiveIndependentTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.ActiveIndependentReach", PortalBoundaryTestFlags)
bool FPortalSolverActiveIndependentTest::RunTest(const FString& Parameters)
{
	const auto SetNativeCap = [](FNativeScene& F, UBoxComponent* Box, double SpeedCmPerSecond)
	{
		auto* Proxy = Box->BodyInstance.GetPhysicsActor();
		F.Scene->GetSolver()->EnqueueCommandImmediate([Proxy,SpeedCmPerSecond]()
		{
			if (auto* R = Proxy->GetHandle_LowLevel()->CastToRigidParticle())
			{ R->SetMaxLinearSpeedSq(SpeedCmPerSecond*SpeedCmPerSecond); }
		});
		F.FlushRegistration();
	};
	for (bool Substeps : {false,true}) for (bool Held : {false,true})
	{
		FNativeScene F(Substeps,true,true,true);
		F.ProtectedBody->DestroyComponent(); F.ProtectedBody = nullptr;
		auto* Remote = F.Box(FVector(1500,400,0),FVector(5),true);
		Remote->SetPhysicsLinearVelocity(FVector(100,0,0));
		SetNativeCap(F,Remote,250);
		F.EnableNativeClearance(false);
		if (Held)
		{
			BeginHold(F,FTransform(FVector(-40,0,0))); F.Hold.Region.Planes.Reset();
			TestTrue(TEXT("Remote active fixture binds the normal held route"),
				F.Callback->ConfigureRelationBeforeDispatch(F.Hold.DesiredHolderPose,F.Hold.LocalGrabAnchor));
		}
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		bool Clear = true;
		for (const auto& S : F.Samples)
		{ Clear &= S.PreClearance == EStaticClearanceReason::Clear
			&& S.IntegratedClearance == EStaticClearanceReason::Clear
			&& S.SolvedClearance == EStaticClearanceReason::Clear; }
		TestTrue(TEXT("Bounded independent remote motion stays certified at all native stages"),Clear);
		TestTrue(TEXT("Unrelated moving body does not prevent the single held/free transfer"),
			F.Samples.Last().Transfers == 1 && F.FactCursor->LastRevision() == 1 && F.FactsValid);
		TestTrue(TEXT("Remote body keeps native motion without a portal write"),
			Remote->GetComponentLocation().X > 1500 && Remote->GetPhysicsLinearVelocity().X > 0);
	}
	for (bool Substeps : {false,true})
	{
		FNativeScene F(Substeps,true,true,true);
		F.ProtectedBody->DestroyComponent(); F.ProtectedBody = nullptr;
		auto* Near = F.Box(FVector(1013,0,0),FVector(.25),true);
		SetNativeCap(F,Near,250);
		F.EnableNativeClearance(false); F.Advance(Substeps);
		TestTrue(TEXT("Capped active body whose reach meets the exit is still blocked"),
			F.Samples[0].PreClearance == EStaticClearanceReason::UnsupportedScene
				&& F.Samples.Last().Transfers == 0 && F.Samples[0].DisabledPairs == 0);
	}
	{
		FNativeScene F(true,true,true,true);
		F.ProtectedBody->DestroyComponent(); F.ProtectedBody = nullptr;
		auto* Remote = F.Box(FVector(1500,400,0),FVector(5),true);
		SetNativeCap(F,Remote,250);
		auto* Proxy = Remote->BodyInstance.GetPhysicsActor();
		F.EnableNativeClearance(false);
		F.Callback->AfterIntegrateMutation = [Proxy]()
		{ Proxy->GetHandle_LowLevel()->CastToRigidParticle()->SetMaxLinearSpeedSq(500*500); };
		F.Advance(true);
		TestTrue(TEXT("Changed native speed cap invalidates the first affected interval"),
			F.Samples[0].PreClearance == EStaticClearanceReason::Clear
				&& F.Samples[0].IntegratedClearance == EStaticClearanceReason::UnsupportedScene
				&& F.Samples[0].DisabledPairs == 0 && F.Samples[0].Transfers == 0);
	}
	{
		FNativeScene F(false,true,true,true);
		F.ProtectedBody->DestroyComponent(); F.ProtectedBody = nullptr;
		auto* Remote = F.Box(FVector(1500,400,0),FVector(5),true);
		Remote->SetPhysicsLinearVelocity(FVector(120,0,0));
		SetNativeCap(F,Remote,250);
		F.Box(FVector(1511.5,400,0),FVector(5),false);
		F.FlushRegistration(); F.EnableNativeClearance(false);
		F.Advance(false);
		TestTrue(TEXT("Potential remote contact is rejected before integration rather than after support bypass"),
			F.Samples[0].PreClearance == EStaticClearanceReason::UnsupportedScene
				&& F.Samples[0].DisabledPairs == 0 && F.Samples[0].Transfers == 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverBindingRetirementTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.NativeBindingRetirement", PortalBoundaryTestFlags)
bool FPortalSolverBindingRetirementTest::RunTest(const FString& Parameters)
{
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance(); F.Advance(true);
		F.ExitWall->BodyInstance.SetBodyTransform(FTransform(FVector(1000,10,0)),ETeleportType::TeleportPhysics); F.FlushRegistration(); F.Advance(true);
		bool Changed = false;
		for (const auto& S : F.Samples) { Changed |= S.PreClearance == EStaticClearanceReason::BindingChanged; }
		TestTrue(TEXT("Native support movement retires its immutable topology binding"),Changed && F.Samples.Last().Retired);
		F.ExitWall->BodyInstance.SetBodyTransform(FTransform(FVector(1000,0,0)),ETeleportType::TeleportPhysics); F.FlushRegistration(); F.Advance(true);
		TestTrue(TEXT("Returning support to old pose does not revive stale binding"),F.Samples.Last().Retired && !F.Samples.Last().Decision.BypassSupport && F.Samples.Last().Transfers == 0);
	}
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance();
		F.Body->SetBoxExtent(FVector(6)); F.Advance(true);
		TestTrue(TEXT("Native shape mutation cannot reuse bound registered geometry"),F.Samples[0].PreClearance == EStaticClearanceReason::BindingChanged && F.Samples.Last().Retired);
	}
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance(); ++F.Command.HalfWidth; F.Advance(true);
		TestTrue(TEXT("Unversioned aperture change cannot reuse bound pair"),F.Samples[0].PreClearance == EStaticClearanceReason::BindingChanged && F.Samples.Last().Retired);
	}
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance(); F.Advance(true);
		F.EntryWall->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Ignore); F.Advance(true);
		TestTrue(TEXT("Support response loss retires the binding needed for safe contact restoration"),F.Samples.Last().Retired);
		F.EntryWall->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Block); F.Advance(true);
		TestTrue(TEXT("Restoring a support filter cannot revive retired passage authority"),F.Samples.Last().Retired && F.Samples.Last().Transfers == 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverNativeFiltersTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.NativeSimulationFilters", PortalBoundaryTestFlags)
bool FPortalSolverNativeFiltersTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (int32 Mode : {0,1,2,3,4})
	{
		FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
		auto* Obstacle = F.Box(FVector(1013,0,0),FVector(.25),Mode == 3);
		if (Mode == 0 || Mode == 3) { Obstacle->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Ignore); }
		if (Mode == 1) { Obstacle->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Overlap); }
		if (Mode == 2) { Obstacle->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }
		if (Mode == 4)
		{
			Obstacle->SetCollisionObjectType(ECC_WorldDynamic);
			F.Body->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Ignore);
		}
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		bool Clear = true;
		for (const auto& S : F.Samples) { Clear &= S.PreClearance == EStaticClearanceReason::Clear
			&& S.IntegratedClearance == EStaticClearanceReason::Clear && S.SolvedClearance == EStaticClearanceReason::Clear; }
		TestTrue(FString::Printf(TEXT("Native bilateral filters allow clear passage: substeps=%d mode=%d"),Substeps,Mode),Clear);
		TestTrue(TEXT("Ignored/Overlap/query-only/filtered-dynamic obstruction allows one committed fact"),
			F.Samples.Last().Transfers == 1 && F.FactCursor->LastRevision() == 1 && F.FactsValid);
	}
	for (bool Substeps : {false,true})
	{
		FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
		auto* Obstacle = F.Box(FVector(1013,0,0),FVector(.25),false);
		Obstacle->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Ignore);
		for (int32 I=0; I<3; ++I) { F.Advance(Substeps); }
		TestTrue(TEXT("Ignored obstacle initially permits partial insertion"),FMath::IsNearlyEqual(F.Body->GetComponentLocation().X,2.,1.e-6));
		const int32 First = F.Samples.Num(); Obstacle->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Block);
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		TestTrue(TEXT("Native filter mutation revokes clearance at first affected PreIntegrate"),F.Samples[First].PreClearance == EStaticClearanceReason::DestinationBlocked);
		TestTrue(TEXT("Restored blocking response cancels without a transfer or GT Recovery"),F.Samples.Last().Transfers == 0
			&& F.FactCursor->LastRevision() == 0 && F.Body->GetComponentLocation().X > 6.8);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverKinematicCoverageTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.KinematicIntervalCoverage", PortalBoundaryTestFlags)
bool FPortalSolverKinematicCoverageTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true})
	{
		{
			FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance(false);
			F.ProtectedBody->SetSimulatePhysics(false); F.FlushRegistration();
			F.ProtectedBody->BodyInstance.SetBodyTransform(FTransform(FVector(8,80,0)),ETeleportType::None);
			for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
			bool Clear = true;
			for (const auto& S : F.Samples) { Clear &= S.PreClearance == EStaticClearanceReason::Clear
				&& S.IntegratedClearance == EStaticClearanceReason::Clear && S.SolvedClearance == EStaticClearanceReason::Clear; }
			TestTrue(TEXT("Unrelated moving kinematic no longer rejects the entire scene"),Clear && F.Samples.Last().Transfers == 1 && F.FactCursor->LastRevision() == 1);
			TestTrue(TEXT("Clear kinematic target actually reaches its native endpoint"),FMath::IsNearlyEqual(F.ProtectedBody->BodyInstance.GetUnrealWorldTransform().GetLocation().Y,80.,1.e-4));
		}
		for (bool Partial : {false,true})
		{
			FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
			if (Partial) { for (int32 I=0; I<3; ++I) { F.Advance(Substeps); } }
			auto* Moving = F.Box(FVector(1013,-30,0),FVector(.25),true); Moving->SetSimulatePhysics(false);
			Moving->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly); F.FlushRegistration();
			const int32 First = F.Samples.Num();
			Moving->BodyInstance.SetBodyTransform(FTransform(FVector(1013,30,0)),ETeleportType::None);
			F.Advance(Substeps);
			TestTrue(TEXT("Endpoint-clear kinematic sweep blocks before entering the corridor"),F.Samples[First].PreClearance == EStaticClearanceReason::DestinationBlocked);
			TestTrue(TEXT("Swept obstruction actual native target reaches clear far endpoint"),FMath::IsNearlyEqual(Moving->BodyInstance.GetUnrealWorldTransform().GetLocation().Y,30.,1.e-4));
			// Keep a stationary blocker after the path sweep to prove contact restoration.
			Moving->BodyInstance.SetBodyTransform(FTransform(FVector(1013,0,0)),ETeleportType::TeleportPhysics);
			for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
			TestTrue(TEXT("Moving PhysicsOnly blocker cancels partial/normal insertion with no transfer"),F.Samples.Last().Transfers == 0
				&& F.FactCursor->LastRevision() == 0 && F.Body->GetComponentLocation().X > 6.8);
		}
		{
			FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
			auto* Rotating = F.Box(FVector(1013,17,0),FVector(.25,20,.25),true); Rotating->SetSimulatePhysics(false);
			Rotating->BodyInstance.SetBodyTransform(FTransform(FQuat(FVector::UpVector,UE_PI/2),FVector(1013,17,0)),ETeleportType::TeleportPhysics);
			F.FlushRegistration();
			Rotating->BodyInstance.SetBodyTransform(FTransform(FQuat(FVector::UpVector,-UE_PI/2),FVector(1013,17,0)),ETeleportType::None);
			F.Advance(Substeps);
			TestTrue(TEXT("Endpoint-clear thin rotating box is blocked by intermediate-rotation envelope"),F.Samples[0].PreClearance == EStaticClearanceReason::DestinationBlocked);
			TestTrue(TEXT("Rotating kinematic actually reaches native target orientation"),Rotating->BodyInstance.GetUnrealWorldTransform().GetRotation().Equals(FQuat(FVector::UpVector,-UE_PI/2),1.e-5));
			TestTrue(TEXT("Rotational obstacle sweep cannot commit a transfer"),F.Samples.Last().Transfers == 0);
		}
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverKinematicVelocityTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.KinematicVelocityCoverage", PortalBoundaryTestFlags)
bool FPortalSolverKinematicVelocityTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (bool Blocked : {false,true})
	{
		FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
		auto* Moving = F.Box(FVector(1013,Blocked ? -30 : 40,0),FVector(.25),true);
		Moving->SetSimulatePhysics(false); F.FlushRegistration();
		// Use the native GT handoff, not a component trajectory/extrapolation.
		auto& Native = Moving->BodyInstance.GetPhysicsActor()->GetGameThreadAPI();
		Native.SetV(Chaos::FVec3(0,3600,0)); Native.SetW(Chaos::FVec3(0,0,5));
		Chaos::FKinematicTarget Target; Target.SetVelocityMode(); Native.SetKinematicTarget(Target);
		F.Advance(Substeps);
		FVector NativePosition = FVector::ZeroVector;
		auto* Proxy = Moving->BodyInstance.GetPhysicsActor();
		F.Scene->GetSolver()->EnqueueCommandImmediate([Proxy,&NativePosition]()
		{ NativePosition = FVector(Proxy->GetPhysicsThreadAPI()->X()); });
		F.FlushRegistration(); // wait for the native read; no simulated interval.
		AddInfo(FString::Printf(TEXT("Velocity-mode actual PT endpoint substeps=%d blocked=%d y=%.6f"),Substeps,Blocked,NativePosition.Y));
		TestTrue(TEXT("Native velocity-mode target actually integrates a full frame"),
			FMath::IsNearlyEqual(NativePosition.Y,Blocked ? 30. : 100.,1.e-3));
		if (Blocked)
		{
			TestTrue(TEXT("Kinematic velocity/angular-motion sweep rejects before affected integration"),F.Samples[0].PreClearance == EStaticClearanceReason::DestinationBlocked);
			TestTrue(TEXT("Velocity-mode obstruction creates no committed transfer"),F.Samples.Last().Transfers == 0 && F.FactCursor->LastRevision() == 0);
		}
		else
		{
			for (int32 I=0; I<11; ++I) { F.Advance(Substeps); }
			bool Clear = true;
			for (const auto& S : F.Samples) { Clear &= S.PreClearance == EStaticClearanceReason::Clear
				&& S.IntegratedClearance == EStaticClearanceReason::Clear && S.SolvedClearance == EStaticClearanceReason::Clear; }
			TestTrue(TEXT("Unrelated velocity-mode spinning kinematic stays inside each physical envelope"),Clear);
			TestTrue(TEXT("Velocity-mode clear scene commits once with coherent facts"),F.Samples.Last().Transfers == 1
				&& F.FactCursor->LastRevision() == 1 && F.FactsValid);
		}
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverDormantClearTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.DormantStableBodies", PortalBoundaryTestFlags)
bool FPortalSolverDormantClearTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (bool Held : {false,true}) for (double Mass : {1.,20.})
	{
		FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
		auto* Sleeper = F.Box(FVector(1500,400,100),FVector(5),true);
		Sleeper->SetMassOverrideInKg(NAME_None,Mass); Sleeper->PutAllRigidBodiesToSleep(); F.FlushRegistration();
		TestFalse(TEXT("Candidate is genuinely asleep, not a static/kinematic replacement"),Sleeper->IsAnyRigidBodyAwake());
		if (Held)
		{
			BeginHold(F,FTransform(FVector(-40,0,0))); F.Hold.Region.Planes.Reset();
			TestTrue(TEXT("Held traveller shares the native dormant scene proof"),F.Callback->ConfigureRelationBeforeDispatch(F.Hold.DesiredHolderPose,F.Hold.LocalGrabAnchor));
		}
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		bool Clear = true, Material = true;
		for (const auto& S : F.Samples) { Clear &= S.PreClearance == EStaticClearanceReason::Clear
			&& S.IntegratedClearance == EStaticClearanceReason::Clear && S.SolvedClearance == EStaticClearanceReason::Clear;
			Material &= S.MaterialPreserved; }
		TestTrue(FString::Printf(TEXT("Isolated native sleeper permits every stage: substeps=%d held=%d mass=%.0f"),Substeps,Held,Mass),Clear);
		TestTrue(TEXT("Isolated dormant scene commits one coherent transfer/fact"),F.Samples.Last().Transfers == 1
			&& F.FactCursor->LastRevision() == 1 && F.FactsValid && Material);
		TestFalse(TEXT("Verifier preserves obstacle sleep state"),Sleeper->IsAnyRigidBodyAwake());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverDormantWakeTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.DormantWakeAndObstruction", PortalBoundaryTestFlags)
bool FPortalSolverDormantWakeTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true})
	{
		{
			FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
			auto* Sleeper = F.Box(FVector(1013,0,0),FVector(.25),true);
			Sleeper->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly); Sleeper->PutAllRigidBodiesToSleep(); F.FlushRegistration();
			for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
			TestTrue(TEXT("Sleeping PhysicsOnly obstruction still blocks mapped exit"),F.Samples[0].PreClearance == EStaticClearanceReason::DestinationBlocked);
			TestTrue(TEXT("Sleeping obstruction cannot publish a transfer"),F.Samples.Last().Transfers == 0 && F.FactCursor->LastRevision() == 0);
		}
		{
			FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
			auto* Sleeper = F.Box(FVector(1500,400,100),FVector(5),true); Sleeper->PutAllRigidBodiesToSleep();
			for (int32 I=0; I<3; ++I) { F.Advance(Substeps); }
			TestTrue(TEXT("Dormant lease initially permits partial insertion"),FMath::IsNearlyEqual(F.Body->GetComponentLocation().X,2.,1.e-6));
			const int32 First = F.Samples.Num(); Sleeper->WakeAllRigidBodies();
			auto* Unbounded = Sleeper->BodyInstance.GetPhysicsActor();
			F.Scene->GetSolver()->EnqueueCommandImmediate([Unbounded]()
			{ Unbounded->GetHandle_LowLevel()->CastToRigidParticle()->SetMaxLinearSpeedSq(TNumericLimits<float>::Max()); });
			F.FlushRegistration();
			for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
			TestTrue(TEXT("Wake into unbounded active motion rejects at the first affected PreIntegrate"),F.Samples[First].PreClearance == EStaticClearanceReason::UnsupportedScene);
			TestTrue(TEXT("Wake rejection restores ordinary contacts without GT Recovery"),F.Body->GetComponentLocation().X > 6.8
				&& F.Samples.Last().Transfers == 0 && F.FactCursor->LastRevision() == 0);
			Sleeper->PutAllRigidBodiesToSleep(); const int32 Reacquire = F.Samples.Num(); F.Advance(Substeps);
			TestTrue(TEXT("A new physical interval may acquire a fresh actually valid sleep lease"),F.Samples[Reacquire].PreClearance == EStaticClearanceReason::Clear && !F.Samples.Last().Retired);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverDormantPartnersTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.DormantWakePartners", PortalBoundaryTestFlags)
bool FPortalSolverDormantPartnersTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (int32 Mode : {0,1,2})
	{
		FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
		auto* Sleeper = F.Box(FVector(1500,400,0),FVector(1),true); Sleeper->PutAllRigidBodiesToSleep();
		auto* Partner = F.Box(FVector(1500,Mode == 0 ? 360 : 600,0),FVector(1),true);
		Partner->SetCollisionObjectType(ECC_WorldDynamic);
		F.Body->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Ignore);
		if (Mode == 0) { Partner->SetSimulatePhysics(false); }
		if (Mode == 2) { Partner->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Ignore); }
		F.FlushRegistration();
		if (Mode == 0) { Partner->BodyInstance.SetBodyTransform(FTransform(FVector(1500,440,0)),ETeleportType::None); }
		F.Advance(Substeps);
		if (Mode == 2)
		{
			for (int32 I=0; I<11; ++I) { F.Advance(Substeps); }
			TestTrue(TEXT("Native filtering against both traveller and sleeper excludes the wake source"),F.Samples[0].PreClearance == EStaticClearanceReason::Clear
				&& F.Samples.Last().Transfers == 1 && F.FactCursor->LastRevision() == 1);
		}
		else
		{
			TestTrue(TEXT("Partner ignored by traveller but collidable with sleeper cannot create false clearance"),F.Samples[0].PreClearance == EStaticClearanceReason::UncertifiedDynamicInteraction);
			TestTrue(TEXT("Potential wake source cannot disable support pairs or create a fact"),F.Samples[0].DisabledPairs == 0
				&& F.Samples.Last().Transfers == 0 && F.FactCursor->LastRevision() == 0);
		}
	}
	for (bool Substeps : {false,true})
	{
		FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
		auto* Sleeper = F.Box(FVector(1500,400,0),FVector(1),true);
		auto* Bridge = F.Box(FVector(1500,440,0),FVector(1),true);
		auto* Active = F.Box(FVector(1500,600,0),FVector(1),true);
		Bridge->SetCollisionObjectType(ECC_WorldDynamic); Active->SetCollisionObjectType(ECC_Pawn);
		F.Body->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Ignore);
		F.Body->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
		Sleeper->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
		Sleeper->PutAllRigidBodiesToSleep(); Bridge->PutAllRigidBodiesToSleep(); F.FlushRegistration();
		F.Advance(Substeps);
		TestTrue(TEXT("Active wake influence through a sleeper ignored by the traveller is still rejected"),
			F.Samples[0].PreClearance == EStaticClearanceReason::UncertifiedDynamicInteraction
			&& F.Samples[0].DisabledPairs == 0 && F.Samples.Last().Transfers == 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalSolverDormantLeaseTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.DormantLeaseInvalidation", PortalBoundaryTestFlags)
bool FPortalSolverDormantLeaseTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (bool PostSolve : {false,true})
	{
		FNativeScene F(Substeps,true,true,true); F.EnableNativeClearance();
		auto* Sleeper = F.Box(FVector(1500,400,0),FVector(1),true); Sleeper->PutAllRigidBodiesToSleep(); F.FlushRegistration();
		auto Mutation = [Proxy=Sleeper->BodyInstance.GetPhysicsActor(),Changed=false]() mutable
		{
			if (Changed) { return; } Changed = true;
			auto* Native = Proxy->GetPhysicsThreadAPI(); Native->SetX(Native->X()+Chaos::FVec3(0,1,0));
		};
		if (PostSolve) { F.Callback->AfterSolveMutation = MoveTemp(Mutation); }
		else { F.Callback->AfterIntegrateMutation = MoveTemp(Mutation); }
		F.Advance(Substeps);
		TestTrue(TEXT("Dormant interval started with a valid native certificate"),F.Samples[0].PreClearance == EStaticClearanceReason::Clear);
		TestTrue(TEXT("Native pose mutation cannot reuse the same interval lease"),
			(PostSolve ? F.Samples[0].SolvedClearance : F.Samples[0].IntegratedClearance) == EStaticClearanceReason::UnsupportedScene);
		TestTrue(TEXT("Changed native obstacle cannot publish a transfer"),F.Samples.Last().Transfers == 0 && F.FactCursor->LastRevision() == 0);
	}
	for (int32 Invalid : {0,1,2})
	{
		FNativeScene F(true,true,true,true); F.EnableNativeClearance();
		auto* Sleeper = F.Box(FVector(1500,400,0),FVector(5),true); F.FlushRegistration();
		if (Invalid == 2)
		{
			auto* Contact = F.Box(FVector(1509,400,0),FVector(5),true);
			Contact->SetSimulatePhysics(false); F.Advance(true);
			// The warm-up also collided the traveller with its support; restore the
			// declared normal-motion fixture so that it cannot mask the sleep gate.
			F.Body->BodyInstance.SetBodyTransform(FTransform(FVector(8,0,0)),ETeleportType::TeleportPhysics);
			F.Body->SetPhysicsLinearVelocity(FVector(-120,0,0));
			F.Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		}
		if (Invalid == 1)
		{
			AActor* Owner = F.World->SpawnActor<AActor>(); auto* Joint = NewObject<UPhysicsConstraintComponent>(Owner);
			Owner->SetRootComponent(Joint); Owner->AddInstanceComponent(Joint);
			Joint->SetWorldLocation(Sleeper->GetComponentLocation()); Joint->RegisterComponent();
			Joint->SetConstrainedComponents(Sleeper,NAME_None,F.EntryWall,NAME_None);
		}
		Sleeper->PutAllRigidBodiesToSleep(); F.FlushRegistration();
		if (Invalid == 2)
		{
			int32 Midphases = 0; auto* Proxy = Sleeper->BodyInstance.GetPhysicsActor();
			F.Scene->GetSolver()->EnqueueCommandImmediate([Proxy,&Midphases]()
			{ Midphases = Proxy->GetHandle_LowLevel()->ParticleCollisions().Num(); });
			F.FlushRegistration();
			TestTrue(TEXT("The sleeping contact fixture really retains native collision midphases"),Midphases > 0);
		}
		if (Invalid == 0)
		{
			auto* Proxy = Sleeper->BodyInstance.GetPhysicsActor();
			F.Scene->GetSolver()->EnqueueCommandImmediate([Proxy]()
			{ Proxy->GetHandle_LowLevel()->CastToRigidParticle()->SetLinearImpulseVelocity(Chaos::FVec3(0,1,0)); });
		}
		const int32 First = F.Samples.Num(); F.Advance(true);
		TestTrue(FString::Printf(TEXT("Invalid dormant profile %d (impulse / joint / contact) rejects; reason=%d"),Invalid,static_cast<int32>(F.Samples[First].PreClearance)),F.Samples[First].PreClearance == EStaticClearanceReason::UnsupportedScene);
		TestTrue(TEXT("Invalid sleep lease creates no transfer"),F.Samples.Last().Transfers == 0);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalContactIslandStableTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.ContactIslandStable", PortalBoundaryTestFlags)
bool FPortalContactIslandStableTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (bool Stack : {false,true}) for (bool Held : {false,true})
	{
		FNativeSleepingContacts Fixture(Substeps,Stack); auto& F = Fixture.Scene;
		TestTrue(TEXT("Fixture has actual sleeping native contact island and retained midphases"),Fixture.NativeSleepingIsland
			&& Fixture.NativeIslandSize == (Stack ? 2 : 1) && Fixture.NativeMidphases > 0);
		TArray<FTransform> Poses; for (auto* Member : Fixture.Members) { Poses.Add(Member->GetComponentTransform()); }
		if (Held)
		{
			BeginHold(F,FTransform(FVector(-40,0,0))); F.Hold.Region.Planes.Reset();
			TestTrue(TEXT("Held traveller uses the shared contact-island certificate"),F.Callback->ConfigureRelationBeforeDispatch(F.Hold.DesiredHolderPose,F.Hold.LocalGrabAnchor));
		}
		for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
		bool Clear = true, Material = true;
		for (const auto& S : F.Samples)
		{ Clear &= S.PreClearance == EStaticClearanceReason::Clear && S.IntegratedClearance == EStaticClearanceReason::Clear
			&& S.SolvedClearance == EStaticClearanceReason::Clear; Material &= S.MaterialPreserved; }
		TestTrue(FString::Printf(TEXT("Closed contact island certifies all stages: substeps=%d stack=%d held=%d; first=%d/%d/%d"),
			Substeps,Stack,Held,static_cast<int32>(F.Samples[0].PreClearance),static_cast<int32>(F.Samples[0].IntegratedClearance),
			static_cast<int32>(F.Samples[0].SolvedClearance)),Clear);
		TestTrue(TEXT("Closed native contact island permits exactly one coherent held/free transfer"),F.Samples.Last().Transfers == 1
			&& F.FactCursor->LastRevision() == 1 && F.FactsValid && Material);
		for (int32 I=0; I<Fixture.Members.Num(); ++I)
		{ TestTrue(TEXT("Verifier preserves sleeping furniture pose and state"),!Fixture.Members[I]->IsAnyRigidBodyAwake()
			&& Fixture.Members[I]->GetComponentTransform().Equals(Poses[I],1.e-6)); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalContactIslandWakeTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.ContactIslandWakeAndBlock", PortalBoundaryTestFlags)
bool FPortalContactIslandWakeTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true})
	{
		{
			FNativeSleepingContacts Fixture(Substeps,false,true); auto& F = Fixture.Scene;
			TestTrue(TEXT("Blocked-exit fixture really has sleeping retained contacts"),Fixture.NativeSleepingIsland && Fixture.NativeMidphases > 0);
			for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
			TestTrue(TEXT("Sleeping contact-island member inside mapped exit still blocks"),F.Samples[0].PreClearance == EStaticClearanceReason::DestinationBlocked
				&& F.Samples.Last().Transfers == 0 && F.FactCursor->LastRevision() == 0);
		}
		{
			FNativeSleepingContacts Fixture(Substeps,true); auto& F = Fixture.Scene;
			for (int32 I=0; I<3; ++I) { F.Advance(Substeps); }
			TestTrue(TEXT("Valid contact island initially permits partial insertion"),FMath::IsNearlyEqual(F.Body->GetComponentLocation().X,2.,1.e-6));
			Fixture.Members.Last()->WakeAllRigidBodies(); const int32 First = F.Samples.Num();
			for (int32 I=0; I<12; ++I) { F.Advance(Substeps); }
			TestTrue(TEXT("Woken contact-island member denies first affected physical interval"),F.Samples[First].PreClearance == EStaticClearanceReason::UnsupportedScene);
			TestTrue(TEXT("Contact-island wake restores ordinary blocking without GT Recovery"),F.Body->GetComponentLocation().X > 6.8
				&& F.Samples.Last().Transfers == 0 && F.FactCursor->LastRevision() == 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalContactIslandLeaseTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.ContactIslandLeaseInvalidation", PortalBoundaryTestFlags)
bool FPortalContactIslandLeaseTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true}) for (int32 Stage : {0,1,2})
	{
		FNativeSleepingContacts Fixture(Substeps,true); auto& F = Fixture.Scene;
		auto Mutation = [Proxy=Fixture.Floor->BodyInstance.GetPhysicsActor(),Changed=false]() mutable
		{
			if (Changed) { return; } Changed = true;
			auto* P = Proxy->GetHandle_LowLevel(); P->SetX(P->GetX()+Chaos::FVec3(0,0,1));
		};
		if (Stage == 0) { F.Scene->GetSolver()->EnqueueCommandImmediate(MoveTemp(Mutation)); }
		else if (Stage == 1) { F.Callback->AfterIntegrateMutation = MoveTemp(Mutation); }
		else { F.Callback->AfterSolveMutation = MoveTemp(Mutation); }
		F.Advance(Substeps);
		const auto& S = F.Samples[0];
		if (Stage) { TestTrue(TEXT("Support mutation interval begins with valid native contact lease"),S.PreClearance == EStaticClearanceReason::Clear); }
		TestTrue(TEXT("Stale cached static contact or changed interval support denies certification"),
			(Stage == 0 ? S.PreClearance : Stage == 1 ? S.IntegratedClearance : S.SolvedClearance) == EStaticClearanceReason::UnsupportedScene);
		TestTrue(TEXT("Stale or changed static contact cannot create a committed transfer fact"),F.Samples.Last().Transfers == 0 && F.FactCursor->LastRevision() == 0);
		// Cancellation restored wall contacts, which can rotate the traveller.
		// Restore only this fixture's normal approach before probing a fresh lease.
		F.Body->BodyInstance.SetBodyTransform(FTransform(FVector(8,0,0)),ETeleportType::TeleportPhysics);
		F.Body->SetPhysicsLinearVelocity(FVector(-120,0,0));
		F.Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		F.Advance(Substeps);
		const auto FreshReason = F.Samples[F.Samples.Num()-(Substeps ? 2 : 1)].PreClearance;
		TestTrue(FString::Printf(TEXT("Fresh interval cannot bootstrap from unchanged stale static manifold: substeps=%d stage=%d reason=%d"),Substeps,Stage,static_cast<int32>(FreshReason)),FreshReason == EStaticClearanceReason::UnsupportedScene);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalContactIslandUnsupportedTest,
	"SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance.ContactIslandUnsupported", PortalBoundaryTestFlags)
bool FPortalContactIslandUnsupportedTest::RunTest(const FString& Parameters)
{
	for (bool Substeps : {false,true})
	{
		{
			FNativeSleepingContacts Fixture(Substeps,false,false,true); auto& F = Fixture.Scene;
			TestTrue(TEXT("Kinematic support fixture retains actual native contact"),Fixture.NativeMidphases > 0);
			F.Advance(Substeps);
			TestTrue(TEXT("Retained kinematic contact remains outside stationary contact-island profile"),F.Samples[0].PreClearance == EStaticClearanceReason::UnsupportedScene
				&& F.Samples[0].DisabledPairs == 0 && F.Samples.Last().Transfers == 0);
		}
		{
			FNativeSleepingContacts Fixture(Substeps,true); auto& F = Fixture.Scene;
			AActor* Owner = F.World->SpawnActor<AActor>(); auto* Joint = NewObject<UPhysicsConstraintComponent>(Owner);
			Owner->SetRootComponent(Joint); Owner->AddInstanceComponent(Joint);
			Joint->SetWorldLocation(Fixture.Members.Last()->GetComponentLocation()); Joint->RegisterComponent();
			Joint->SetConstrainedComponents(Fixture.Members.Last(),NAME_None,Fixture.Floor,NAME_None);
			for (auto* Member : Fixture.Members) { Member->PutAllRigidBodiesToSleep(); }
			F.Advance(Substeps);
			TestTrue(TEXT("Sleeping contact island cannot hide a persistent world joint"),F.Samples[0].PreClearance == EStaticClearanceReason::UnsupportedScene
				&& F.Samples[0].DisabledPairs == 0 && F.Samples.Last().Transfers == 0);
		}
	}
	return true;
}
#endif
