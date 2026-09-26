#include "InteriorPortalPassageCoordinator.h"
#include "InteriorPortalMath.h"

namespace InteriorPortalPhysics
{
	namespace
	{
		bool ValidHandle(const FTravellerHandle& H) { return H.Epoch && H.Id && H.Generation; }
		bool ValidStep(const FPhysicsStepKey& S)
		{ return S.SolverEpoch && S.SolverFrame >= 0 && S.EvolutionSerial && FMath::IsFinite(S.DeltaSeconds) && S.DeltaSeconds > 0; }
		bool SameStep(const FPhysicsStepKey& A, const FPhysicsStepKey& B)
		{ return A.SolverEpoch == B.SolverEpoch && A.SolverFrame == B.SolverFrame && A.EvolutionSerial == B.EvolutionSerial && A.DeltaSeconds == B.DeltaSeconds; }
		bool LaterStep(const FPhysicsStepKey& A, const FPhysicsStepKey& B)
		{ return !B.SolverEpoch || (A.SolverEpoch == B.SolverEpoch && A.SolverFrame > B.SolverFrame && A.EvolutionSerial > B.EvolutionSerial); }
		bool ValidMaterial(const FBodyCommitState& S)
		{
			return S.Motion.Pose.IsValid() && !S.Motion.LinearVelocity.ContainsNaN() && !S.Motion.AngularVelocity.ContainsNaN()
				&& FMath::IsFinite(S.MassKg) && S.MassKg > 0 && !S.LocalInertia.ContainsNaN() && S.LocalInertia.GetMin() > 0
				&& !S.LocalCOM.ContainsNaN() && !S.RotationOfMass.ContainsNaN() && S.RotationOfMass.IsNormalized();
		}
		bool RigidFrame(const FTransform& F) { return F.IsValid() && F.GetScale3D().Equals(FVector::OneVector, 1.e-6); }
	}
	EFactResult FTransferFactCursor::Consume(const FTransferFact& F)
	{
		if (!ValidHandle(DomainBody) || !Epoch || !Binding || F.Body != DomainBody || F.SolverEpoch != Epoch
			|| F.BindingEpoch != Binding || F.Step.SolverEpoch != Epoch)
		{ return EFactResult::WrongDomain; }
		if (!F.Revision || !F.PairGeneration || !ValidStep(F.Step) || F.FromEndpoint < 0 || F.FromEndpoint > 1
			|| F.ToEndpoint != 1 - F.FromEndpoint || !ValidMaterial(F.Before) || !ValidMaterial(F.After)
			|| F.Before.MassKg != F.After.MassKg || F.Before.LocalInertia != F.After.LocalInertia
			|| F.Before.LocalCOM != F.After.LocalCOM || F.Before.RotationOfMass != F.After.RotationOfMass
			|| F.Before.Sleeping != F.After.Sleeping)
		{ return EFactResult::Invalid; }
		if (F.Revision <= Revision) { return EFactResult::Duplicate; }
		if (Revision == MAX_uint64 || F.Revision != Revision + 1) { return EFactResult::Gap; }
		if (!LaterStep(F.Step, LastStep)) { return EFactResult::Invalid; }
		Revision = F.Revision; LastStep = F.Step;
		return EFactResult::Consumed;
	}
	FPassageCoordinator::FPassageCoordinator(FTravellerHandle InBody, uint64 InPair, uint64 SolverEpoch, uint64 BindingEpoch,
		const FTransform& Endpoint0, const FTransform& Endpoint1, int32 InitialEndpoint, int32 JournalCapacity)
		: Body(InBody), Pair(InPair), Epoch(SolverEpoch), Binding(BindingEpoch), Endpoint(InitialEndpoint), Capacity(JournalCapacity)
	{
		Frames[0] = Endpoint0; Frames[1] = Endpoint1;
		if (!ValidHandle(Body) || !Pair || !Epoch || !Binding || Endpoint < 0 || Endpoint > 1 || Capacity <= 0 || Capacity > 1024
			|| !RigidFrame(Frames[0]) || !RigidFrame(Frames[1])) { Endpoint = 0; Retire(); }
	}
	bool FPassageCoordinator::Matches(const FBoundaryCommand& C, const FPhysicsStepKey& Step) const
	{
		return Passage != EPassagePhase::Retired && C.Traveller.Handle == Body && C.PairGeneration == Pair
			&& Step.SolverEpoch == Epoch && ValidStep(Step) && C.Entry.Equals(Frames[Endpoint], 0)
			&& C.Exit.Equals(Frames[1 - Endpoint], 0);
	}
	FBoundaryDecision FPassageCoordinator::EvaluateInterval(const FBoundaryCommand& C, const FBoundaryState& A,
		const FBoundaryState& B, const FPhysicsStepKey& Step, uint32 Reuse, bool OtherContact)
	{
		if (!Matches(C, Step) || !LaterStep(Step, CurrentStep))
		{
			RevokeInterval(EBoundaryReason::InvalidCommand); return CurrentDecision;
		}
		CurrentStep = Step; IntervalStart = A; IntervalCommand = C; IntervalCommandRevision = C.Revision; IntervalReuse = Reuse;
		CurrentDecision = EvaluateBoundary(C, A, B, Step, Reuse, OtherContact);
		if (Journal.Num() >= Capacity || CommittedRevision == MAX_uint64 || RelationRevision == MAX_uint64)
		{ RevokeInterval(EBoundaryReason::Disabled); }
		if (CurrentDecision.BypassSupport)
		{
			const auto Fit = EvaluatePose(C.Traveller.Geometry, B.Pose, C.Entry, C.HalfWidth, C.HalfHeight, C.MarginCm);
			if (Passage == EPassagePhase::Exiting && Fit.MinNormal > C.SupportHalfThicknessCm + 1.) { Passage = EPassagePhase::Outside; }
			else if (Passage != EPassagePhase::Exiting)
			{ Passage = Fit.MinNormal > C.SupportHalfThicknessCm + 1. ? EPassagePhase::Outside
				: (Fit.MinNormal <= C.SupportHalfThicknessCm ? EPassagePhase::Straddling : EPassagePhase::Entering); }
		}
		return CurrentDecision;
	}
	void FPassageCoordinator::RevokeInterval(EBoundaryReason Reason)
	{ CurrentDecision = {Reason, false, false}; }
	FHoldRelationSnapshot FPassageCoordinator::Hold() const
	{
		auto R = Relation;
		R.Route.Revision = RelationRevision; R.Route.PairGeneration = Pair;
		R.Route.Kind = HolderEndpoint == Endpoint ? EHoldRoute::Direct : EHoldRoute::SinglePair;
		R.Route.HolderSide = Frames[HolderEndpoint]; R.Route.BodySide = Frames[Endpoint];
		return R;
	}
	bool FPassageCoordinator::AcquireHold(uint64 Instance, FTravellerHandle Holder, int32 Side,
		const FTransform& Desired, const FVector& Anchor)
	{
		if (Passage == EPassagePhase::Retired || Relation.Active || !Instance || Instance <= LastHoldInstance
			|| RelationRevision == MAX_uint64 || !ValidHandle(Holder) || Side < 0 || Side > 1
			|| !Desired.IsValid() || Anchor.ContainsNaN()) { return false; }
		Relation = {}; Relation.Instance = Instance; Relation.Holder = Holder; Relation.LocalAnchor = Anchor;
		Relation.DesiredHolderPose = Desired; Relation.Active = true; HolderEndpoint = Side;
		LastHoldInstance = Instance; ++RelationRevision;
		return true;
	}
	bool FPassageCoordinator::SetHolderIntent(uint64 Instance, FTravellerHandle Holder, uint64 HolderRevision, const FTransform& Desired)
	{
		if (Passage == EPassagePhase::Retired || !Relation.Active || Instance != Relation.Instance || Holder != Relation.Holder
			|| HolderRevision != Relation.HolderTransferRevision || !Desired.IsValid()) { return false; }
		Relation.DesiredHolderPose = Desired; return true;
	}
	bool FPassageCoordinator::ReleaseHold(uint64 Instance)
	{
		if (Instance != Relation.Instance || !Instance) { return false; }
		if (Relation.Active) { Relation.Active = false; if (RelationRevision != MAX_uint64) { ++RelationRevision; } }
		return true;
	}
	bool FPassageCoordinator::HolderCrossed(uint64 Instance, FTravellerHandle Holder, uint64 Revision, int32 From)
	{
		if (Passage == EPassagePhase::Retired || !Relation.Active || Instance != Relation.Instance || Holder != Relation.Holder || !Revision)
		{ return false; }
		if (Revision == Relation.HolderTransferRevision) { return true; }
		if (Relation.HolderTransferRevision == MAX_uint64 || Revision != Relation.HolderTransferRevision + 1
			|| From != HolderEndpoint || RelationRevision == MAX_uint64)
		{ ReleaseHold(Instance); return false; }
		Relation.DesiredHolderPose = InteriorPortalMath::BuildVirtualViewTransform(Relation.DesiredHolderPose, Frames[From], Frames[1-From]);
		HolderEndpoint = 1-From; Relation.HolderTransferRevision = Revision; ++RelationRevision;
		return true;
	}
	ETransferResult FPassageCoordinator::CommitSolved(const FBoundaryCommand& C, const FBodyCommitState& Solved,
		const FPhysicsStepKey& Step, IPortalTransferAdapter& Adapter)
	{
		if (Passage == EPassagePhase::Retired) { return ETransferResult::Retired; }
		if (SameStep(Step, LastCommitStep)) { return ETransferResult::AlreadyCommitted; }
		if (Journal.Num() >= Capacity) { return ETransferResult::JournalFull; }
		if (!Matches(C, Step) || !SameStep(Step, CurrentStep) || C.Revision != IntervalCommandRevision
			|| !ValidMaterial(Solved) || !CurrentDecision.TransferAfterSolve || CommittedRevision == MAX_uint64
			|| RelationRevision == MAX_uint64 || !Solved.LocalCOM.Equals(C.LocalAuthorityReference, 1.e-6)
			|| C.Traveller.Geometry != IntervalCommand.Traveller.Geometry || C.LocalAuthorityReference != IntervalCommand.LocalAuthorityReference
			|| C.HalfWidth != IntervalCommand.HalfWidth || C.HalfHeight != IntervalCommand.HalfHeight || C.MarginCm != IntervalCommand.MarginCm
			|| C.MaxTranslationCm != IntervalCommand.MaxTranslationCm || C.MaxStepSeconds != IntervalCommand.MaxStepSeconds
			|| C.ExitCorridorCertified != IntervalCommand.ExitCorridorCertified || C.PermitTransfer != IntervalCommand.PermitTransfer
			|| C.PermitSupportBypass != IntervalCommand.PermitSupportBypass || C.IsolatedStaticScope != IntervalCommand.IsolatedStaticScope
			|| C.MaxReuseSteps != IntervalCommand.MaxReuseSteps) { return ETransferResult::Rejected; }
		const auto Decision = EvaluateBoundary(IntervalCommand, IntervalStart, Solved.Motion, Step, IntervalReuse, false);
		if (!Decision.TransferAfterSolve) { return ETransferResult::NoCrossing; }
		FTransferFact Fact;
		Fact.Body = Body; Fact.SolverEpoch = Epoch; Fact.BindingEpoch = Binding; Fact.PairGeneration = Pair; Fact.Revision = CommittedRevision + 1;
		Fact.Step = Step; Fact.FromEndpoint = Endpoint; Fact.ToEndpoint = 1 - Endpoint;
		Fact.Before = Solved; Fact.After = Solved;
		const FQuat Q = InteriorPortalMath::Rotation(Frames[Endpoint], Frames[1-Endpoint]);
		Fact.After.Motion.Pose.SetLocation(InteriorPortalMath::Position(Solved.Motion.Pose.GetLocation(),Frames[Endpoint],Frames[1-Endpoint]));
		Fact.After.Motion.Pose.SetRotation(Q * Solved.Motion.Pose.GetRotation());
		Fact.After.Motion.LinearVelocity = Q.RotateVector(Solved.Motion.LinearVelocity);
		Fact.After.Motion.AngularVelocity = Q.RotateVector(Solved.Motion.AngularVelocity);
		if (!EvaluatePose(C.Traveller.Geometry, Fact.After.Motion.Pose, Frames[1-Endpoint], C.HalfWidth, C.HalfHeight, C.MarginCm).Fits())
		{ return ETransferResult::Rejected; }
		Fact.HoldAfter = Relation;
		Fact.HoldAfter.Route.Revision = RelationRevision + 1; Fact.HoldAfter.Route.PairGeneration = Pair;
		Fact.HoldAfter.Route.Kind = HolderEndpoint == Fact.ToEndpoint ? EHoldRoute::Direct : EHoldRoute::SinglePair;
		Fact.HoldAfter.Route.HolderSide = Frames[HolderEndpoint]; Fact.HoldAfter.Route.BodySide = Frames[Fact.ToEndpoint];
		// Reserve before the physical write: a committed fact must always have journal storage.
		Journal.Reserve(Capacity);
		if (!Adapter.Commit(Fact)) { RevokeInterval(EBoundaryReason::Disabled); return ETransferResult::AdapterRejected; }
		Endpoint = Fact.ToEndpoint; Passage = EPassagePhase::Exiting;
		++CommittedRevision; ++RelationRevision; LastCommitStep = Step; Journal.Add(MoveTemp(Fact));
		return ETransferResult::Committed;
	}
	void FPassageCoordinator::Retire()
	{
		Passage = EPassagePhase::Retired; Relation.Active = false;
		RevokeInterval(EBoundaryReason::Disabled);
	}
	bool FPassageCoordinator::Acknowledge(const FTransferAcknowledgment& Ack)
	{
		if (Ack.Body != Body || Ack.SolverEpoch != Epoch || Ack.BindingEpoch != Binding) { return false; }
		const uint64 Revision = Ack.LastConsumedRevision;
		if (Revision < AckRevision || Revision > CommittedRevision) { return false; }
		AckRevision = Revision;
		Journal.RemoveAll([&](const FTransferFact& F) { return F.Revision <= Revision; });
		return true;
	}
}
