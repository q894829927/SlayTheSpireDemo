#pragma once

#include "InteriorPortalChaosPassageSession.h"
#include "Templates/Function.h"

namespace InteriorPortalPhysics
{
	/** A binding domain is independent of the current pair and command revision. */
	struct FPassageBindingDomain
	{
		FTravellerHandle Body;
		uint64 SolverEpoch = 0;
		uint64 BindingEpoch = 0;
		uint64 PairGeneration = 0;
		bool operator==(const FPassageBindingDomain& Other) const
		{
			return Body == Other.Body && SolverEpoch == Other.SolverEpoch
				&& BindingEpoch == Other.BindingEpoch && PairGeneration == Other.PairGeneration;
		}
	};
	enum class EBindingFactApplyResult : uint8
	{
		Applied, Duplicate, Gap, WrongDomain, Invalid, ApplyRejected
	};

	/** GT-side fact/ack owner for one traveller. The PT session is owned separately.
	 * Opening is blocked until the prior session confirms retirement and journal drain.
	 * ApplyFact validates a tentative cursor, invokes reconciliation once, and only
	 * then advances the acknowledged prefix. The callback must not mutate this owner.
	 */
	class SLAYTHESPIREDEMO_API FPassageBindingLifecycle final
	{
	public:
		FPassageBindingLifecycle() = default;
		FPassageBindingLifecycle(const FPassageBindingLifecycle&) = delete;
		FPassageBindingLifecycle& operator=(const FPassageBindingLifecycle&) = delete;
		bool Open(FTravellerHandle Body, uint64 SolverEpoch, uint64 PairGeneration, FPassageBindingDomain& Out);
		bool BeginRetirement(const FPassageBindingDomain& Domain);
		EBindingFactApplyResult ApplyFact(const FPassageBindingDomain& Domain, const FTransferFact& Fact,
			TFunctionRef<bool(const FTransferFact&)> Apply);
		bool Acknowledgment(const FPassageBindingDomain& Domain, FTransferAcknowledgment& Out) const;
		bool ConfirmRetired(const FPassageBindingDomain& Domain, const FPassageSessionHandoff& Handoff);
		bool HasActive() const { return Active.IsValid(); }
		bool HasRetiring() const { return Retiring.IsValid(); }
	private:
		struct FRecord
		{
			explicit FRecord(const FPassageBindingDomain& InDomain)
				: Domain(InDomain), Cursor(InDomain.Body,InDomain.SolverEpoch,InDomain.BindingEpoch) {}
			FPassageBindingDomain Domain;
			FTransferFactCursor Cursor;
		};
		FRecord* Find(const FPassageBindingDomain& Domain) const;
		TUniquePtr<FRecord> Active;
		TUniquePtr<FRecord> Retiring;
		bool bApplyingFact = false;
	};
}
