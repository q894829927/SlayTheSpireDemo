#pragma once

#include "InteriorPortalPhysicsBinding.h"
#include "InteriorPortalPassageCoordinator.h"

namespace Chaos { class FPBDRigidsSolver; class FSingleParticlePhysicsProxy; }

namespace InteriorPortalPhysics
{
	class FChaosTransferAdapter;
	class FChaosStaticClearance;

	/** Value-only committed facts remain available after all native writes are retired. */
	struct FPassageSessionHandoff
	{
		FTravellerHandle Body;
		uint64 SolverEpoch = 0, BindingEpoch = 0;
		TArray<FTransferFact> PendingFacts;
	};

	/** One PT owner for passage metadata, transfer writes and clearance authority.
	 * The outer sim callback must retire this session before removing any bound proxy.
	 * GT owns the prepared request; this object never discovers UObjects.
	 */
	class SLAYTHESPIREDEMO_API FChaosPassageSession final
	{
	public:
		FChaosPassageSession(Chaos::FPBDRigidsSolver* Solver,
			Chaos::FSingleParticlePhysicsProxy* Body,
			Chaos::FSingleParticlePhysicsProxy* Support0,
			Chaos::FSingleParticlePhysicsProxy* Support1,
			const FBoundaryCommand& Command, uint64 SolverEpoch, uint64 BindingEpoch);
		~FChaosPassageSession();
		FChaosPassageSession(const FChaosPassageSession&) = delete;
		FChaosPassageSession& operator=(const FChaosPassageSession&) = delete;
		bool AttachClearance_Internal(const FPreparedPhysicsBinding& Prepared);
		FPassageSessionHandoff Retire_Internal();
		bool ReferencesProxy(const Chaos::FSingleParticlePhysicsProxy* Proxy) const;
		bool IsRetired() const { return Retired; }
		FPassageCoordinator* Passage() const { return Coordinator.Get(); }
		FChaosTransferAdapter* Transfer() const { return Adapter.Get(); }
		FChaosStaticClearance* Clearance() const { return ClearanceVerifier.Get(); }
	private:
		Chaos::FPBDRigidsSolver* Solver = nullptr;
		Chaos::FSingleParticlePhysicsProxy* BodyProxy = nullptr;
		Chaos::FSingleParticlePhysicsProxy* SupportProxies[2] = {nullptr,nullptr};
		FTravellerHandle Body;
		uint64 Pair = 0, Epoch = 0, Binding = 0;
		FTransform Frames[2];
		FVector Scale = FVector::OneVector;
		TUniquePtr<FPassageCoordinator> Coordinator;
		TUniquePtr<FChaosTransferAdapter> Adapter;
		TUniquePtr<FChaosStaticClearance> ClearanceVerifier;
		bool Retired = false;
	};
}
