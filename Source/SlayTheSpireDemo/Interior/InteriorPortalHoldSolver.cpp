#include "InteriorPortalHoldSolver.h"
#include "InteriorPortalMath.h"

namespace InteriorPortalPhysics
{
	namespace
	{
		bool ValidProfile(const FHoldProfile& P)
		{
			for (double V : {P.LinearStiffness, P.LinearDamping, P.MaxForce, P.AngularStiffness,
				P.AngularDamping, P.MaxTorque, P.MaxAnchorErrorCm, P.SkinCm, P.MaxStepSeconds})
			{ if (!FMath::IsFinite(V) || V < 0) { return false; } }
			return P.MaxAnchorErrorCm > 0 && P.MaxStepSeconds > 0;
		}
		bool RigidFrame(const FTransform& T)
		{ return T.IsValid() && T.GetScale3D().Equals(FVector::OneVector, 1.e-6); }
		// Exact directional support for the same baked primitives used by passage queries.
		bool Support(const FGeometry& G, const FQuat& Q, const FVector& N, double& Min, double& Radius)
		{
			Min = TNumericLimits<double>::Max(); Radius = 0;
			for (const FPrimitive& P : G.Primitives)
			{
				const auto Include = [&](const FVector& V, double R)
				{
					Min = FMath::Min(Min, FVector::DotProduct(N, Q.RotateVector(V)) - R);
					Radius = FMath::Max(Radius, V.Size() + R);
				};
				if (P.Shape == EShape::Sphere) { Include(P.Center, P.Radius); }
				else if (P.Shape == EShape::Capsule)
				{ Include(P.Center - P.HalfSegment, P.Radius); Include(P.Center + P.HalfSegment, P.Radius); }
				else { for (const FVector& V : P.Vertices) { Include(V, 0); } }
			}
			return FMath::IsFinite(Min) && FMath::IsFinite(Radius);
		}
	}
	FHoldTarget SolveHoldTarget(const FHoldCommand& C, const FBoundaryState& Body,
		const FTravellerHandle& CurrentBody, uint64 CurrentPair, uint64 RouteRevision, uint64 RegionRevision)
	{
		FHoldTarget R;
		const auto Reject = [&](EHoldReason Reason) { R.Reason = Reason; return R; };
		if (!CurrentBody.Epoch || !CurrentBody.Id || !CurrentBody.Generation || C.Traveller.Handle != CurrentBody
			|| !RouteRevision || C.Route.Revision != RouteRevision || !RegionRevision || C.Region.Revision != RegionRevision)
		{ return Reject(EHoldReason::StaleIdentity); }
		if (!ValidProfile(C.Profile) || !C.DesiredHolderPose.IsValid() || C.LocalGrabAnchor.ContainsNaN()
			|| !EvaluatePose(C.Traveller.Geometry, Body.Pose, FTransform::Identity, 1.e100, 1.e100).Fits())
		{ return Reject(EHoldReason::InvalidInput); }
		if (!C.Region.CertifiedStaticCoverage) { return Reject(EHoldReason::UncertifiedRegion); }
		if (C.Route.Kind == EHoldRoute::Unsupported) { return Reject(EHoldReason::UnsupportedRoute); }
		R.Pose = C.DesiredHolderPose;
		R.Pose.SetScale3D(C.Traveller.Geometry.BakedScale);
		if (C.Route.Kind == EHoldRoute::SinglePair)
		{
			if (!CurrentPair || C.Route.PairGeneration != CurrentPair)
			{ return Reject(EHoldReason::StaleIdentity); }
			if (!RigidFrame(C.Route.HolderSide) || !RigidFrame(C.Route.BodySide))
			{ return Reject(EHoldReason::InvalidInput); }
			R.Pose.SetLocation(InteriorPortalMath::Position(R.Pose.GetLocation(), C.Route.HolderSide, C.Route.BodySide));
			R.Pose.SetRotation(InteriorPortalMath::Rotation(C.Route.HolderSide, C.Route.BodySide) * R.Pose.GetRotation());
		}
		else if (C.Route.Kind != EHoldRoute::Direct) { return Reject(EHoldReason::UnsupportedRoute); }
		if (FVector::Dist(Body.Pose.TransformPositionNoScale(C.LocalGrabAnchor),
			R.Pose.TransformPositionNoScale(C.LocalGrabAnchor)) > C.Profile.MaxAnchorErrorCm)
		{ return Reject(EHoldReason::ExcessiveError); }
		TArray<double> Bounds;
		uint64 LastId = 0;
		double CollisionRadius = 0;
		for (const FHoldHalfSpace& Plane : C.Region.Planes)
		{
			if (Plane.Id <= LastId || Plane.Normal.ContainsNaN() || Plane.Point.ContainsNaN()
				|| !FMath::IsNearlyEqual(Plane.Normal.SizeSquared(), 1., 1.e-6))
			{ return Reject(EHoldReason::InvalidInput); }
			LastId = Plane.Id;
			double Min;
			if (!Support(C.Traveller.Geometry, Body.Pose.GetRotation(), Plane.Normal, Min, CollisionRadius))
			{ return Reject(EHoldReason::InvalidInput); }
			const double Bound = FVector::DotProduct(Plane.Normal, Plane.Point) + C.Profile.SkinCm - Min;
			if (FVector::DotProduct(Plane.Normal, Body.Pose.GetLocation()) < Bound - 1.e-6)
			{ return Reject(EHoldReason::InitialOverlap); }
			Bounds.Add(Bound);
		}
		const double Angle = Body.Pose.GetRotation().AngularDistance(R.Pose.GetRotation());
		const double Envelope = 2 * CollisionRadius * FMath::Sin(Angle * .5);
		for (int32 I = 0; I < Bounds.Num(); ++I)
		{
			if (FVector::DotProduct(C.Region.Planes[I].Normal, Body.Pose.GetLocation()) < Bounds[I] + Envelope - 1.e-6)
			{ R.RotationProjected = true; R.Pose.SetRotation(Body.Pose.GetRotation()); break; }
		}
		if (!R.RotationProjected) { for (double& B : Bounds) { B += Envelope; } }
		const FVector RawPosition = R.Pose.GetLocation();
		FVector Projected = RawPosition;
		for (int32 Pass = 0; Pass < 32; ++Pass)
		{
			bool Changed = false;
			for (int32 I = 0; I < Bounds.Num(); ++I)
			{
				const FVector& N = C.Region.Planes[I].Normal;
				const double Gap = Bounds[I] - FVector::DotProduct(N, Projected);
				if (Gap > 1.e-8) { Projected += N * Gap; Changed = true; }
			}
			if (!Changed) { break; }
		}
		for (int32 I = 0; I < Bounds.Num(); ++I)
		{
			if (!FMath::IsFinite(FVector::DotProduct(C.Region.Planes[I].Normal, Projected))
				|| FVector::DotProduct(C.Region.Planes[I].Normal, Projected) < Bounds[I] - 1.e-6)
			{ return Reject(EHoldReason::Infeasible); }
		}
		R.Pose.SetLocation(Projected);
		R.Reason = R.RotationProjected || !Projected.Equals(RawPosition, 1.e-6) ? EHoldReason::Projected : EHoldReason::Feasible;
		return R;
	}
	FHoldDrive EvaluateHoldDrive(const FHoldCommand& C, const FHoldPhysicalState& S,
		const FPhysicsStepKey& Step, const FTravellerHandle& CurrentBody, uint64 Pair,
		uint64 RouteRevision, uint64 RegionRevision)
	{
		FHoldDrive R;
		R.Target = SolveHoldTarget(C, S.Body, CurrentBody, Pair, RouteRevision, RegionRevision);
		if (!R.Target.Usable()) { return R; }
		if (!Step.SolverEpoch || Step.SolverFrame < 0 || !Step.EvolutionSerial
			|| !FMath::IsFinite(Step.DeltaSeconds) || Step.DeltaSeconds <= 0
			|| Step.DeltaSeconds > C.Profile.MaxStepSeconds + 1.e-9
			|| !FMath::IsFinite(S.MassKg) || S.MassKg <= 0 || S.LocalCOM.ContainsNaN()
			|| S.LocalInertia.ContainsNaN() || S.LocalInertia.GetMin() <= 0
			|| S.RotationOfMass.ContainsNaN() || !S.RotationOfMass.IsNormalized()
			|| S.Body.LinearVelocity.ContainsNaN() || S.Body.AngularVelocity.ContainsNaN())
		{ R.Target.Reason = EHoldReason::InvalidInput; return R; }
		const double Dt = Step.DeltaSeconds;
		const auto& P = C.Profile;
		const FVector Anchor = S.Body.Pose.TransformPositionNoScale(C.LocalGrabAnchor);
		const FVector Arm = Anchor - S.Body.Pose.TransformPositionNoScale(S.LocalCOM);
		const FVector AnchorV = S.Body.LinearVelocity + FVector::CrossProduct(S.Body.AngularVelocity, Arm);
		const FVector Error = R.Target.Pose.TransformPositionNoScale(C.LocalGrabAnchor) - Anchor;
		const double LinearDenominator = 1 + (P.LinearDamping * Dt + P.LinearStiffness * Dt * Dt) / S.MassKg;
		R.Force = ((P.LinearStiffness * Error - (P.LinearDamping + P.LinearStiffness * Dt) * AnchorV)
			/ LinearDenominator).GetClampedToMaxSize(P.MaxForce);
		FQuat Difference = (R.Target.Pose.GetRotation() * S.Body.Pose.GetRotation().Inverse()).GetNormalized();
		if (Difference.W < 0) { Difference = Difference * -1.; }
		FVector Axis; double Angle;
		Difference.ToAxisAndAngle(Axis, Angle);
		const FQuat MassRotation = S.Body.Pose.GetRotation() * S.RotationOfMass;
		const FVector AngularError = MassRotation.UnrotateVector(Axis * Angle);
		const FVector AngularV = MassRotation.UnrotateVector(S.Body.AngularVelocity);
		FVector LocalTorque;
		for (int32 I = 0; I < 3; ++I)
		{
			LocalTorque[I] = (P.AngularStiffness * AngularError[I] - (P.AngularDamping + P.AngularStiffness * Dt) * AngularV[I])
				/ (1 + (P.AngularDamping * Dt + P.AngularStiffness * Dt * Dt) / S.LocalInertia[I]);
		}
		R.Torque = (MassRotation.RotateVector(LocalTorque) + FVector::CrossProduct(Arm, R.Force)).GetClampedToMaxSize(P.MaxTorque);
		if (R.Force.ContainsNaN() || R.Torque.ContainsNaN())
		{ R = FHoldDrive(); R.Target.Reason = EHoldReason::InvalidInput; }
		return R;
	}
}
