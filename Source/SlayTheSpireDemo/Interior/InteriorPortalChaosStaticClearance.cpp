#include "InteriorPortalChaosStaticClearance.h"
#include "InteriorPortalMath.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/PBDRigidsSOAs.h"
#include "Chaos/ShapeInstance.h"
#include "Chaos/Collision/CollisionFilter.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"

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
			return R && P->ObjectState() == Chaos::EObjectStateType::Sleeping
				&& FMath::IsFinite(R->M()) && R->M() > 0 && R->InvM() > 0
				&& FVector(R->GetV()).IsNearlyZero(1.e-6) && FVector(R->GetW()).IsNearlyZero(1.e-6)
				&& FVector(R->Acceleration()).IsNearlyZero(1.e-6) && FVector(R->AngularAcceleration()).IsNearlyZero(1.e-6)
				&& FVector(R->LinearImpulseVelocity()).IsNearlyZero(1.e-6) && FVector(R->AngularImpulseVelocity()).IsNearlyZero(1.e-6)
				&& P->ParticleConstraints().IsEmpty() && P->ParticleCollisions().Num() == 0;
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
	{ Retired = true; Consumed = true; KinematicEnvelopes.Reset(); DormantLeases.Reset(); Body = nullptr; Supports[0] = Supports[1] = nullptr; Solver = nullptr; }
	FStaticClearanceProof FChaosStaticClearance::Certify_Internal(const FBoundaryCommand& C,
		const FBoundaryState& A, const FBoundaryState& B, const FPhysicsStepKey& Step, EClearanceStage Stage, int32 Endpoint)
	{
		using namespace StaticClearanceDetail;
		FStaticClearanceProof Proof;
		if (Sequence == MAX_uint64) { Retire_Internal(); }
		Proof.Binding = Binding; Proof.Sequence = Sequence == MAX_uint64 ? Sequence : ++Sequence; Consumed = false;
		Proof.Command = C; Proof.Start = A; Proof.End = B; Proof.Step = Step; Proof.Stage = Stage;
		const auto Reject = [&](EStaticClearanceReason Reason) { Proof.Result = Reason; return Proof; };
		if (Retired) { return Reject(EStaticClearanceReason::Retired); }
		const FTransform ExpectedEntry = Endpoint == 0 ? Bound.Entry : Bound.Exit, ExpectedExit = Endpoint == 0 ? Bound.Exit : Bound.Entry;
		if (Endpoint < 0 || Endpoint > 1 || C.Traveller.Handle != Bound.Traveller.Handle || C.PairGeneration != Bound.PairGeneration
			|| !(C.Traveller.Geometry == Bound.Traveller.Geometry) || !C.Entry.Equals(ExpectedEntry,0) || !C.Exit.Equals(ExpectedExit,0)
			|| C.HalfWidth != Bound.HalfWidth || C.HalfHeight != Bound.HalfHeight)
		{ Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		if (Step.SolverEpoch != Epoch || Step.SolverFrame != Solver->GetCurrentFrame() || !Step.EvolutionSerial
			|| !C.Revision || !FMath::IsFinite(Step.DeltaSeconds) || Step.DeltaSeconds <= 0 || Step.DeltaSeconds > C.MaxStepSeconds + 1.e-9)
		{ return Reject(EStaticClearanceReason::InvalidInterval); }
		if (Stage == EClearanceStage::PreIntegrate)
		{
			if (IntervalStep.SolverEpoch && (Step.EvolutionSerial <= IntervalStep.EvolutionSerial || Step.SolverFrame <= IntervalStep.SolverFrame))
			{ return Reject(EStaticClearanceReason::InvalidInterval); }
			IntervalStep = Step; IntervalStart = A; IntervalCommand = C; LastStage = Stage;
			KinematicEnvelopes.Reset();
			DormantLeases.Reset();
		}
		else
		{
			if (!SameStep(Step,IntervalStep) || !SameState(A,IntervalStart) || !SameRequest(C,IntervalCommand)
				|| static_cast<uint8>(Stage) != static_cast<uint8>(LastStage)+1)
			{ return Reject(EStaticClearanceReason::InvalidInterval); }
			LastStage = Stage;
		}
		auto* H = Body->GetPhysicsThreadAPI(); auto* Particle = Body->GetHandle_LowLevel();
		const auto* Rigid = Particle ? Particle->CastToRigidParticle() : nullptr;
		FBox Local(ForceInit); const FBox Registered = RegisteredBounds(C.Traveller.Geometry);
		if (!H || !Rigid || (H->ObjectState() != Chaos::EObjectStateType::Dynamic && H->ObjectState() != Chaos::EObjectStateType::Sleeping)
			|| !Simulates(*Particle) || !NativeBounds(*Particle,Local) || !Registered.IsValid
			|| !Local.Min.Equals(Registered.Min,1.e-6) || !Local.Max.Equals(Registered.Max,1.e-6)
			|| !FVector(H->CenterOfMass()).Equals(C.LocalAuthorityReference,1.e-6))
		{ Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		// Persistent constraints can move/rotate the body after this hook; they
		// are not collision constraints and are outside the independent-body profile.
		if (!Particle->ParticleConstraints().IsEmpty()) { return Reject(EStaticClearanceReason::UnsupportedScene); }
		const FTransform Actual(Stage == EClearanceStage::PreIntegrate ? FQuat(H->R()) : FQuat(Rigid->GetQ()),
			Stage == EClearanceStage::PreIntegrate ? FVector(H->X()) : FVector(Rigid->GetP()),C.Traveller.Geometry.BakedScale);
		const auto& ActualSample = Stage == EClearanceStage::PreIntegrate ? A : B;
		if (!ActualSample.Pose.Equals(Actual,1.e-6) || !ActualSample.LinearVelocity.Equals(FVector(H->V()),1.e-6)
			|| !ActualSample.AngularVelocity.Equals(FVector(H->W()),1.e-6))
		{ return Reject(EStaticClearanceReason::InvalidInterval); }
		const FVector Normal = C.Entry.GetUnitAxis(EAxis::X);
		const auto NormalOnly = [&](const FVector& V) { return !V.ContainsNaN() && (V-Normal*FVector::DotProduct(V,Normal)).IsNearlyZero(1.e-6); };
		if (!NormalOnly(A.LinearVelocity) || !NormalOnly(B.LinearVelocity) || !NormalOnly(FVector(H->Acceleration()))
			|| !FVector(H->AngularAcceleration()).IsNearlyZero(1.e-6)
			|| !A.AngularVelocity.IsNearlyZero(1.e-6) || !B.AngularVelocity.IsNearlyZero(1.e-6)
			|| !A.Pose.GetRotation().Equals(B.Pose.GetRotation(),1.e-8)
			|| FVector::Dist(A.Pose.GetLocation(),B.Pose.GetLocation()) > C.MaxTranslationCm)
		{ return Reject(EStaticClearanceReason::UnsupportedMotion); }
		FBox SupportBounds[2];
		for (int32 I=0; I<2; ++I)
		{
			const auto* P = Supports[I]->GetHandle_LowLevel();
			if (!P || P->ObjectState() != Chaos::EObjectStateType::Static || !Simulates(*P) || !Interacts(Particle,P) || !NativeBounds(*P,SupportBounds[I])
				|| !SupportBounds[I].Min.Equals(BoundSupportBounds[I].Min,1.e-6) || !SupportBounds[I].Max.Equals(BoundSupportBounds[I].Max,1.e-6)
				|| !FTransform(FQuat(P->GetR()),FVector(P->GetX())).Equals(SupportPoses[I],1.e-6))
			{ Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		}
		const Chaos::FImplicitObject* Geometries[3] = {Particle->GetGeometry(),Supports[0]->GetHandle_LowLevel()->GetGeometry(),Supports[1]->GetHandle_LowLevel()->GetGeometry()};
		for (int32 I=0; I<3; ++I)
		{
			const uint32 Hash = Geometries[I]->GetTypeHash();
			if (GeometryBound && (NativeGeometry[I] != Geometries[I] || GeometryHashes[I] != Hash))
			{ Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
			NativeGeometry[I] = Geometries[I]; GeometryHashes[I] = Hash;
		}
		GeometryBound = true;
		const FGeometry Box = BoxGeometry(Local,C.Traveller.Geometry.BakedScale);
		FTransform AtPlane = B.Pose;
		AtPlane.AddToTranslation(-Normal*C.Entry.InverseTransformPositionNoScale(B.Pose.TransformPositionNoScale(C.LocalAuthorityReference)).X);
		const FQuat Map = InteriorPortalMath::Rotation(C.Entry,C.Exit);
		const auto Mapped = [&](const FTransform& T)
		{ return FTransform(Map*T.GetRotation(),InteriorPortalMath::Position(T.GetLocation(),C.Entry,C.Exit),C.Traveller.Geometry.BakedScale); };
		const auto SourceSpan = EvaluatePose(BoxGeometry(SupportBounds[Endpoint]),SupportPoses[Endpoint],C.Entry,1.e100,1.e100);
		const auto ExitSpan = EvaluatePose(BoxGeometry(SupportBounds[1-Endpoint]),SupportPoses[1-Endpoint],C.Exit,1.e100,1.e100);
		if (!SourceSpan.Fits() || !ExitSpan.Fits() || SourceSpan.MinNormal > 0 || SourceSpan.MaxNormal < 0
			|| ExitSpan.MinNormal > 0 || ExitSpan.MaxNormal < 0 || SourceSpan.MaxNormal > C.SupportHalfThicknessCm + 1.e-6)
		{ Retire_Internal(); return Reject(EStaticClearanceReason::BindingChanged); }
		FTransform ExitClear = Mapped(AtPlane);
		const auto Span = EvaluatePose(Box,ExitClear,C.Exit,C.HalfWidth,C.HalfHeight,C.MarginCm);
		ExitClear.AddToTranslation(C.Exit.GetUnitAxis(EAxis::X)*(FMath::Max(0.,ExitSpan.MaxNormal-Span.MinNormal+C.MarginCm)+C.MaxTranslationCm));
		if (!EvaluateTranslation(Box,A.Pose,B.Pose,C.Entry,C.HalfWidth,C.HalfHeight,C.MarginCm).Fits()
			|| !EvaluateTranslation(Box,B.Pose,AtPlane,C.Entry,C.HalfWidth,C.HalfHeight,C.MarginCm).Fits()
			|| !EvaluateTranslation(Box,Mapped(A.Pose),Mapped(B.Pose),C.Exit,C.HalfWidth,C.HalfHeight,C.MarginCm).Fits()
			|| !EvaluateTranslation(Box,Mapped(AtPlane),ExitClear,C.Exit,C.HalfWidth,C.HalfHeight,C.MarginCm).Fits())
		{ return Reject(EStaticClearanceReason::OutsideAperture); }
		const FBox Source = SweptBounds(Local,A.Pose,B.Pose,C.MarginCm) + SweptBounds(Local,B.Pose,AtPlane,C.MarginCm);
		const FBox Destination = SweptBounds(Local,Mapped(A.Pose),Mapped(B.Pose),C.MarginCm)
			+ SweptBounds(Local,Mapped(AtPlane),ExitClear,C.MarginCm);
		bool Unsupported = false, SourceBlocked = false, DestinationBlocked = false, SawBody = false;
		bool WakeRisk = false;
		TArray<const Chaos::FGeometryParticleHandle*> Sleepers;
		bool SawSupports[2] = {false,false};
		if (Solver->GetParticles().GetNonDisabledView().Num() > 4096) { return Reject(EStaticClearanceReason::UnsupportedScene); }
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
			if (!NativeBounds(P,Bounds)) { Unsupported = true; continue; }
			FBox World = WorldBounds(Bounds,FTransform(FQuat(P.GetR()),FVector(P.GetX())));
			if (P.ObjectState() == Chaos::EObjectStateType::Kinematic)
			{
				if (!KinematicInterval(P.Handle(),Bounds,World)) { Unsupported = true; continue; }
			}
			else if (P.ObjectState() == Chaos::EObjectStateType::Sleeping)
			{
				if (!StableDormant(P.Handle())) { Unsupported = true; continue; }
				const FTransform Pose(FQuat(P.GetR()),FVector(P.GetX())); const int32 Key = P.UniqueIdx().Idx;
				if (Pose.ContainsNaN() || !Pose.GetRotation().IsNormalized()) { Unsupported = true; continue; }
				if (Stage == EClearanceStage::PreIntegrate)
				{ DormantLeases.Add(Key,{Bounds,Pose,P.GetGeometry(),P.GetGeometry()->GetTypeHash()}); }
				else
				{
					const auto* Lease = DormantLeases.Find(Key);
					if (!Lease || Lease->Geometry != P.GetGeometry() || Lease->GeometryHash != P.GetGeometry()->GetTypeHash()
						|| !Lease->Local.Min.Equals(Bounds.Min,1.e-6) || !Lease->Local.Max.Equals(Bounds.Max,1.e-6)
						|| !Lease->Pose.Equals(Pose,1.e-6)) { Unsupported = true; continue; }
				}
				Sleepers.Add(P.Handle());
			}
			else if (P.ObjectState() != Chaos::EObjectStateType::Static) { Unsupported = true; continue; }
			if (!HitsTraveller) { continue; }
			if (P.PhysicsProxy() != Supports[Endpoint]) { SourceBlocked |= Source.Intersect(World); }
			if (P.PhysicsProxy() != Supports[1-Endpoint]) { DestinationBlocked |= Destination.Intersect(World); }
		}
		if (!Sleepers.IsEmpty())
		{
			if (Sleepers.Num() > 32 || Sleepers.Num()*Solver->GetParticles().GetNonDisabledView().Num() > 16384)
			{ return Reject(EStaticClearanceReason::UnsupportedScene); }
			const auto& Settings = Solver->GetEvolution()->GetCollisionConstraints().GetDetectorSettings();
			static const auto* Ref = IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.Collision.CullDistanceReferenceSize"));
			static const auto* Min = IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.Collision.MinCullDistanceScale"));
			if (!Ref || !Min || !Settings.bFilteringEnabled || Settings.bDeferNarrowPhase)
			{ return Reject(EStaticClearanceReason::UnsupportedScene); }
			const double Values[5] = {Settings.BoundsExpansion,FMath::Max(Settings.BoundsVelocityInflation,Settings.BoundsVelocityInflationMACD),
				FMath::Max(Settings.MaxVelocityBoundsExpansion,Settings.MaxVelocityBoundsExpansionMACD),Ref->GetFloat(),Min->GetFloat()};
			for (int32 I=0; I<5; ++I)
			{
				if (!FMath::IsFinite(Values[I]) || Values[I] < 0 || (Stage != EClearanceStage::PreIntegrate && DormantCollisionSettings[I] != Values[I]))
				{ return Reject(EStaticClearanceReason::UnsupportedScene); }
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
