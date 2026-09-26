#pragma once
#include "InteriorPortalTravellerRegistry.h"

namespace InteriorPortalPhysics
{
	/** Values only: no component, actor, world or solver pointers cross this boundary. */
	struct FBoundaryCommand
	{
		FTravellerSnapshot Traveller;
		uint64 PairGeneration = 0;
		uint64 Revision = 0;
		uint32 MaxReuseSteps = 1;
		bool PermitSupportBypass = false;
		bool IsolatedStaticScope = false;
		bool PermitTransfer = false;
		bool ExitCorridorCertified = false;
		FTransform Entry = FTransform::Identity;
		FTransform Exit = FTransform::Identity;
		FVector LocalAuthorityReference = FVector::ZeroVector; // Baked cm; production adapter supplies actual COM.
		double HalfWidth = 65;
		double HalfHeight = 115;
		double MarginCm = .5;
		double SupportHalfThicknessCm = 0;
		double MaxStepSeconds = 1. / 60.;
		double MaxTranslationCm = 2;
		FVector ForceIntent = FVector::ZeroVector;
		double MaxForce = 0; // kg * cm / s^2; bounds this adapter's force only.
	};
	struct FPhysicsStepKey
	{
		uint64 SolverEpoch = 0;
		int32 SolverFrame = -1;
		uint64 EvolutionSerial = 0;
		double DeltaSeconds = 0;
	};
	struct FBoundaryState
	{
		FTransform Pose = FTransform::Identity;
		FVector LinearVelocity = FVector::ZeroVector;
		FVector AngularVelocity = FVector::ZeroVector;
	};
	enum class EBoundaryReason : uint8
	{
		Eligible, Disabled, InvalidCommand, ExpiredInput, UnsupportedStep,
		UnsupportedRotation, UnsupportedMotion, OtherContact, OutsideAperture,
		TransferNotCertified
	};
	struct FBoundaryDecision
	{
		EBoundaryReason Reason = EBoundaryReason::InvalidCommand;
		bool BypassSupport = false;
		bool TransferAfterSolve = false;
	};
	struct FBoundaryReceipt
	{
		FTravellerHandle Body;
		uint64 PairGeneration = 0;
		uint64 CommandRevision = 0;
		FPhysicsStepKey Step;
	};
	/** Restricted spike profile: fixed rotation, isolated static corridor, bounded interval. */
	SLAYTHESPIREDEMO_API FBoundaryDecision EvaluateBoundary(const FBoundaryCommand& Command,
		const FBoundaryState& Start, const FBoundaryState& Integrated, const FPhysicsStepKey& Step,
		uint32 ReuseOrdinal, bool HasOtherContact);
	SLAYTHESPIREDEMO_API FVector BoundedForce(const FBoundaryCommand& Command);
	/** Inelastic reaction against a static gate: remove forbidden inward motion only. */
	SLAYTHESPIREDEMO_API FVector StaticGateProjection(const FVector& Vector, const FTransform& Frame);
	/** Receipt validation does not commit pose, contacts or a transfer. */
	SLAYTHESPIREDEMO_API bool CanConsumeReceipt(const FBoundaryReceipt& Receipt,
		const FTravellerHandle& CurrentBody, uint64 CurrentPair, uint64 CurrentCommandRevision,
		uint64 CurrentSolverEpoch, const FPhysicsStepKey& LastConsumed);
}
