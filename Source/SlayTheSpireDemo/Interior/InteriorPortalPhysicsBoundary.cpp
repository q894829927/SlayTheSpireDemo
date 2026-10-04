#include "InteriorPortalPhysicsBoundary.h"

namespace InteriorPortalPhysics
{
	FVector StaticGateProjection(const FVector& V, const FTransform& Frame)
	{
		const FVector N = Frame.GetUnitAxis(EAxis::X);
		return V - N * FMath::Min(0., FVector::DotProduct(V, N));
	}
	FVector BoundedForce(const FBoundaryCommand& C)
	{
		if (C.ForceIntent.ContainsNaN() || !FMath::IsFinite(C.MaxForce) || C.MaxForce <= 0)
		{ return FVector::ZeroVector; }
		return C.ForceIntent.GetClampedToMaxSize(C.MaxForce);
	}
	FBoundaryDecision EvaluateBoundary(const FBoundaryCommand& C, const FBoundaryState& A,
		const FBoundaryState& B, const FPhysicsStepKey& Step, uint32 ReuseOrdinal, bool HasOtherContact)
	{
		const auto Reject = [](EBoundaryReason R) { return FBoundaryDecision{R, false, false}; };
		if (!C.Traveller.Handle.Epoch || !C.Traveller.Handle.Id || !C.Traveller.Handle.Generation
			|| !C.PairGeneration || !C.Revision || !C.MaxReuseSteps || C.LocalAuthorityReference.ContainsNaN() || !Step.SolverEpoch
			|| Step.SolverFrame < 0 || !Step.EvolutionSerial || !FMath::IsFinite(C.MaxStepSeconds)
			|| !FMath::IsFinite(C.MaxTranslationCm) || C.MaxStepSeconds <= 0 || C.MaxTranslationCm <= 0)
		{ return Reject(EBoundaryReason::InvalidCommand); }
		if (ReuseOrdinal >= C.MaxReuseSteps) { return Reject(EBoundaryReason::ExpiredInput); }
		if (!FMath::IsFinite(Step.DeltaSeconds) || Step.DeltaSeconds <= 0 || Step.DeltaSeconds > C.MaxStepSeconds + 1.e-9)
		{ return Reject(EBoundaryReason::UnsupportedStep); }
		if (!C.PermitSupportBypass) { return Reject(EBoundaryReason::Disabled); }
		if (!C.IsolatedStaticScope || HasOtherContact) { return Reject(EBoundaryReason::OtherContact); }
		if (A.Pose.ContainsNaN() || B.Pose.ContainsNaN() || A.LinearVelocity.ContainsNaN()
			|| B.LinearVelocity.ContainsNaN() || A.AngularVelocity.ContainsNaN() || B.AngularVelocity.ContainsNaN())
		{ return Reject(EBoundaryReason::InvalidCommand); }
		if (!A.AngularVelocity.IsNearlyZero(1.e-6) || !B.AngularVelocity.IsNearlyZero(1.e-6)
			|| !A.Pose.GetRotation().Equals(B.Pose.GetRotation(), 1.e-8))
		{ return Reject(EBoundaryReason::UnsupportedRotation); }
		if (FVector::Dist(A.Pose.GetLocation(), B.Pose.GetLocation()) > C.MaxTranslationCm)
		{ return Reject(EBoundaryReason::UnsupportedMotion); }
		if (!EvaluateTranslation(C.Traveller.Geometry, A.Pose, B.Pose, C.Entry,
			C.HalfWidth, C.HalfHeight, C.MarginCm).Fits())
		{ return Reject(EBoundaryReason::OutsideAperture); }
		const double From = C.Entry.InverseTransformPositionNoScale(A.Pose.TransformPositionNoScale(C.LocalAuthorityReference)).X;
		const double To = C.Entry.InverseTransformPositionNoScale(B.Pose.TransformPositionNoScale(C.LocalAuthorityReference)).X;
		const bool Crossed = From > 1.e-6 && To <= 0;
		if (Crossed && (!C.PermitTransfer || !C.ExitCorridorCertified || !C.Exit.IsValid()
			|| !C.Exit.GetScale3D().Equals(FVector::OneVector, 1.e-6)))
		{ return Reject(EBoundaryReason::TransferNotCertified); }
		return { EBoundaryReason::Eligible, true, Crossed };
	}
	bool CanConsumeReceipt(const FBoundaryReceipt& R, const FTravellerHandle& Body, uint64 Pair,
		uint64 Revision, uint64 Epoch, const FPhysicsStepKey& Last)
	{
		return Body.Epoch && Body.Id && Body.Generation && Pair && Revision && Epoch
			&& R.Body == Body && R.PairGeneration == Pair && R.CommandRevision == Revision
			&& R.Step.SolverEpoch == Epoch && R.Step.SolverFrame >= 0 && R.Step.EvolutionSerial
			&& FMath::IsFinite(R.Step.DeltaSeconds) && R.Step.DeltaSeconds > 0
			&& (Last.SolverEpoch == 0 || (Last.SolverEpoch == Epoch
				&& R.Step.EvolutionSerial > Last.EvolutionSerial && R.Step.SolverFrame > Last.SolverFrame));
	}
}
