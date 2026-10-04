#pragma once

#include "InteriorPortalPhysicsBoundary.h"

namespace InteriorPortalPhysics
{
	struct FHoldProfile
	{
		double LinearStiffness = 300, LinearDamping = 160, MaxForce = 1200;
		double AngularStiffness = 600, AngularDamping = 200, MaxTorque = 1200;
		double MaxAnchorErrorCm = 300, SkinCm = .5, MaxStepSeconds = 1. / 60.;
	};
	enum class EHoldRoute : uint8 { Direct, SinglePair, Unsupported };
	struct FHoldRoute
	{
		EHoldRoute Kind = EHoldRoute::Direct;
		uint64 Revision = 0, PairGeneration = 0;
		FTransform HolderSide = FTransform::Identity, BodySide = FTransform::Identity;
	};
	/** Inward normal: legal space satisfies dot(N, worldPoint - Point) >= 0. */
	struct FHoldHalfSpace
	{
		uint64 Id = 0;
		FVector Normal = FVector::ForwardVector, Point = FVector::ZeroVector;
	};
	/** A certified convex free region, not an arbitrary list of unverified world hits. */
	struct FHoldRegion
	{
		uint64 Revision = 0;
		bool CertifiedStaticCoverage = false;
		TArray<FHoldHalfSpace> Planes; // Strictly increasing stable IDs.
	};
	struct FHoldCommand
	{
		FTravellerSnapshot Traveller;
		FHoldRoute Route;
		FHoldRegion Region;
		FHoldProfile Profile;
		FTransform DesiredHolderPose = FTransform::Identity;
		FVector LocalGrabAnchor = FVector::ZeroVector; // Baked collision-space cm.
	};
	enum class EHoldReason : uint8
	{
		Feasible, Projected, InvalidInput, StaleIdentity, UnsupportedRoute,
		UncertifiedRegion, InitialOverlap, ExcessiveError, Infeasible
	};
	struct FHoldTarget
	{
		EHoldReason Reason = EHoldReason::InvalidInput;
		FTransform Pose = FTransform::Identity;
		bool RotationProjected = false;
		bool Usable() const { return Reason == EHoldReason::Feasible || Reason == EHoldReason::Projected; }
	};
	struct FHoldPhysicalState
	{
		FBoundaryState Body;
		double MassKg = 0;
		FVector LocalCOM = FVector::ZeroVector, LocalInertia = FVector::ZeroVector;
		FQuat RotationOfMass = FQuat::Identity;
	};
	struct FHoldDrive
	{
		FHoldTarget Target;
		FVector Force = FVector::ZeroVector, Torque = FVector::ZeroVector;
	};
	/** Pure advisory query; never changes route, contact permission or body pose. */
	SLAYTHESPIREDEMO_API FHoldTarget SolveHoldTarget(const FHoldCommand& Command,
		const FBoundaryState& Body, const FTravellerHandle& CurrentBody, uint64 CurrentPair,
		uint64 CurrentRouteRevision, uint64 CurrentRegionRevision);
	/** Physical wrench only. Contact/transfer authority is deliberately absent. */
	SLAYTHESPIREDEMO_API FHoldDrive EvaluateHoldDrive(const FHoldCommand& Command,
		const FHoldPhysicalState& State, const FPhysicsStepKey& Step,
		const FTravellerHandle& CurrentBody, uint64 CurrentPair,
		uint64 CurrentRouteRevision, uint64 CurrentRegionRevision);
}
