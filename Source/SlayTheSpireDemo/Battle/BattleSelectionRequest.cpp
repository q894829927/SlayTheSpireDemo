#include "BattleSelectionRequest.h"

#include "BattleManager.h"
#include "../Cards/CardInstance.h"
#include "../Selection/SelectionResolver.h"
#include "../Selection/SelectionTypes.h"

namespace
{
	bool IsSupportedExactCardRequest(const FSelectionRequest* Request)
	{
		if (Request == nullptr
			|| Request->MinCount <= 0
			|| Request->MinCount != Request->MaxCount
			|| Request->MaxCount > Request->Candidates.Num()
			|| Request->Candidates.Num() == 0)
		{
			return false;
		}

		TSet<int32> SeenRuntimeIds;
		for (const FSelectionCandidate& Candidate : Request->Candidates)
		{
			const UCardInstance* Card = Cast<UCardInstance>(Candidate.RuntimeObject.Get());
			if (!IsValid(Card)
				|| Candidate.RuntimeSequence == INDEX_NONE
				|| Candidate.RuntimeSequence != Card->GetRuntimeId()
				|| SeenRuntimeIds.Contains(Candidate.RuntimeSequence))
			{
				return false;
			}
			SeenRuntimeIds.Add(Candidate.RuntimeSequence);
		}
		return true;
	}
}

bool BattleSelectionRequest::TryBuildPendingCardSelectionReadView(
	const ABattleManager* Battle,
	FPendingCardSelectionReadView& OutView
)
{
	OutView = FPendingCardSelectionReadView{};
	if (!IsValid(Battle))
	{
		return false;
	}

	const USelectionResolver* Resolver = Battle->GetSelectionResolver();
	const FSelectionRequest* Request = IsValid(Resolver) ? Resolver->GetPendingRequest() : nullptr;
	if (!IsSupportedExactCardRequest(Request))
	{
		return false;
	}

	OutView.SelectionSource = Request->SelectionSource;
	OutView.RequiredCount = Request->MinCount;
	OutView.bCanCancel = Resolver->CanCancelPendingSelection();
	OutView.CandidateRuntimeIds.Reserve(Request->Candidates.Num());
	for (const FSelectionCandidate& Candidate : Request->Candidates)
	{
		OutView.CandidateRuntimeIds.Add(Candidate.RuntimeSequence);
	}
	return true;
}

bool BattleSelectionRequest::SubmitPendingCardSelection(
	ABattleManager* Battle,
	const TArray<int32>& CardRuntimeIds
)
{
	if (!IsValid(Battle))
	{
		return false;
	}

	USelectionResolver* Resolver = Battle->GetSelectionResolver();
	const FSelectionRequest* Request = IsValid(Resolver) ? Resolver->GetPendingRequest() : nullptr;
	if (!IsSupportedExactCardRequest(Request)
		|| CardRuntimeIds.Num() != Request->MinCount)
	{
		return false;
	}

	TSet<int32> RequestedRuntimeIds;
	for (const int32 RuntimeId : CardRuntimeIds)
	{
		if (RuntimeId == INDEX_NONE
			|| RequestedRuntimeIds.Contains(RuntimeId))
		{
			return false;
		}

		const bool bIsCandidate = Request->Candidates.ContainsByPredicate(
			[RuntimeId](const FSelectionCandidate& Candidate)
			{
				return Candidate.RuntimeSequence == RuntimeId;
			}
		);
		if (!bIsCandidate)
		{
			return false;
		}
		RequestedRuntimeIds.Add(RuntimeId);
	}

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Reserve(CardRuntimeIds.Num());

	// Rebuild in authoritative candidate order. Player click order is not an
	// implicit Gameplay ordering control for later Exhaust/Trigger resolution.
	for (const FSelectionCandidate& Candidate : Request->Candidates)
	{
		if (!RequestedRuntimeIds.Contains(Candidate.RuntimeSequence))
		{
			continue;
		}

		UCardInstance* Card = Cast<UCardInstance>(Candidate.RuntimeObject.Get());
		if (!IsValid(Card)
			|| Card->GetRuntimeId() != Candidate.RuntimeSequence)
		{
			return false;
		}
		Result.SelectedObjects.Add(Card);
	}

	if (Result.SelectedObjects.Num() != CardRuntimeIds.Num())
	{
		return false;
	}
	return Resolver->SubmitResult(Result);
}

bool BattleSelectionRequest::SubmitPendingSelectionCancel(ABattleManager* Battle)
{
	if (!IsValid(Battle))
	{
		return false;
	}

	USelectionResolver* Resolver = Battle->GetSelectionResolver();
	return IsValid(Resolver) && Resolver->SubmitCancel();
}
