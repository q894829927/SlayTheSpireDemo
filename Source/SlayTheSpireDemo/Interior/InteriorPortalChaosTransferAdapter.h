#pragma once

#include "InteriorPortalPassageCoordinator.h"

namespace Chaos { class FSingleParticlePhysicsProxy; }

namespace InteriorPortalPhysics
{
	/** PT-only, non-owning binding. Owner must retire before any bound proxy is removed.
	 * Arm only at PostSolve of the supplied interval. No UObject or game-thread pose writes. */
	class SLAYTHESPIREDEMO_API FChaosTransferAdapter final : public IPortalTransferAdapter
	{
	public:
		FChaosTransferAdapter(Chaos::FSingleParticlePhysicsProxy* Proxy, FTravellerHandle Body,
			uint64 Pair, uint64 SolverEpoch, uint64 BindingEpoch, const FVector& BakedScale);
		FChaosTransferAdapter(const FChaosTransferAdapter&) = delete;
		FChaosTransferAdapter& operator=(const FChaosTransferAdapter&) = delete;
		void BeginSolvedStep_Internal(const FPhysicsStepKey& Step);
		void Retire_Internal();
		virtual bool Commit(const FTransferFact& Fact) override;
	private:
		Chaos::FSingleParticlePhysicsProxy* Proxy = nullptr;
		FTravellerHandle Body;
		uint64 Pair = 0, Epoch = 0, Binding = 0, LastRevision = 0, LastCommittedSerial = 0;
		FVector Scale = FVector::OneVector;
		FPhysicsStepKey SolvedStep;
		bool Armed = false;
	};
}
