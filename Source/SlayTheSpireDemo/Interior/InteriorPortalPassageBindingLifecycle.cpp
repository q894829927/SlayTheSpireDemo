#include "InteriorPortalPassageBindingLifecycle.h"

#include "HAL/ThreadSafeCounter.h"

namespace InteriorPortalPhysics
{
	namespace
	{
		FThreadSafeCounter64 NextBindingEpoch;
		bool ValidBody(const FTravellerHandle& Body)
		{ return Body.Epoch && Body.Id && Body.Generation; }
	}

	bool FPassageBindingLifecycle::Open(FTravellerHandle Body, uint64 SolverEpoch, uint64 PairGeneration,
		FPassageBindingDomain& Out)
	{
		if (bApplyingFact || Active || Retiring || !ValidBody(Body) || !SolverEpoch || !PairGeneration) { return false; }
		const int64 NewEpoch = NextBindingEpoch.Increment();
		if (NewEpoch <= 0) { return false; }
		FPassageBindingDomain Domain{Body,SolverEpoch,static_cast<uint64>(NewEpoch),PairGeneration};
		Active = MakeUnique<FRecord>(Domain);
		Out = Domain;
		return true;
	}

	bool FPassageBindingLifecycle::BeginRetirement(const FPassageBindingDomain& Domain)
	{
		if (bApplyingFact) { return false; }
		if (Retiring && Retiring->Domain == Domain) { return true; }
		if (!Active || Retiring || !(Active->Domain == Domain)) { return false; }
		Retiring = MoveTemp(Active);
		return true;
	}

	FPassageBindingLifecycle::FRecord* FPassageBindingLifecycle::Find(const FPassageBindingDomain& Domain) const
	{
		if (Active && Active->Domain == Domain) { return Active.Get(); }
		if (Retiring && Retiring->Domain == Domain) { return Retiring.Get(); }
		return nullptr;
	}

	EBindingFactApplyResult FPassageBindingLifecycle::ApplyFact(const FPassageBindingDomain& Domain,
		const FTransferFact& Fact, TFunctionRef<bool(const FTransferFact&)> Apply)
	{
		if (bApplyingFact) { return EBindingFactApplyResult::ApplyRejected; }
		FRecord* Record = Find(Domain);
		if (!Record || Fact.PairGeneration != Domain.PairGeneration) { return EBindingFactApplyResult::WrongDomain; }
		FTransferFactCursor Candidate = Record->Cursor;
		switch (Candidate.Consume(Fact))
		{
		case EFactResult::Duplicate: return EBindingFactApplyResult::Duplicate;
		case EFactResult::Gap: return EBindingFactApplyResult::Gap;
		case EFactResult::WrongDomain: return EBindingFactApplyResult::WrongDomain;
		case EFactResult::Invalid: return EBindingFactApplyResult::Invalid;
		case EFactResult::Consumed: break;
		}
		bApplyingFact = true;
		const bool bApplied = Apply(Fact);
		bApplyingFact = false;
		if (!bApplied) { return EBindingFactApplyResult::ApplyRejected; }
		Record->Cursor = Candidate;
		return EBindingFactApplyResult::Applied;
	}

	bool FPassageBindingLifecycle::Acknowledgment(const FPassageBindingDomain& Domain,
		FTransferAcknowledgment& Out) const
	{
		const FRecord* Record = Find(Domain);
		if (!Record) { return false; }
		Out = Record->Cursor.Acknowledgment();
		return true;
	}

	bool FPassageBindingLifecycle::ConfirmRetired(const FPassageBindingDomain& Domain,
		const FPassageSessionHandoff& Handoff)
	{
		if (bApplyingFact || !Retiring || !(Retiring->Domain == Domain) || Handoff.Body != Domain.Body
			|| Handoff.SolverEpoch != Domain.SolverEpoch || Handoff.BindingEpoch != Domain.BindingEpoch
			|| Handoff.PairGeneration != Domain.PairGeneration || !Handoff.PendingFacts.IsEmpty()
			|| Retiring->Cursor.LastRevision() != Handoff.FinalCommittedRevision) { return false; }
		Retiring.Reset();
		return true;
	}
}
