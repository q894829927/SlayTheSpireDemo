#pragma once

#include "InteriorPortalPassageBindingLifecycle.h"
#include "InteriorPortalChaosStaticClearance.h"

class FPhysScene_Chaos;
namespace Chaos { class FPBDRigidsSolver; class FSingleParticlePhysicsProxy; }

namespace InteriorPortalPhysics
{
	class FPortalBindingObserveCallback;

	/** GT owner of one traveller's real Chaos callback. This stage observes only:
	 * no force, collision permission, transfer, speed-cap or body setter is called.
	 * The old gameplay writer remains the only active motion authority.
	 */
	class SLAYTHESPIREDEMO_API FPortalPhysicsBindingBridge final
	{
	public:
		FPortalPhysicsBindingBridge() = default;
		~FPortalPhysicsBindingBridge();
		FPortalPhysicsBindingBridge(const FPortalPhysicsBindingBridge&) = delete;
		FPortalPhysicsBindingBridge& operator=(const FPortalPhysicsBindingBridge&) = delete;
		void Update_GameThread(const FPhysicsBindingRequest* Request);
		void Cancel_GameThread();
		void Shutdown_GameThread();
		bool HasLiveBinding() const { return Callback != nullptr; }
		uint64 ObservedPhysicsSteps() const { return ObservedSteps; }
		uint64 ObservedClearanceSteps() const { return ClearanceSteps; }
		uint64 BindingMismatchSteps() const { return MismatchSteps; }
		EStaticClearanceReason LastClearanceReason() const { return ClearanceReason; }
		uint32 LastSceneIssueMask() const { return SceneIssueMask; }
		ENativeBindingIssue LastBindingIssue() const { return BindingIssue; }
		int32 LastBindingComponent() const { return BindingComponent; }
		FPhysicsStepKey LastClearanceStep() const { return ClearanceStep; }
		uint64 BoundPairGeneration() const { return Callback ? Domain.PairGeneration : 0; }
	private:
		bool Start_GameThread(const FPhysicsBindingRequest& Request, const FPreparedPhysicsBinding& Prepared);
		void Poll_GameThread();
		FPassageBindingLifecycle Lifecycle;
		FPassageBindingDomain Domain;
		FPreparedPhysicsBinding Prepared;
		FPhysScene_Chaos* Scene = nullptr;
		FPortalBindingObserveCallback* Callback = nullptr;
		Chaos::FSingleParticlePhysicsProxy* Proxies[3] = {nullptr,nullptr,nullptr};
		uint64 ObservedSteps = 0;
		uint64 ClearanceSteps = 0;
		uint64 MismatchSteps = 0;
		EStaticClearanceReason ClearanceReason = EStaticClearanceReason::InvalidInterval;
		uint32 SceneIssueMask = 0;
		ENativeBindingIssue BindingIssue = ENativeBindingIssue::None;
		int32 BindingComponent = -1;
		FPhysicsStepKey ClearanceStep;
		bool bCancelling = false;
	};
}
