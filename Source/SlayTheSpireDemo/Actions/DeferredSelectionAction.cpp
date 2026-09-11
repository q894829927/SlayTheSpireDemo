#include "DeferredSelectionAction.h"
#include "BattleActionQueue.h"
#include "SelectionRequestAction.h"
#include "../Selection/SelectionCandidateSource.h"
#include "../Selection/SelectionResolver.h"
#include "../Selection/AuthoredContinuation.h"

void UDeferredSelectionAction::Initialize(USelectionCandidateSource* InSource, USelectionResolver* InResolver,
	UAuthoredContinuation* InContinuation, int32 InCount, ESelectionCancelPolicy InCancelPolicy,
	FName InSelectionSource, EDeferredSelectionMode InMode, FSelectionInteractiveBoundaryAccess InBoundary,
	FSelectionRandomIndexChooser InRandom)
{
	CandidateSource = InSource;
	Resolver = InResolver;
	Continuation = InContinuation;
	RequestedCount = InCount;
	CancelPolicy = InCancelPolicy;
	SelectionSource = InSelectionSource;
	Mode = InMode;
	BoundaryAccess = MoveTemp(InBoundary);
	RandomChooser = MoveTemp(InRandom);
}

void UDeferredSelectionAction::Execute(UBattleActionQueue* Queue)
{
	if (!IsValid(Queue) || RequestedCount <= 0)
	{
		Finish();
		return;
	}

	if (!IsValid(CandidateSource) || !IsValid(Continuation))
	{
		Queue->RequestResolutionFault(TEXT("DeferredSelection missing CandidateSource or Continuation."));
		Finish();
		return;
	}

	FSelectionRequest Request;
	const ESelectionCandidateBuildStatus Status = CandidateSource->BuildCandidates(Request.Candidates);
	if (Status == ESelectionCandidateBuildStatus::NoCandidates)
	{
		Finish();
		return;
	}
	if (Status != ESelectionCandidateBuildStatus::Success || Request.Candidates.IsEmpty())
	{
		Queue->RequestResolutionFault(TEXT("DeferredSelection candidate capture failed."));
		Finish();
		return;
	}

	// A real non-empty Player decision must always cross the shared interactive
	// boundary. An unbound boundary is a framework wiring error, not permission
	// to expose a pending request against stale Presentation state. Zero-count and
	// legal empty-candidate cases have already returned above and create no fence.
	if (Mode == EDeferredSelectionMode::Player)
	{
		if (!IsValid(Resolver) || !BoundaryAccess.IsBound())
		{
			Queue->RequestResolutionFault(TEXT("DeferredSelection Player mode requires a Resolver and interactive Presentation boundary."));
			Finish();
			return;
		}
	}
	else if (!RandomChooser.IsBound())
	{
		Queue->RequestResolutionFault(TEXT("DeferredSelection Random mode requires an authoritative RNG provider."));
		Finish();
		return;
	}

	check(CountPolicy == ESelectionCountPolicy::ExactNClampToAvailable);
	Request.MinCount = Request.MaxCount = FMath::Min(RequestedCount, Request.Candidates.Num());
	Request.CancelPolicy = CancelPolicy;
	Request.SelectionSource = SelectionSource;

	FPresentationRecordWriter Writer = GetPresentationRecordWriter();
	UBattleAction* Next = nullptr;
	if (Mode == EDeferredSelectionMode::Player)
	{
		Writer = BoundaryAccess.Execute(this);
		if (Writer.GetBattleId() == 0 || Writer.GetSelectionBoundaryRevision() <= 0)
		{
			Queue->RequestResolutionFault(TEXT("DeferredSelection interactive boundary did not provide an exact pending-selection identity."));
			Finish();
			return;
		}
		if (!Queue->RebindPendingPresentationRecordWriter(this, Writer))
		{
			Queue->RequestResolutionFault(TEXT("DeferredSelection could not rebind the continuation tail."));
			Finish();
			return;
		}

		FPendingSelectionRequestIdentity RequestIdentity;
		RequestIdentity.BattleId = static_cast<int64>(Writer.GetBattleId());
		RequestIdentity.SelectionBoundaryRevision = Writer.GetSelectionBoundaryRevision();

		USelectionRequestAction* Selection = NewObject<USelectionRequestAction>(Queue);
		Selection->Initialize(Resolver, Request, Continuation, RequestIdentity);
		Next = Selection;
	}
	else
	{
		URandomSelectionAction* Selection = NewObject<URandomSelectionAction>(Queue);
		Selection->Initialize(Request, Continuation, RandomChooser);
		Next = Selection;
	}

	Next->SetPresentationRecordWriter(Writer);
	if (!Queue->AddToFront(Next))
	{
		Queue->RequestResolutionFault(TEXT("DeferredSelection could not enqueue required selection."));
	}
	Finish();
}
