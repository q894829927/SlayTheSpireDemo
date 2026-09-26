#pragma once

#include "InteriorPortalHoldSolver.h"

namespace InteriorPortalPhysics
{
	enum class EPassagePhase : uint8 { Outside, Entering, Straddling, Exiting, Retired };
	struct FBodyCommitState
	{
		FBoundaryState Motion;
		double MassKg = 0;
		FVector LocalInertia = FVector::ZeroVector, LocalCOM = FVector::ZeroVector;
		FQuat RotationOfMass = FQuat::Identity;
		bool Sleeping = false;
	};
	struct FHoldRelationSnapshot
	{
		uint64 Instance = 0, HolderTransferRevision = 0;
		FTravellerHandle Holder;
		FVector LocalAnchor = FVector::ZeroVector;
		FTransform DesiredHolderPose = FTransform::Identity;
		FHoldRoute Route;
		bool Active = false;
	};
	struct FTransferFact
	{
		FTravellerHandle Body;
		uint64 SolverEpoch = 0, BindingEpoch = 0, PairGeneration = 0, Revision = 0;
		FPhysicsStepKey Step;
		int32 FromEndpoint = 0, ToEndpoint = 1;
		FBodyCommitState Before, After;
		FHoldRelationSnapshot HoldAfter;
	};
	struct FTransferAcknowledgment
	{
		FTravellerHandle Body;
		uint64 SolverEpoch = 0, BindingEpoch = 0, LastConsumedRevision = 0;
	};
	/** Must either write this complete transfer or return false without any physical write. */
	class IPortalTransferAdapter
	{
	public:
		virtual ~IPortalTransferAdapter() = default;
		virtual bool Commit(const FTransferFact& Transfer) = 0;
	};
	enum class ETransferResult : uint8 { Committed, NoCrossing, Rejected, AdapterRejected, AlreadyCommitted, JournalFull, Retired };
	enum class EFactResult : uint8 { Consumed, Duplicate, Gap, WrongDomain, Invalid };
	/** Observation only: freshness of current intent/topology is deliberately absent. */
	class SLAYTHESPIREDEMO_API FTransferFactCursor
	{
	public:
		FTransferFactCursor(FTravellerHandle Body, uint64 SolverEpoch, uint64 BindingEpoch)
			: DomainBody(Body), Epoch(SolverEpoch), Binding(BindingEpoch) {}
		EFactResult Consume(const FTransferFact& Fact);
		uint64 LastRevision() const { return Revision; }
		FTransferAcknowledgment Acknowledgment() const { return {DomainBody,Epoch,Binding,Revision}; }
	private:
		FTravellerHandle DomainBody;
		uint64 Epoch = 0, Binding = 0, Revision = 0;
		FPhysicsStepKey LastStep;
	};
	/** Solver-side metadata/commit owner. It never integrates ordinary physical motion. */
	class SLAYTHESPIREDEMO_API FPassageCoordinator
	{
	public:
		FPassageCoordinator(FTravellerHandle Body, uint64 Pair, uint64 SolverEpoch, uint64 BindingEpoch,
			const FTransform& Endpoint0, const FTransform& Endpoint1, int32 InitialEndpoint = 0,
			int32 JournalCapacity = 32);
		FPassageCoordinator(const FPassageCoordinator&) = delete;
		FPassageCoordinator& operator=(const FPassageCoordinator&) = delete;
		FBoundaryDecision EvaluateInterval(const FBoundaryCommand& Command, const FBoundaryState& Start,
			const FBoundaryState& Integrated, const FPhysicsStepKey& Step, uint32 ReuseOrdinal, bool OtherContact);
		void RevokeInterval(EBoundaryReason Reason);
		ETransferResult CommitSolved(const FBoundaryCommand& Command, const FBodyCommitState& Solved,
			const FPhysicsStepKey& Step, IPortalTransferAdapter& Adapter);
		bool AcquireHold(uint64 Instance, FTravellerHandle Holder, int32 HolderEndpoint,
			const FTransform& DesiredHolderPose, const FVector& LocalAnchor);
		bool SetHolderIntent(uint64 Instance, FTravellerHandle Holder, uint64 HolderTransferRevision,
			const FTransform& DesiredHolderPose);
		bool HolderCrossed(uint64 Instance, FTravellerHandle Holder, uint64 TransferRevision, int32 FromEndpoint);
		bool ReleaseHold(uint64 Instance);
		FHoldRelationSnapshot Hold() const;
		void Retire();
		bool Acknowledge(const FTransferAcknowledgment& Ack);
		const TArray<FTransferFact>& PendingFacts() const { return Journal; }
		uint64 TransferRevision() const { return CommittedRevision; }
		int32 BodyEndpoint() const { return Endpoint; }
		EPassagePhase Phase() const { return Passage; }
		const FBoundaryDecision& Decision() const { return CurrentDecision; }
	private:
		bool Matches(const FBoundaryCommand& C, const FPhysicsStepKey& Step) const;
		FTravellerHandle Body;
		uint64 Pair = 0, Epoch = 0, Binding = 0, CommittedRevision = 0, AckRevision = 0, LastHoldInstance = 0, RelationRevision = 1;
		FTransform Frames[2];
		int32 Endpoint = 0, HolderEndpoint = 0, Capacity = 32;
		EPassagePhase Passage = EPassagePhase::Outside;
		FBoundaryDecision CurrentDecision;
		FPhysicsStepKey CurrentStep, LastCommitStep;
		FBoundaryState IntervalStart;
		FBoundaryCommand IntervalCommand;
		uint64 IntervalCommandRevision = 0;
		uint32 IntervalReuse = 0;
		FHoldRelationSnapshot Relation;
		TArray<FTransferFact> Journal;
	};
}
