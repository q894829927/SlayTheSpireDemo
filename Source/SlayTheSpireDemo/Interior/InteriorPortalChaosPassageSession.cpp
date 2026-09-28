#include "InteriorPortalChaosPassageSession.h"

#include "InteriorPortalChaosStaticClearance.h"
#include "InteriorPortalChaosTransferAdapter.h"

namespace InteriorPortalPhysics
{
	FChaosPassageSession::FChaosPassageSession(Chaos::FPBDRigidsSolver* InSolver,
		Chaos::FSingleParticlePhysicsProxy* InBody,
		Chaos::FSingleParticlePhysicsProxy* Support0,
		Chaos::FSingleParticlePhysicsProxy* Support1,
		const FBoundaryCommand& C, uint64 SolverEpoch, uint64 BindingEpoch)
		: Solver(InSolver), BodyProxy(InBody), SupportProxies{Support0,Support1},
		Body(C.Traveller.Handle), Pair(C.PairGeneration), Epoch(SolverEpoch), Binding(BindingEpoch),
		Frames{C.Entry,C.Exit}, Scale(C.Traveller.Geometry.BakedScale)
	{
		Coordinator = MakeUnique<FPassageCoordinator>(Body,Pair,Epoch,Binding,Frames[0],Frames[1]);
		Adapter = MakeUnique<FChaosTransferAdapter>(BodyProxy,Body,Pair,Epoch,Binding,Scale);
		if (!Solver || !BodyProxy || !SupportProxies[0] || !SupportProxies[1]
			|| BodyProxy == SupportProxies[0] || BodyProxy == SupportProxies[1]
			|| SupportProxies[0] == SupportProxies[1] || !Epoch || !Binding || !Pair
			|| !Body.Epoch || !Body.Id || !Body.Generation)
		{ Retire_Internal(); }
	}
	FChaosPassageSession::~FChaosPassageSession() = default;

	bool FChaosPassageSession::AttachClearance_Internal(const FPreparedPhysicsBinding& Prepared)
	{
		const auto& C = Prepared.Command;
		if (Retired || ClearanceVerifier || !(C.Traveller.Handle == Body) || C.PairGeneration != Pair
			|| C.Traveller.Geometry.BakedScale != Scale
			|| !C.Entry.Equals(Frames[0],0) || !C.Exit.Equals(Frames[1],0)
			|| C.IsolatedStaticScope || C.ExitCorridorCertified)
		{ Retire_Internal(); return false; }
		ClearanceVerifier = MakeUnique<FChaosStaticClearance>(Solver,BodyProxy,SupportProxies[0],SupportProxies[1],
			C,Epoch,Binding,Prepared.SupportPose[0],Prepared.SupportPose[1],
			Prepared.SupportGeometry[0],Prepared.SupportGeometry[1]);
		return true;
	}

	FPassageSessionHandoff FChaosPassageSession::Retire_Internal()
	{
		if (!Retired)
		{
			Retired = true;
			Coordinator->Retire();
			Adapter->Retire_Internal();
			if (ClearanceVerifier) { ClearanceVerifier->Retire_Internal(); }
		}
		return {Body,Epoch,Binding,Coordinator->PendingFacts()};
	}

	bool FChaosPassageSession::ReferencesProxy(const Chaos::FSingleParticlePhysicsProxy* Proxy) const
	{ return Proxy && (Proxy == BodyProxy || Proxy == SupportProxies[0] || Proxy == SupportProxies[1]); }
}
