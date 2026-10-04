#pragma once

#include "InteriorPortalPhysicsBoundary.h"

namespace Chaos { class FPBDRigidsSolver; class FSingleParticlePhysicsProxy; class FCollisionContactModifier; }

namespace InteriorPortalPhysics
{
	enum class EClearanceStage : uint8 { PreIntegrate, PostIntegrate, PostSolve };
	enum class EStaticClearanceReason : uint8
	{
		Clear, Retired, InvalidInterval, BindingChanged, UnsupportedMotion,
		UnsupportedScene, OutsideAperture, SourceBlocked, DestinationBlocked, UncertifiedDynamicInteraction
	};
	enum class ENativeBindingIssue : uint8
	{
		None, Command, BodyState, BodyCollision, BodyBounds, BodyCOM,
		SupportState, SupportCollision, SupportBounds, SupportPose, GeometryChanged, SupportSpan
	};
	/** Diagnostic categories for fail-closed scene rejection. Several may apply in one scan. */
	enum class ENativeSceneIssue : uint8
	{
		TravellerConstraint, ParticleBudget, NativeBounds, KinematicMotion,
		UnboundedActive, ActiveCCD, ActiveConstraint, ActiveContact, ActiveInvalidState,
		ActivePairInteraction, ActiveStaticReach,
		DormantBody, OtherParticleState, LeaseBudget, CollisionSettings
	};
	constexpr uint32 NativeSceneIssueBit(ENativeSceneIssue Issue)
	{ return 1u << static_cast<uint8>(Issue); }
	class FChaosStaticClearance;
	/** Issued by a native solver scan, never constructible from GT query results. */
	class FStaticClearanceProof
	{
	public:
		EStaticClearanceReason Reason() const { return Result; }
		ENativeBindingIssue BindingIssue() const { return Issue; }
		int32 BindingComponent() const { return Component; } // -1 body/command, 0 entry support, 1 exit support.
		uint32 SceneIssueMask() const { return SceneIssues; }
	private:
		friend class FChaosStaticClearance;
		EStaticClearanceReason Result = EStaticClearanceReason::InvalidInterval;
		ENativeBindingIssue Issue = ENativeBindingIssue::None;
		int32 Component = -1;
		uint32 SceneIssues = 0;
		uint64 Binding = 0, Sequence = 0;
		FBoundaryCommand Command;
		FBoundaryState Start, End;
		FPhysicsStepKey Step;
		EClearanceStage Stage = EClearanceStage::PreIntegrate;
	};
	/** PT-only, fixed-orientation, speed-capped traveller with static supports and certified kinematics.
	 * Native sleepers and closed sleeping contact islands are leased per interval.
	 * Contact-free, speed-capped remote dynamics have bounded interval leases;
	 * contacting/CCD dynamics and persistent joints still reject.
	 * Retire before proxy removal.
	 * A bounded native scan, not yet an active general-world production provider. */
	class SLAYTHESPIREDEMO_API FChaosStaticClearance
	{
	public:
		FChaosStaticClearance(Chaos::FPBDRigidsSolver* Solver, Chaos::FSingleParticlePhysicsProxy* Body,
			Chaos::FSingleParticlePhysicsProxy* Support0, Chaos::FSingleParticlePhysicsProxy* Support1,
			const FBoundaryCommand& BindingCommand, uint64 SolverEpoch, uint64 BindingEpoch,
			const FTransform& SupportPose0, const FTransform& SupportPose1,
			const FGeometry& SupportGeometry0, const FGeometry& SupportGeometry1);
		FChaosStaticClearance(const FChaosStaticClearance&) = delete;
		FChaosStaticClearance& operator=(const FChaosStaticClearance&) = delete;
		FStaticClearanceProof Certify_Internal(const FBoundaryCommand& Command,
			const FBoundaryState& Start, const FBoundaryState& End, const FPhysicsStepKey& Step,
			EClearanceStage Stage, int32 EntryEndpoint);
		bool Consume_Internal(const FStaticClearanceProof& Proof, const FBoundaryCommand& Command,
			const FBoundaryState& Start, const FBoundaryState& End, const FPhysicsStepKey& Step, EClearanceStage Stage);
		/** Call before selectively disabling support contacts; any leased-body contact revokes this interval. */
		bool ContactsRemainIndependent_Internal(Chaos::FCollisionContactModifier& Modifier) const;
		void Retire_Internal();
	private:
		Chaos::FPBDRigidsSolver* Solver = nullptr;
		Chaos::FSingleParticlePhysicsProxy* Body = nullptr;
		Chaos::FSingleParticlePhysicsProxy* Supports[2] = {nullptr,nullptr};
		FBoundaryCommand Bound, IntervalCommand;
		FTransform SupportPoses[2];
		FBox BoundSupportBounds[2];
		const void* NativeGeometry[3] = {nullptr,nullptr,nullptr};
		uint32 GeometryHashes[3] = {0,0,0};
		bool GeometryBound = false;
		struct FKinematicEnvelope
		{
			FBox Sweep, Local;
			const void* Geometry = nullptr;
			uint32 GeometryHash = 0;
		};
		// Native unique indices are keys only, never retained particle/proxy pointers.
		// Cleared for every physical interval; later hooks verify against its sweep.
		TMap<int32, FKinematicEnvelope> KinematicEnvelopes;
		struct FActiveEnvelope
		{
			FBox Reach, Local;
			const void* Geometry = nullptr;
			uint32 GeometryHash = 0;
			double MaxLinearSpeedSq = 0;
		};
		TMap<int32, FActiveEnvelope> ActiveEnvelopes;
		struct FStaticContactParticipant
		{
			int32 Id = INDEX_NONE;
			FTransform Pose;
			const void* Geometry = nullptr;
			uint32 GeometryHash = 0;
			bool operator==(const FStaticContactParticipant& B) const
			{ return Id == B.Id && Pose.Equals(B.Pose,1.e-6) && Geometry == B.Geometry && GeometryHash == B.GeometryHash; }
		};
		struct FDormantLease
		{
			FBox Local;
			FTransform Pose;
			const void* Geometry = nullptr;
			uint32 GeometryHash = 0;
			TArray<int32> IslandMembers, ContactPartners;
			TArray<FStaticContactParticipant> StaticContacts;
			int32 IslandConstraints = 0;
		};
		TMap<int32, FDormantLease> DormantLeases;
		double DormantCollisionSettings[5] = {0,0,0,0,0};
		FBoundaryState IntervalStart;
		FPhysicsStepKey IntervalStep;
		double TravellerMaxLinearSpeedSq = 0;
		uint64 Epoch = 0, Binding = 0, Sequence = 0;
		EClearanceStage LastStage = EClearanceStage::PreIntegrate;
		bool Consumed = true, Retired = false;
	};
}
