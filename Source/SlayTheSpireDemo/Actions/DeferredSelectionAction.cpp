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
	if (!IsValid(Queue) || RequestedCount <= 0) { Finish(); return; }
	if (!IsValid(CandidateSource) || !IsValid(Continuation)
		|| (Mode == EDeferredSelectionMode::Player && !IsValid(Resolver))
		|| (Mode == EDeferredSelectionMode::Random && !RandomChooser.IsBound()))
	{
		Queue->RequestResolutionFault(TEXT("DeferredSelection missing runtime dependency."));
		Finish(); return;
	}
	FSelectionRequest Request;
	const ESelectionCandidateBuildStatus Status = CandidateSource->BuildCandidates(Request.Candidates);
	if (Status == ESelectionCandidateBuildStatus::NoCandidates) { Finish(); return; }
	if (Status != ESelectionCandidateBuildStatus::Success || Request.Candidates.IsEmpty())
	{
		Queue->RequestResolutionFault(TEXT("DeferredSelection candidate capture failed."));
		Finish(); return;
	}
	check(CountPolicy == ESelectionCountPolicy::ExactNClampToAvailable);
	Request.MinCount = Request.MaxCount = FMath::Min(RequestedCount, Request.Candidates.Num());
	Request.CancelPolicy = CancelPolicy;
	Request.SelectionSource = SelectionSource;
	FPresentationRecordWriter Writer = GetPresentationRecordWriter();
	UBattleAction* Next = nullptr;
	if (Mode == EDeferredSelectionMode::Player)
	{
		if (BoundaryAccess.IsBound())
		{
			Writer = BoundaryAccess.Execute(this);
			if (!Queue->RebindPendingPresentationRecordWriter(this, Writer))
			{
				Queue->RequestResolutionFault(TEXT("DeferredSelection could not rebind the continuation tail."));
				Finish(); return;
			}
		}
		USelectionRequestAction* Selection = NewObject<USelectionRequestAction>(Queue);
		Selection->Initialize(Resolver, Request, Continuation);
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
