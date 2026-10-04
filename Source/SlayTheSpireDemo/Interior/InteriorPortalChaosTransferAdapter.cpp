#include "InteriorPortalChaosTransferAdapter.h"
#include "Chaos/ParticleHandle.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

namespace InteriorPortalPhysics
{
	FChaosTransferAdapter::FChaosTransferAdapter(Chaos::FSingleParticlePhysicsProxy* InProxy, FTravellerHandle InBody,
		uint64 InPair, uint64 InEpoch, uint64 InBinding, const FVector& BakedScale)
		: Proxy(InProxy), Body(InBody), Pair(InPair), Epoch(InEpoch), Binding(InBinding), Scale(BakedScale)
	{
		if (!Body.Epoch || !Body.Id || !Body.Generation || !Pair || !Epoch || !Binding
			|| Scale.ContainsNaN() || Scale.GetMin() <= UE_SMALL_NUMBER) { Retire_Internal(); }
	}
	void FChaosTransferAdapter::BeginSolvedStep_Internal(const FPhysicsStepKey& Step)
	{
		SolvedStep = Step;
		Armed = Proxy && Step.SolverEpoch == Epoch && Step.SolverFrame >= 0
			&& Step.EvolutionSerial > LastCommittedSerial && FMath::IsFinite(Step.DeltaSeconds) && Step.DeltaSeconds > 0;
	}
	void FChaosTransferAdapter::Retire_Internal() { Armed = false; Proxy = nullptr; }
	bool FChaosTransferAdapter::Commit(const FTransferFact& F)
	{
		if (!Armed || !(F.Body == Body) || F.PairGeneration != Pair || F.SolverEpoch != Epoch
			|| F.BindingEpoch != Binding || F.Revision != LastRevision + 1 || F.Step.SolverEpoch != SolvedStep.SolverEpoch
			|| F.Step.SolverFrame != SolvedStep.SolverFrame || F.Step.EvolutionSerial != SolvedStep.EvolutionSerial
			|| F.Step.DeltaSeconds != SolvedStep.DeltaSeconds
			|| F.FromEndpoint < 0 || F.FromEndpoint > 1 || F.ToEndpoint != 1 - F.FromEndpoint)
		{ return false; }
		const auto Valid = [&](const FBodyCommitState& S)
		{
			return S.Motion.Pose.IsValid() && S.Motion.Pose.GetScale3D().Equals(Scale, 1.e-6)
				&& !S.Motion.LinearVelocity.ContainsNaN() && !S.Motion.AngularVelocity.ContainsNaN()
				&& FMath::IsFinite(S.MassKg) && S.MassKg > 0
				&& !S.LocalInertia.ContainsNaN() && S.LocalInertia.GetMin() > 0 && !S.LocalCOM.ContainsNaN()
				&& !S.RotationOfMass.ContainsNaN() && S.RotationOfMass.IsNormalized();
		};
		if (!Valid(F.Before) || !Valid(F.After) || F.Before.MassKg != F.After.MassKg
			|| F.Before.LocalInertia != F.After.LocalInertia || F.Before.LocalCOM != F.After.LocalCOM
			|| F.Before.RotationOfMass != F.After.RotationOfMass || F.Before.Sleeping != F.After.Sleeping)
		{ return false; }
		auto* H = Proxy->GetPhysicsThreadAPI();
		auto* Handle = Proxy->GetHandle_LowLevel();
		const auto* Rigid = Handle ? Handle->CastToRigidParticle() : nullptr;
		if (!H || !Rigid || (H->ObjectState() != Chaos::EObjectStateType::Dynamic
			&& H->ObjectState() != Chaos::EObjectStateType::Sleeping)) { return false; }
		const FVector InvI(H->InvI()); FVector Inertia;
		for (int32 I = 0; I < 3; ++I) { Inertia[I] = InvI[I] > 0 ? 1. / InvI[I] : 0; }
		// P/Q are the solved end of this interval. X/R may still describe its start.
		if (!FVector(Rigid->GetP()).Equals(F.Before.Motion.Pose.GetLocation(), 1.e-6)
			|| !FQuat(Rigid->GetQ()).Equals(F.Before.Motion.Pose.GetRotation(), 1.e-6)
			|| !FVector(H->V()).Equals(F.Before.Motion.LinearVelocity, 1.e-6)
			|| !FVector(H->W()).Equals(F.Before.Motion.AngularVelocity, 1.e-6)
			|| H->M() != F.Before.MassKg || Inertia != F.Before.LocalInertia
			|| FVector(H->CenterOfMass()) != F.Before.LocalCOM || FQuat(H->RotationOfMass()) != F.Before.RotationOfMass
			|| (H->ObjectState() == Chaos::EObjectStateType::Sleeping) != F.Before.Sleeping)
		{ return false; }
		// All checks precede the first setter. These setters cannot fail or invoke external observers.
		H->SetX(F.After.Motion.Pose.GetLocation()); H->SetR(F.After.Motion.Pose.GetRotation());
		H->SetV(F.After.Motion.LinearVelocity); H->SetW(F.After.Motion.AngularVelocity);
		LastRevision = F.Revision; LastCommittedSerial = F.Step.EvolutionSerial; Armed = false;
		return true;
	}
}
