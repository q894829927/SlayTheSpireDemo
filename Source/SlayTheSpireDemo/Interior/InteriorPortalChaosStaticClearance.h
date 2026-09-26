#pragma once

#include "InteriorPortalPhysicsBoundary.h"

namespace Chaos { class FPBDRigidsSolver; class FSingleParticlePhysicsProxy; }

namespace InteriorPortalPhysics
{
	enum class EClearanceStage : uint8 { PreIntegrate, PostIntegrate, PostSolve };
	enum class EStaticClearanceReason : uint8
	{
		Clear, Retired, InvalidInterval, BindingChanged, UnsupportedMotion,
		UnsupportedScene, OutsideAperture, SourceBlocked, DestinationBlocked
	};
	class FChaosStaticClearance;
	/** Issued by a native solver scan, never constructible from GT query results. */
	class FStaticClearanceProof
	{
	public:
		EStaticClearanceReason Reason() const { return Result; }
	private:
		friend class FChaosStaticClearance;
		EStaticClearanceReason Result = EStaticClearanceReason::InvalidInterval;
		uint64 Binding = 0, Sequence = 0;
		FBoundaryCommand Command;
		FBoundaryState Start, End;
		FPhysicsStepKey Step;
		EClearanceStage Stage = EClearanceStage::PreIntegrate;
	};
	/** PT-only, fixed-normal traveller with static supports and certified kinematics.
	 * Collidable dynamics still reject; simulation filters exclude unrelated pairs.
	 * Retire before proxy removal.
	 * An O(N) correctness primitive, not yet an active general-world production provider. */
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
		FBoundaryState IntervalStart;
		FPhysicsStepKey IntervalStep;
		uint64 Epoch = 0, Binding = 0, Sequence = 0;
		EClearanceStage LastStage = EClearanceStage::PreIntegrate;
		bool Consumed = true, Retired = false;
	};
}
