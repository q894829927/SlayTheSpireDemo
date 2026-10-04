#include "InteriorPortalChaosStaticClearance.h"
#include "InteriorPortalMath.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/PBDRigidsSOAs.h"
#include "Chaos/ShapeInstance.h"
#include "Chaos/Collision/CollisionFilter.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/ContactModification.h"
#include "Chaos/Island/IslandManager.h"
#include "Chaos/Collision/CollisionConstraintAllocator.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include <cfloat>

namespace InteriorPortalPhysics
{
	namespace StaticClearanceDetail
	{
		bool SameStep(const FPhysicsStepKey& A, const FPhysicsStepKey& B)
		{ return A.SolverEpoch == B.SolverEpoch && A.SolverFrame == B.SolverFrame && A.EvolutionSerial == B.EvolutionSerial && A.DeltaSeconds == B.DeltaSeconds; }
		bool SameState(const FBoundaryState& A, const FBoundaryState& B)
		{ return A.Pose.Equals(B.Pose,0) && A.LinearVelocity == B.LinearVelocity && A.AngularVelocity == B.AngularVelocity; }
		bool SameRequest(const FBoundaryCommand& A, const FBoundaryCommand& B)
		{
			return A.Traveller.Handle == B.Traveller.Handle && A.Traveller.Geometry == B.Traveller.Geometry
				&& A.PairGeneration == B.PairGeneration && A.Revision == B.Revision && A.MaxReuseSteps == B.MaxReuseSteps
				&& A.PermitSupportBypass == B.PermitSupportBypass && A.PermitTransfer == B.PermitTransfer
				&& A.Entry.Equals(B.Entry,0) && A.Exit.Equals(B.Exit,0) && A.LocalAuthorityReference == B.LocalAuthorityReference
				&& A.HalfWidth == B.HalfWidth && A.HalfHeight == B.HalfHeight && A.MarginCm == B.MarginCm
				&& A.SupportHalfThicknessCm == B.SupportHalfThicknessCm && A.MaxStepSeconds == B.MaxStepSeconds
				&& A.MaxTranslationCm == B.MaxTranslationCm;
		}
		FGeometry BoxGeometry(const FBox& Box, const FVector& Scale = FVector::OneVector)
		{
			FGeometry G; G.BakedScale = Scale; FPrimitive P; P.Shape = EShape::Box;
			for (int32 X : {0,1}) for (int32 Y : {0,1}) for (int32 Z : {0,1})
			{ P.Vertices.Add(FVector(X ? Box.Max.X : Box.Min.X,Y ? Box.Max.Y : Box.Min.Y,Z ? Box.Max.Z : Box.Min.Z)); }
			G.Primitives.Add(MoveTemp(P)); return G;
		}
		FBox RegisteredBounds(const FGeometry& G)
		{
			FBox B(ForceInit);
			for (const auto& P : G.Primitives)
			{
				if (P.Shape == EShape::Sphere) { B += P.Center - FVector(P.Radius); B += P.Center + FVector(P.Radius); }
				else if (P.Shape == EShape::Capsule)
				{ B += P.Center-P.HalfSegment-FVector(P.Radius); B += P.Center-P.HalfSegment+FVector(P.Radius);
				  B += P.Center+P.HalfSegment-FVector(P.Radius); B += P.Center+P.HalfSegment+FVector(P.Radius); }
				else { for (const auto& V : P.Vertices) { B += V; } }
			}
			return B;
		}
		bool SameNativeLocalBounds(const FBox& Native, const FBox& Prepared)
		{
			if (!Native.IsValid || !Prepared.IsValid) { return false; }
			const double Magnitude = FMath::Max(FMath::Max(Native.Min.GetAbsMax(),Native.Max.GetAbsMax()),
				FMath::Max(Prepared.Min.GetAbsMax(),Prepared.Max.GetAbsMax()));
			// GT scale baking and Chaos float geometry may round the same authored
			// bound differently. Accept at most two float ULPs, capped at .01 cm.
			const double Tolerance = FMath::Min(.01,FMath::Max(1.e-6,2.*FLT_EPSILON*FMath::Max(1.,Magnitude)));
			return FMath::IsFinite(Tolerance)
				&& Native.Min.Equals(Prepared.Min,Tolerance) && Native.Max.Equals(Prepared.Max,Tolerance);
		}
		bool WithinNativeStepBudget(double DeltaSeconds, double MaxStepSeconds)
		{
			if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0
				|| !FMath::IsFinite(MaxStepSeconds) || MaxStepSeconds <= 0) { return false; }
			// UE's 1/60 solver step may arrive as 0.0166667 while the command
			// encodes exact 1/60. This only absorbs timestep representation error;
			// all reach and sweep calculations below use the actual DeltaSeconds.
			const double Tolerance = FMath::Min(1.e-7,FMath::Max(1.e-9,32.*FLT_EPSILON*MaxStepSeconds));
			return DeltaSeconds <= MaxStepSeconds + Tolerance;
		}
		template<typename TParticle> bool Simulates(const TParticle& P)
		{ for (const auto& S : P.ShapesArray()) { if (Chaos::FilterHasSimEnabled(S.Get())) { return true; } } return false; }
		bool Interacts(const Chaos::FGeometryParticleHandle* Body, const Chaos::FGeometryParticleHandle* Other)
		{
			// Passing no ignore manager conservatively keeps explicitly ignored pairs.
			// Geometry/trace-type exclusions likewise remain conservative over-blocks.
			if (!Chaos::ParticlePairBroadPhaseFilter(Body,Other,nullptr)) { return false; }
			for (const auto& A : Body->ShapesArray()) for (const auto& B : Other->ShapesArray())
			{
				if (Chaos::FilterHasSimEnabled(A.Get()) && Chaos::FilterHasSimEnabled(B.Get())
					&& A->GetShapeFilterData().NarrowFilter(B->GetShapeFilterData()) != Chaos::Filter::ENarrowFilterResult::None)
				{ return true; }
			}
			return false;
		}
		template<typename TParticle> bool NativeBounds(const TParticle& P, FBox& Out)
		{
			const auto* G = P.GetGeometry();
			if (!G || !G->HasBoundingBox() || P.CastToClustered()) { return false; }
			const auto B = G->BoundingBox(); Out = FBox(FVector(B.Min()),FVector(B.Max()));
			return Out.IsValid && !Out.Min.ContainsNaN() && !Out.Max.ContainsNaN() && Out.GetSize().GetMin() > 0;
		}
		FBox WorldBounds(const FBox& Local, const FTransform& Pose)
		{
			FBox World(ForceInit);
			const FGeometry Geometry = BoxGeometry(Local);
			for (const auto& V : Geometry.Primitives[0].Vertices) { World += Pose.TransformPositionNoScale(V); }
			return World;
		}
		FBox SweptBounds(const FBox& Local, const FTransform& A, const FTransform& B, double Margin)
		{ return (WorldBounds(Local,A) + WorldBounds(Local,B)).ExpandBy(Margin); }
		bool KinematicSweep(const Chaos::FGeometryParticleHandle* P, const FBox& Local, double Dt, FBox& Out)
		{
			const auto* K = P->CastToKinematicParticle();
			const FVector Origin(P->GetX()); const FQuat Rotation(P->GetR());
			if (!K || Origin.ContainsNaN() || Rotation.ContainsNaN() || !Rotation.IsNormalized()) { return false; }
			const auto& Target = K->KinematicTarget(); FVector End = Origin;
			switch (Target.GetMode())
			{
			case Chaos::EKinematicTargetMode::None:
			case Chaos::EKinematicTargetMode::Reset:
				Out = WorldBounds(Local,FTransform(Rotation,Origin)).ExpandBy(1.e-4); return true;
			case Chaos::EKinematicTargetMode::Position:
				// Full remaining frame target covers every unknown substep fraction.
				End = FVector(Target.GetPosition());
				if (FQuat(Target.GetRotation()).ContainsNaN() || !FQuat(Target.GetRotation()).IsNormalized()) { return false; }
				break;
			case Chaos::EKinematicTargetMode::Velocity:
				if (FVector(K->GetV()).ContainsNaN() || FVector(K->GetW()).ContainsNaN()) { return false; }
				End += FVector(K->GetV())*Dt; break;
			default: return false;
			}
			// Radius is around the native actor origin, covering off-center geometry
			// and every intermediate rotation, not just endpoint boxes.
			double Radius = 0; const FGeometry Geometry = BoxGeometry(Local);
			for (const auto& V : Geometry.Primitives[0].Vertices) { Radius = FMath::Max(Radius,V.Size()); }
			if (End.ContainsNaN() || !FMath::IsFinite(Radius)) { return false; }
			Out = FBox(Origin,Origin); Out += End; Out = Out.ExpandBy(Radius+1.e-4); return true;
		}
		bool StableDormant(const Chaos::FGeometryParticleHandle* P)
		{
			const auto* R = P->CastToRigidParticle();
			return R && !R->Disabled() && P->ObjectState() == Chaos::EObjectStateType::Sleeping
				&& FMath::IsFinite(R->M()) && R->M() > 0 && R->InvM() > 0
				&& FVector(R->GetV()).IsNearlyZero(1.e-6) && FVector(R->GetW()).IsNearlyZero(1.e-6)
				&& FVector(R->Acceleration()).IsNearlyZero(1.e-6) && FVector(R->AngularAcceleration()).IsNearlyZero(1.e-6)
				&& FVector(R->LinearImpulseVelocity()).IsNearlyZero(1.e-6) && FVector(R->AngularImpulseVelocity()).IsNearlyZero(1.e-6)
				&& P->ParticleConstraints().IsEmpty();
		}
	}
	FChaosStaticClearance::FChaosStaticClearance(Chaos::FPBDRigidsSolver* InSolver, Chaos::FSingleParticlePhysicsProxy* InBody,
		Chaos::FSingleParticlePhysicsProxy* Support0, Chaos::FSingleParticlePhysicsProxy* Support1,
		const FBoundaryCommand& C, uint64 SolverEpoch, uint64 BindingEpoch, const FTransform& Pose0, const FTransform& Pose1,
		const FGeometry& Geometry0, const FGeometry& Geometry1)
		: Solver(InSolver), Body(InBody), Bound(C), Epoch(SolverEpoch), Binding(BindingEpoch)
	{
		Supports[0] = Support0; Supports[1] = Support1; SupportPoses[0] = Pose0; SupportPoses[1] = Pose1;
		SupportPoses[0].SetScale3D(FVector::OneVector); SupportPoses[1].SetScale3D(FVector::OneVector);
		BoundSupportBounds[0] = StaticClearanceDetail::RegisteredBounds(Geometry0);
		BoundSupportBounds[1] = StaticClearanceDetail::RegisteredBounds(Geometry1);
		if (!Solver || !Body || !Support0 || !Support1 || Body == Support0 || Body == Support1 || Support0 == Support1
			|| !Epoch || !Binding || !C.PairGeneration || !C.Traveller.Handle.Epoch || !C.Traveller.Handle.Id
			|| !C.Traveller.Handle.Generation || !BoundSupportBounds[0].IsValid || !BoundSupportBounds[1].IsValid) { Retire_Internal(); }
	}
	void FChaosStaticClearance::Retire_Internal()
	{ Retired = true; Consumed = true; KinematicEnvelopes.Reset(); ActiveEnvelopes.Reset(); DormantLeases.Reset(); Body = nullptr; Supports[0] = Supports[1] = nullptr; Solver = nullptr; }
	bool FChaosStaticClearance::ContactsRemainIndependent_Internal(Chaos::FCollisionContactModifier& Modifier) const
	{
		if (Retired || !Solver || !Body || TravellerMaxLinearSpeedSq <= 0) { return false; }
		const auto* Traveller = Body->GetHandle_LowLevel();
		const auto* RigidTraveller = Traveller ? Traveller->CastToRigidParticle() : nullptr;
		if (!RigidTraveller || RigidTraveller->MaxLinearSpeedSq() != TravellerMaxLinearSpeedSq
			|| !Traveller->ParticleConstraints().IsEmpty()) { return false; }
		if (ActiveEnvelopes.IsEmpty()) { return true; }
		for (auto& Pair : Modifier)
		{
			const auto Particles = Pair.GetParticlePair();
			if (ActiveEnvelopes.Contains(Particles[0]->UniqueIdx().Idx)
				|| ActiveEnvelopes.Contains(Particles[1]->UniqueIdx().Idx)) { return false; }
		}
		int32 Seen = 0;
		for (const auto& P : Solver->GetParticles().GetNonDisabledView())
		{
			const auto* Lease = ActiveEnvelopes.Find(P.UniqueIdx().Idx);
			if (!Lease) { continue; }
			++Seen;
			const auto* R = P.CastToRigidParticle();
			FBox Local(ForceInit);
			if (!R || P.ObjectState() != Chaos::EObjectStateType::Dynamic || R->CCDEnabled()
				|| !P.ParticleConstraints().IsEmpty() || P.ParticleCollisions().Num() != 0
				|| !StaticClearanceDetail::NativeBounds(P,Local) || Lease->Geometry != P.GetGeometry()
				|| Lease->GeometryHash != P.GetGeometry()->GetTypeHash()
				|| Lease->MaxLinearSpeedSq != R->MaxLinearSpeedSq()
				|| !Lease->Local.Min.Equals(Local.Min,1.e-6) || !Lease->Local.Max.Equals(Local.Max,1.e-6)) { return false; }
			const FBox World = StaticClearanceDetail::WorldBounds(Local,FTransform(FQuat(R->GetQ()),FVector(R->GetP())));
			if (!Lease->Reach.IsInsideOrOn(World.Min) || !Lease->Reach.IsInsideOrOn(World.Max)) { return false; }
		}
		return Seen == ActiveEnvelopes.Num();
	}
	FStaticClearanceProof FChaosStaticClearance::Certify_Internal(const FBoundaryCommand& C,
		const FBoundaryState& A, const FBoundaryState& B, const FPhysicsStepKey& Step, EClearanceStage Stage, int32 Endpoint)
	{
		using namespace StaticClearanceDetail;
		FStaticClearanceProof Proof;
		if (Sequence == MAX_uint64) { Retire_Internal(); }
		Proof.Binding = Binding; Proof.Sequence = Sequence == MAX_uint64 ? Sequence : ++Sequence; Consumed = false;
		Proof.Command = C; Proof.Start = A; Proof.End = B; Proof.Step = Step; Proof.Stage = Stage;
		ENativeBindingIssue Issue = ENativeBindingIssue::None;
		int32 IssueComponent = -1;
		uint32 SceneIssues = 0;
		const auto MarkSceneIssue = [&](ENativeSceneIssue SceneIssue)
		{ SceneIssues |= NativeSceneIssueBit(SceneIssue); };
		const auto Reject = [&](EStaticClearanceReason Reason)
		{ Proof.Result = Reason; Proof.Issue = Issue; Proof.Component = IssueComponent;
		  Proof.SceneIssues = SceneIssues; return Proof; };
		if (Retired) { return Reject(EStaticClearanceReason::Retired); }
		const FTransform ExpectedEntry = Endpoint == 0 ? Bound.Entry : Bound.Exit, ExpectedExit = Endpoint == 0 ? Bound.Exit : Bound.Entry;
		if (Endpoint < 0 || Endpoint > 1 || C.Traveller.Handle != Bound.Traveller.Handle || C.PairGeneration != Bound.PairGeneration
			|| !(C.Traveller.Geometry == Bound.Traveller.Geometry) || !C.Entry.Equals(ExpectedEntry,0) || !C.Exit.Equals(ExpectedExit,0)
			|| C.HalfWidth != Bound.HalfWidth || C.HalfHeight != Bound.HalfHeight)
		{ Issue = ENativeBindingIssue::Command; Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		// Binding identity is independent of the current interval's time budget.
		// Check native shapes/poses first, so a slow frame still reports a safe
		// interval rejection without hiding a stale scaled support binding.
		auto* H = Body->GetPhysicsThreadAPI(); auto* Particle = Body->GetHandle_LowLevel();
		const auto* Rigid = Particle ? Particle->CastToRigidParticle() : nullptr;
		FBox Local(ForceInit); const FBox Registered = RegisteredBounds(C.Traveller.Geometry);
		if (!H || !Rigid || (H->ObjectState() != Chaos::EObjectStateType::Dynamic && H->ObjectState() != Chaos::EObjectStateType::Sleeping))
		{ Issue = ENativeBindingIssue::BodyState; Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		if (!Simulates(*Particle))
		{ Issue = ENativeBindingIssue::BodyCollision; Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		if (!NativeBounds(*Particle,Local) || !SameNativeLocalBounds(Local,Registered))
		{ Issue = ENativeBindingIssue::BodyBounds; Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		if (!FVector(H->CenterOfMass()).Equals(C.LocalAuthorityReference,1.e-6))
		{ Issue = ENativeBindingIssue::BodyCOM; Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		FBox SupportBounds[2];
		for (int32 I=0; I<2; ++I)
		{
			const auto* P = Supports[I]->GetHandle_LowLevel();
			IssueComponent = I;
			if (!P || P->ObjectState() != Chaos::EObjectStateType::Static)
			{ Issue = ENativeBindingIssue::SupportState; Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
			if (!Simulates(*P) || !Interacts(Particle,P))
			{ Issue = ENativeBindingIssue::SupportCollision; Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
			if (!NativeBounds(*P,SupportBounds[I]) || !SameNativeLocalBounds(SupportBounds[I],BoundSupportBounds[I]))
			{ Issue = ENativeBindingIssue::SupportBounds; Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
			if (!FTransform(FQuat(P->GetR()),FVector(P->GetX())).Equals(SupportPoses[I],1.e-6))
			{ Issue = ENativeBindingIssue::SupportPose; Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		}
		IssueComponent = -1;
		const Chaos::FImplicitObject* Geometries[3] = {Particle->GetGeometry(),Supports[0]->GetHandle_LowLevel()->GetGeometry(),Supports[1]->GetHandle_LowLevel()->GetGeometry()};
		for (int32 I=0; I<3; ++I)
		{
			const uint32 Hash = Geometries[I]->GetTypeHash();
			if (GeometryBound && (NativeGeometry[I] != Geometries[I] || GeometryHashes[I] != Hash))
			{ Issue = ENativeBindingIssue::GeometryChanged; IssueComponent = I-1;
			  Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
			NativeGeometry[I] = Geometries[I]; GeometryHashes[I] = Hash;
		}
		GeometryBound = true;
		const auto SourceSpan = EvaluatePose(BoxGeometry(SupportBounds[Endpoint]),SupportPoses[Endpoint],C.Entry,1.e100,1.e100);
		const auto ExitSpan = EvaluatePose(BoxGeometry(SupportBounds[1-Endpoint]),SupportPoses[1-Endpoint],C.Exit,1.e100,1.e100);
		if (!SourceSpan.Fits() || !ExitSpan.Fits() || SourceSpan.MinNormal > 0 || SourceSpan.MaxNormal < 0
			|| ExitSpan.MinNormal > 0 || ExitSpan.MaxNormal < 0 || SourceSpan.MaxNormal > C.SupportHalfThicknessCm + 1.e-6)
		{ Issue = ENativeBindingIssue::SupportSpan; Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		if (Step.SolverEpoch != Epoch || Step.SolverFrame != Solver->GetCurrentFrame() || !Step.EvolutionSerial
			|| !C.Revision || !WithinNativeStepBudget(Step.DeltaSeconds,C.MaxStepSeconds))
		{ return Reject(EStaticClearanceReason::InvalidInterval); }
		if (Stage == EClearanceStage::PreIntegrate)
		{
			if (IntervalStep.SolverEpoch && (Step.EvolutionSerial <= IntervalStep.EvolutionSerial || Step.SolverFrame <= IntervalStep.SolverFrame))
			{ return Reject(EStaticClearanceReason::InvalidInterval); }
			IntervalStep = Step; IntervalStart = A; IntervalCommand = C; LastStage = Stage;
			KinematicEnvelopes.Reset();
			ActiveEnvelopes.Reset();
			DormantLeases.Reset();
			TravellerMaxLinearSpeedSq = 0;
		}
		else
		{
			if (!SameStep(Step,IntervalStep) || !SameState(A,IntervalStart) || !SameRequest(C,IntervalCommand)
				|| static_cast<uint8>(Stage) != static_cast<uint8>(LastStage)+1)
			{ return Reject(EStaticClearanceReason::InvalidInterval); }
			LastStage = Stage;
		}
		// Persistent constraints can move/rotate the body after this hook; they
		// are not collision constraints and are outside the independent-body profile.
		if (!Particle->ParticleConstraints().IsEmpty())
		{ MarkSceneIssue(ENativeSceneIssue::TravellerConstraint); return Reject(EStaticClearanceReason::UnsupportedScene); }
		// The adapter must install a native hard speed cap before this interval.
		// Arbitrary gravity/force rules are not represented by the submitted
		// predicted pose; the Chaos integrator clamps V before updating P.
		const double SpeedSq = Rigid->MaxLinearSpeedSq();
		// A recreated body or GT/PT state push may restore Chaos's effectively
		// unbounded default. Reject it before issuing a proof.
		const double StepReach = FMath::Sqrt(SpeedSq)*Step.DeltaSeconds;
		if (!FMath::IsFinite(SpeedSq) || SpeedSq <= 0 || !FMath::IsFinite(StepReach)
			|| StepReach >= FMath::Min(C.HalfWidth,C.HalfHeight))
		{ return Reject(EStaticClearanceReason::UnsupportedMotion); }
		if (Stage == EClearanceStage::PreIntegrate) { TravellerMaxLinearSpeedSq = SpeedSq; }
		else if (TravellerMaxLinearSpeedSq != SpeedSq) { return Reject(EStaticClearanceReason::UnsupportedMotion); }
		const FTransform Actual(Stage == EClearanceStage::PreIntegrate ? FQuat(H->R()) : FQuat(Rigid->GetQ()),
			Stage == EClearanceStage::PreIntegrate ? FVector(H->X()) : FVector(Rigid->GetP()),C.Traveller.Geometry.BakedScale);
		const auto& ActualSample = Stage == EClearanceStage::PreIntegrate ? A : B;
		if (!ActualSample.Pose.Equals(Actual,1.e-6) || !ActualSample.LinearVelocity.Equals(FVector(H->V()),1.e-6)
			|| !ActualSample.AngularVelocity.Equals(FVector(H->W()),1.e-6))
		{ return Reject(EStaticClearanceReason::InvalidInterval); }
		const FVector Normal = C.Entry.GetUnitAxis(EAxis::X);
		// The native cap bounds translation in every direction, including gravity.
		// Aperture and scene clearance below certify that full reachable volume;
		// requiring normal-only velocity would reject ordinary falling bodies.
		const double ActualTravel = FVector::Dist(A.Pose.GetLocation(),B.Pose.GetLocation());
		if (A.LinearVelocity.ContainsNaN() || B.LinearVelocity.ContainsNaN() || FVector(H->Acceleration()).ContainsNaN()
			|| !FVector(H->AngularAcceleration()).IsNearlyZero(1.e-6)
			|| !A.AngularVelocity.IsNearlyZero(1.e-6) || !B.AngularVelocity.IsNearlyZero(1.e-6)
			|| !A.Pose.GetRotation().Equals(B.Pose.GetRotation(),1.e-8)
			|| !FMath::IsFinite(ActualTravel) || ActualTravel > C.MaxTranslationCm
			|| ActualTravel > StepReach + 1.e-4)
		{ return Reject(EStaticClearanceReason::UnsupportedMotion); }
		const FGeometry Box = BoxGeometry(Local,C.Traveller.Geometry.BakedScale);
		FTransform AtPlane = B.Pose;
		AtPlane.AddToTranslation(-Normal*C.Entry.InverseTransformPositionNoScale(B.Pose.TransformPositionNoScale(C.LocalAuthorityReference)).X);
		const FQuat Map = InteriorPortalMath::Rotation(C.Entry,C.Exit);
		const auto Mapped = [&](const FTransform& T)
		{ return FTransform(Map*T.GetRotation(),InteriorPortalMath::Position(T.GetLocation(),C.Entry,C.Exit),C.Traveller.Geometry.BakedScale); };
		FTransform ExitClear = Mapped(AtPlane);
		const auto Span = EvaluatePose(Box,ExitClear,C.Exit,C.HalfWidth,C.HalfHeight,C.MarginCm);
		ExitClear.AddToTranslation(C.Exit.GetUnitAxis(EAxis::X)*(FMath::Max(0.,ExitSpan.MaxNormal-Span.MinNormal+C.MarginCm)+C.MaxTranslationCm));
		if (!EvaluateTranslation(Box,A.Pose,B.Pose,C.Entry,C.HalfWidth,C.HalfHeight,C.MarginCm).Fits()
			|| !EvaluateTranslation(Box,B.Pose,AtPlane,C.Entry,C.HalfWidth,C.HalfHeight,C.MarginCm).Fits()
			|| !EvaluateTranslation(Box,Mapped(A.Pose),Mapped(B.Pose),C.Exit,C.HalfWidth,C.HalfHeight,C.MarginCm).Fits()
			|| !EvaluateTranslation(Box,Mapped(AtPlane),ExitClear,C.Exit,C.HalfWidth,C.HalfHeight,C.MarginCm).Fits())
		{ return Reject(EStaticClearanceReason::OutsideAperture); }
		const double MotionReach = FMath::Sqrt(SpeedSq)*Step.DeltaSeconds;
		double RotationRadius = 0;
		const auto Corners = BoxGeometry(Local);
		for (const auto& V : Corners.Primitives[0].Vertices) { RotationRadius = FMath::Max(RotationRadius,V.Size()); }
		const double ConservativeRadius = RotationRadius + MotionReach + C.MarginCm + 1.e-4;
		if (!FMath::IsFinite(ConservativeRadius)) { return Reject(EStaticClearanceReason::UnsupportedMotion); }
		const FVector EntryLocal = C.Entry.InverseTransformPositionNoScale(A.Pose.GetLocation());
		if (FMath::Abs(EntryLocal.Y) + ConservativeRadius > C.HalfWidth
			|| FMath::Abs(EntryLocal.Z) + ConservativeRadius > C.HalfHeight)
		{ return Reject(EStaticClearanceReason::OutsideAperture); }
		FBox Source = SweptBounds(Local,A.Pose,B.Pose,C.MarginCm) + SweptBounds(Local,B.Pose,AtPlane,C.MarginCm);
		FBox Destination = SweptBounds(Local,Mapped(A.Pose),Mapped(B.Pose),C.MarginCm)
			+ SweptBounds(Local,Mapped(AtPlane),ExitClear,C.MarginCm);
		// Origin-centered balls cover all native rotations and all integration
		// translations admitted by the cap, including gravity and unknown forces.
		Source += FBox(A.Pose.GetLocation(),A.Pose.GetLocation()).ExpandBy(ConservativeRadius);
		const FVector ExitOrigin = Mapped(A.Pose).GetLocation();
		Destination += FBox(ExitOrigin,ExitOrigin).ExpandBy(ConservativeRadius);
		bool Unsupported = false, SourceBlocked = false, DestinationBlocked = false, SawBody = false;
		int32 SeenActive = 0;
		bool WakeRisk = false;
		TArray<const Chaos::FGeometryParticleHandle*> Sleepers;
		bool SawSupports[2] = {false,false};
		if (Solver->GetParticles().GetNonDisabledView().Num() > 4096)
		{ MarkSceneIssue(ENativeSceneIssue::ParticleBudget); return Reject(EStaticClearanceReason::UnsupportedScene); }
		const auto KinematicInterval = [&](const Chaos::FGeometryParticleHandle* P, const FBox& Bounds, FBox& World)
		{
			const int32 Key = P->UniqueIdx().Idx;
			if (Stage == EClearanceStage::PreIntegrate)
			{
				if (!KinematicSweep(P,Bounds,Step.DeltaSeconds,World)) { return false; }
				KinematicEnvelopes.Add(Key,{World,Bounds,P->GetGeometry(),P->GetGeometry()->GetTypeHash()});
				return true;
			}
			const auto* Envelope = KinematicEnvelopes.Find(Key);
			if (!Envelope || Envelope->Geometry != P->GetGeometry() || Envelope->GeometryHash != P->GetGeometry()->GetTypeHash()
				|| !Envelope->Local.Min.Equals(Bounds.Min,1.e-6) || !Envelope->Local.Max.Equals(Bounds.Max,1.e-6)
				|| !Envelope->Sweep.IsInsideOrOn(World.Min) || !Envelope->Sweep.IsInsideOrOn(World.Max)) { return false; }
			World = Envelope->Sweep; return true;
		};
		const auto ActiveInterval = [&](const Chaos::FGeometryParticleHandle* P, const FBox& Bounds, FBox& World)
		{
			const auto* R = P->CastToRigidParticle();
			if (!R || P->ObjectState() != Chaos::EObjectStateType::Dynamic || R->CCDEnabled()
				|| !P->ParticleConstraints().IsEmpty() || P->ParticleCollisions().Num() != 0) { return false; }
			const int32 Key = P->UniqueIdx().Idx;
			const double SpeedSq = R->MaxLinearSpeedSq();
			const FVector Origin(Stage == EClearanceStage::PreIntegrate ? P->GetX() : R->GetP());
			if (!FMath::IsFinite(SpeedSq) || SpeedSq < 0 || SpeedSq >= TNumericLimits<float>::Max()
				|| Origin.ContainsNaN()) { return false; }
			if (Stage == EClearanceStage::PreIntegrate)
			{
				// Chaos clamps integration velocity before updating P. A radius about the
				// actor origin contains any orientation of the native bounded geometry.
				// Contacts/joints/CCD are excluded because their solve displacement is
				// not bounded by MaxLinearSpeedSq.
				double Radius = 0;
				const auto Corners = BoxGeometry(Bounds);
				for (const auto& V : Corners.Primitives[0].Vertices) { Radius = FMath::Max(Radius,V.Size()); }
				const double ReachCm = Radius + FMath::Sqrt(SpeedSq)*Step.DeltaSeconds + 1.e-4;
				if (!FMath::IsFinite(ReachCm)) { return false; }
				World = FBox(Origin,Origin).ExpandBy(ReachCm);
				ActiveEnvelopes.Add(Key,{World,Bounds,P->GetGeometry(),P->GetGeometry()->GetTypeHash(),SpeedSq});
				return true;
			}
			World = WorldBounds(Bounds,FTransform(
				FQuat(Stage == EClearanceStage::PostIntegrate || Stage == EClearanceStage::PostSolve ? R->GetQ() : P->GetR()),Origin));
			const auto* Lease = ActiveEnvelopes.Find(Key);
			if (!Lease || Lease->Geometry != P->GetGeometry() || Lease->GeometryHash != P->GetGeometry()->GetTypeHash()
				|| Lease->MaxLinearSpeedSq != SpeedSq || !Lease->Local.Min.Equals(Bounds.Min,1.e-6)
				|| !Lease->Local.Max.Equals(Bounds.Max,1.e-6)
				|| !Lease->Reach.IsInsideOrOn(World.Min) || !Lease->Reach.IsInsideOrOn(World.Max)) { return false; }
			World = Lease->Reach; return true;
		};
		const auto DormantTopology = [&](const Chaos::FGeometryParticleHandle* P, FDormantLease& Lease)
		{
			const int32 MidphaseCount = P->ParticleCollisions().Num();
			if (!MidphaseCount) { return true; }
			if (MidphaseCount > 128) { return false; }
			const auto& Evolution = *Solver->GetEvolution();
			const int32 CollisionContainer = Evolution.GetCollisionConstraints().GetContainerId();
			if (CollisionContainer < 0 || CollisionContainer >= Evolution.GetIslandManager().GetNumConstraintContainers()) { return false; }
			const auto* Island = Evolution.GetIslandManager().GetParticleIsland(P);
			if (!Island || !Island->IsSleeping() || Island->NeedsResim() || Island->IsUsingCache()
				|| Island->GetNumParticles() > 32 || Island->GetNumParticles() <= 0
				|| Island->GetNumConstraints() <= 0 || Island->GetNumConstraints() > 256
				|| Island->GetNumConstraints() != Island->GetNumContainerConstraints(CollisionContainer))
			{ return false; }
			Lease.IslandConstraints = Island->GetNumConstraints();
			for (int32 I=0; I<Island->GetNumParticles(); ++I)
			{
				const auto* Node = Island->GetNode(I);
				const auto* Member = Node ? Node->GetParticle() : nullptr; FBox Bounds(ForceInit);
				if (!Member || !StableDormant(Member) || !Simulates(*Member) || !NativeBounds(*Member,Bounds)) { return false; }
				Lease.IslandMembers.Add(Member->UniqueIdx().Idx);
			}
			Lease.IslandMembers.Sort();
			if (!Lease.IslandMembers.Contains(P->UniqueIdx().Idx)) { return false; }
			bool Valid = true; int32 ContactCount = 0;
			P->ParticleCollisions().VisitConstMidPhases([&](const Chaos::FParticlePairMidPhase& Mid)
			{
				const auto* Other = Mid.GetParticle0() == P ? Mid.GetParticle1() : Mid.GetParticle1() == P ? Mid.GetParticle0() : nullptr;
				if (!Mid.IsValid() || !Mid.IsSleeping() || !Other) { Valid = false; return Chaos::ECollisionVisitorResult::Stop; }
				Lease.ContactPartners.Add(Other->UniqueIdx().Idx);
				if (Other->ObjectState() == Chaos::EObjectStateType::Sleeping)
				{
					if (!StableDormant(Other) || !Lease.IslandMembers.Contains(Other->UniqueIdx().Idx))
					{ Valid = false; return Chaos::ECollisionVisitorResult::Stop; }
				}
				else if (Other->ObjectState() == Chaos::EObjectStateType::Static)
				{
					FBox Bounds(ForceInit); const FTransform Pose(FQuat(Other->GetR()),FVector(Other->GetX()));
					if (!NativeBounds(*Other,Bounds) || !Simulates(*Other) || Pose.ContainsNaN() || !Pose.GetRotation().IsNormalized())
					{ Valid = false; return Chaos::ECollisionVisitorResult::Stop; }
					Lease.StaticContacts.Add({Other->UniqueIdx().Idx,Pose,Other->GetGeometry(),Other->GetGeometry()->GetTypeHash()});
					// A stale cached manifold must not bootstrap a fresh native lease.
					Mid.VisitConstCollisions([&](const Chaos::FPBDCollisionConstraint& Contact)
					{
						if (++ContactCount > 256) { Valid = false; return Chaos::ECollisionVisitorResult::Stop; }
						const int32 Index = Contact.GetParticle0() == Other ? 0 : 1;
						const auto* Shape = Contact.GetShape(Index);
						bool CurrentShape = false;
						for (const auto& NativeShape : Other->ShapesArray()) { CurrentShape |= NativeShape.Get() == Shape; }
						if (!CurrentShape || !Shape || Contact.GetParticle(Index) != Other || Contact.GetImplicit(Index) != Shape->GetLeafGeometry())
						{ Valid = false; return Chaos::ECollisionVisitorResult::Stop; }
						const auto& Relative = Contact.GetShapeRelativeTransform(Index);
						const auto& Cached = Contact.GetShapeWorldTransform(Index);
						const FTransform Expected = FTransform(FQuat(Relative.GetRotation()),FVector(Relative.GetTranslation()))*Pose;
						if (!Contact.IsSleeping() || !Expected.Equals(FTransform(FQuat(Cached.GetRotation()),FVector(Cached.GetTranslation())),1.e-6))
						{ Valid = false; return Chaos::ECollisionVisitorResult::Stop; }
						return Chaos::ECollisionVisitorResult::Continue;
					});
				}
				else { Valid = false; }
				return Valid ? Chaos::ECollisionVisitorResult::Continue : Chaos::ECollisionVisitorResult::Stop;
			});
			Lease.ContactPartners.Sort();
			Lease.StaticContacts.Sort([](const FStaticContactParticipant& L, const FStaticContactParticipant& R) { return L.Id < R.Id; });
			return Valid;
		};
		for (auto& P : Solver->GetParticles().GetNonDisabledView())
		{
			if (P.PhysicsProxy() == Body) { SawBody = true; continue; }
			SawSupports[0] |= P.PhysicsProxy() == Supports[0]; SawSupports[1] |= P.PhysicsProxy() == Supports[1];
			const bool HitsTraveller = Interacts(Particle,P.Handle());
			// Certify every simulation sleeper, including those filtered out against
			// the traveller. Otherwise a wake source could propagate through an
			// unexamined sleeping island into an admitted obstacle.
			const bool Dormant = P.ObjectState() == Chaos::EObjectStateType::Sleeping && Simulates(P);
			if (!HitsTraveller && !Dormant) { continue; }
			FBox Bounds(ForceInit);
			if (!NativeBounds(P,Bounds))
			{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::NativeBounds); continue; }
			FBox World = WorldBounds(Bounds,FTransform(FQuat(P.GetR()),FVector(P.GetX())));
			if (P.ObjectState() == Chaos::EObjectStateType::Kinematic)
			{
				if (!KinematicInterval(P.Handle(),Bounds,World))
				{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::KinematicMotion); continue; }
			}
			else if (P.ObjectState() == Chaos::EObjectStateType::Dynamic)
			{
				++SeenActive;
				if (!ActiveInterval(P.Handle(),Bounds,World))
				{
					Unsupported = true;
					const uint32 PriorIssues = SceneIssues;
					const auto* OtherRigid = P.CastToRigidParticle();
					const double OtherSpeedSq = OtherRigid ? OtherRigid->MaxLinearSpeedSq() : 0;
					if (OtherRigid && (!FMath::IsFinite(OtherSpeedSq) || OtherSpeedSq >= TNumericLimits<float>::Max()))
					{ MarkSceneIssue(ENativeSceneIssue::UnboundedActive); }
					if (OtherRigid && OtherRigid->CCDEnabled()) { MarkSceneIssue(ENativeSceneIssue::ActiveCCD); }
					if (!P.ParticleConstraints().IsEmpty()) { MarkSceneIssue(ENativeSceneIssue::ActiveConstraint); }
					if (P.ParticleCollisions().Num() != 0) { MarkSceneIssue(ENativeSceneIssue::ActiveContact); }
					if (SceneIssues == PriorIssues) { MarkSceneIssue(ENativeSceneIssue::ActiveInvalidState); }
					continue;
				}
			}
			else if (P.ObjectState() == Chaos::EObjectStateType::Sleeping)
			{
				if (!StableDormant(P.Handle()))
				{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::DormantBody); continue; }
				const FTransform Pose(FQuat(P.GetR()),FVector(P.GetX())); const int32 Key = P.UniqueIdx().Idx;
				if (Pose.ContainsNaN() || !Pose.GetRotation().IsNormalized())
				{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::DormantBody); continue; }
				FDormantLease CurrentLease{Bounds,Pose,P.GetGeometry(),P.GetGeometry()->GetTypeHash()};
				if (!DormantTopology(P.Handle(),CurrentLease))
				{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::DormantBody); continue; }
				if (Stage == EClearanceStage::PreIntegrate)
				{ DormantLeases.Add(Key,MoveTemp(CurrentLease)); }
				else
				{
					const auto* Lease = DormantLeases.Find(Key);
					if (!Lease || Lease->Geometry != P.GetGeometry() || Lease->GeometryHash != P.GetGeometry()->GetTypeHash()
						|| !Lease->Local.Min.Equals(Bounds.Min,1.e-6) || !Lease->Local.Max.Equals(Bounds.Max,1.e-6)
						|| !Lease->Pose.Equals(Pose,1.e-6) || Lease->IslandMembers != CurrentLease.IslandMembers
						|| Lease->ContactPartners != CurrentLease.ContactPartners || Lease->StaticContacts != CurrentLease.StaticContacts
						|| Lease->IslandConstraints != CurrentLease.IslandConstraints)
					{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::DormantBody); continue; }
				}
				Sleepers.Add(P.Handle());
			}
			else if (P.ObjectState() != Chaos::EObjectStateType::Static)
			{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::OtherParticleState); continue; }
			if (!HitsTraveller) { continue; }
			if (P.PhysicsProxy() != Supports[Endpoint]) { SourceBlocked |= Source.Intersect(World); }
			if (P.PhysicsProxy() != Supports[1-Endpoint]) { DestinationBlocked |= Destination.Intersect(World); }
		}
		// A speed cap bounds integration, not contact-solver displacement. Before
		// granting any support bypass, exclude every possible contact of a leased
		// active body for the entire interval. An interacting second non-static
		// body is outside this independent-body profile, even when distant.
		if (!ActiveEnvelopes.IsEmpty())
		{
			const auto& Settings = Solver->GetEvolution()->GetCollisionConstraints().GetDetectorSettings();
			static const auto* Ref = IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.Collision.CullDistanceReferenceSize"));
			static const auto* Min = IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.Collision.MinCullDistanceScale"));
			if (!Ref || !Min || !Settings.bFilteringEnabled || Settings.bDeferNarrowPhase
				|| ActiveEnvelopes.Num() > 32 || ActiveEnvelopes.Num()*Solver->GetParticles().GetNonDisabledView().Num() > 16384)
			{ Unsupported = true; MarkSceneIssue(!Ref || !Min || !Settings.bFilteringEnabled || Settings.bDeferNarrowPhase
				? ENativeSceneIssue::CollisionSettings : ENativeSceneIssue::LeaseBudget); }
			else for (auto& Moving : Solver->GetParticles().GetNonDisabledView())
			{
				const auto* Lease = ActiveEnvelopes.Find(Moving.UniqueIdx().Idx);
				if (!Lease) { continue; }
				for (auto& Other : Solver->GetParticles().GetNonDisabledView())
				{
					if (Other.Handle() == Moving.Handle() || Other.PhysicsProxy() == Body
						|| !Interacts(Moving.Handle(),Other.Handle())) { continue; }
					if (Other.ObjectState() != Chaos::EObjectStateType::Static)
					{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::ActivePairInteraction); break; }
					FBox OtherLocal(ForceInit);
					if (!NativeBounds(Other,OtherLocal))
					{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::NativeBounds); break; }
					const double Size = FMath::Max(Lease->Local.GetSize().GetMax(),OtherLocal.GetSize().GetMax());
					const double Travel = FMath::Sqrt(Lease->MaxLinearSpeedSq)*Step.DeltaSeconds;
					const double Padding = Settings.BoundsExpansion*FMath::Max(Min->GetFloat(),Size*Ref->GetFloat())
						+ FMath::Min(FMath::Max(Settings.BoundsVelocityInflation,Settings.BoundsVelocityInflationMACD)*Travel,
							FMath::Max(Settings.MaxVelocityBoundsExpansion,Settings.MaxVelocityBoundsExpansionMACD)) + 1.e-4;
					if (!FMath::IsFinite(Padding) || Padding < 0 || !FMath::IsFinite(Travel)
						|| Lease->Reach.Intersect(WorldBounds(OtherLocal,
							FTransform(FQuat(Other.GetR()),FVector(Other.GetX()))).ExpandBy(Padding)))
					{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::ActiveStaticReach); break; }
				}
				if (Unsupported) { break; }
			}
		}
		if (!Sleepers.IsEmpty())
		{
			if (Sleepers.Num() > 32 || Sleepers.Num()*Solver->GetParticles().GetNonDisabledView().Num() > 16384)
			{ MarkSceneIssue(ENativeSceneIssue::LeaseBudget); return Reject(EStaticClearanceReason::UnsupportedScene); }
			const auto& Settings = Solver->GetEvolution()->GetCollisionConstraints().GetDetectorSettings();
			static const auto* Ref = IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.Collision.CullDistanceReferenceSize"));
			static const auto* Min = IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.Collision.MinCullDistanceScale"));
			if (!Ref || !Min || !Settings.bFilteringEnabled || Settings.bDeferNarrowPhase)
			{ MarkSceneIssue(ENativeSceneIssue::CollisionSettings); return Reject(EStaticClearanceReason::UnsupportedScene); }
			const double Values[5] = {Settings.BoundsExpansion,FMath::Max(Settings.BoundsVelocityInflation,Settings.BoundsVelocityInflationMACD),
				FMath::Max(Settings.MaxVelocityBoundsExpansion,Settings.MaxVelocityBoundsExpansionMACD),Ref->GetFloat(),Min->GetFloat()};
			for (int32 I=0; I<5; ++I)
			{
				if (!FMath::IsFinite(Values[I]) || Values[I] < 0 || (Stage != EClearanceStage::PreIntegrate && DormantCollisionSettings[I] != Values[I]))
				{ MarkSceneIssue(ENativeSceneIssue::CollisionSettings); return Reject(EStaticClearanceReason::UnsupportedScene); }
				DormantCollisionSettings[I] = Values[I];
			}
			for (const auto* Sleeper : Sleepers)
			{
				const auto& Lease = DormantLeases.FindChecked(Sleeper->UniqueIdx().Idx);
				const FBox DormantWorld = WorldBounds(Lease.Local,Lease.Pose);
				for (auto& Other : Solver->GetParticles().GetNonDisabledView())
				{
					if (!Interacts(Sleeper,Other.Handle())) { continue; }
					if (Other.ObjectState() == Chaos::EObjectStateType::Static) { continue; }
					if (Other.ObjectState() == Chaos::EObjectStateType::Sleeping)
					{ if (!StableDormant(Other.Handle())) { WakeRisk = true; } continue; }
					FBox OtherBounds(ForceInit), Reach(ForceInit);
					if (!NativeBounds(Other,OtherBounds)) { WakeRisk = true; continue; }
					if (Other.PhysicsProxy() == Body) { Reach = Source+Destination; }
					else if (Other.ObjectState() == Chaos::EObjectStateType::Kinematic)
					{
						Reach = WorldBounds(OtherBounds,FTransform(FQuat(Other.GetR()),FVector(Other.GetX())));
						if (!KinematicInterval(Other.Handle(),OtherBounds,Reach)) { WakeRisk = true; continue; }
					}
					else { WakeRisk = true; continue; } // Active dynamic reach includes unknown future solve impulses.
					const double Size = FMath::Max(FMath::Max(Lease.Local.GetSize().GetMax(),OtherBounds.GetSize().GetMax()),
						FMath::Max(static_cast<double>(Sleeper->LocalBounds().Extents().GetMax()),static_cast<double>(Other.LocalBounds().Extents().GetMax())));
					const auto* OtherRigid = Other.CastToRigidParticle();
					const double PreviousTravel = FMath::Max(FVector(Sleeper->CastToRigidParticle()->GetPreV()).Size(),
						OtherRigid ? FVector(OtherRigid->GetPreV()).Size() : 0.)*Step.DeltaSeconds;
					const double CurrentTravel = Other.PhysicsProxy() == Body
						? FMath::Max3(A.LinearVelocity.Size()*Step.DeltaSeconds,B.LinearVelocity.Size()*Step.DeltaSeconds,
							FVector::Dist(A.Pose.GetLocation(),B.Pose.GetLocation()))
						: Reach.GetSize().Size(); // Contains the complete remaining native kinematic target.
					const double Padding = Values[0]*FMath::Max(Values[4],Size*Values[3])
						+ FMath::Min(Values[1]*FMath::Max(PreviousTravel,CurrentTravel),Values[2])+1.e-4;
					const FBox Padded = DormantWorld.ExpandBy(Padding);
					WakeRisk |= !FMath::IsFinite(PreviousTravel) || !FMath::IsFinite(CurrentTravel) || !FMath::IsFinite(Padding) || (Other.PhysicsProxy() == Body
						? Padded.Intersect(Source) || Padded.Intersect(Destination) : Padded.Intersect(Reach));
				}
			}
		}
		if (!SawBody || !SawSupports[0] || !SawSupports[1]) { Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		if (Stage != EClearanceStage::PreIntegrate && SeenActive != ActiveEnvelopes.Num())
		{ Unsupported = true; MarkSceneIssue(ENativeSceneIssue::ActiveInvalidState); }
		if (Unsupported) { return Reject(EStaticClearanceReason::UnsupportedScene); }
		if (SourceBlocked) { return Reject(EStaticClearanceReason::SourceBlocked); }
		if (DestinationBlocked) { return Reject(EStaticClearanceReason::DestinationBlocked); }
		if (WakeRisk) { return Reject(EStaticClearanceReason::UncertifiedDynamicInteraction); }
		Proof.Result = EStaticClearanceReason::Clear; return Proof;
	}
	bool FChaosStaticClearance::Consume_Internal(const FStaticClearanceProof& P, const FBoundaryCommand& C,
		const FBoundaryState& A, const FBoundaryState& B, const FPhysicsStepKey& Step, EClearanceStage Stage)
	{
		using namespace StaticClearanceDetail;
		if (Retired || Consumed || P.Result != EStaticClearanceReason::Clear || P.Binding != Binding || P.Sequence != Sequence
			|| P.Stage != Stage || LastStage != Stage || !SameStep(P.Step,Step) || !SameStep(Step,IntervalStep)
			|| !SameRequest(P.Command,C) || !SameState(P.Start,A) || !SameState(P.End,B)) { return false; }
		Consumed = true; return true;
	}
}
