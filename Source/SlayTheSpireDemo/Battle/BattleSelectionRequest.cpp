#include "BattleSelectionRequest.h"

#include "BattleManager.h"
#include "../Cards/CardInstance.h"
#include "../Selection/SelectionResolver.h"
#include "../Selection/SelectionTypes.h"

namespace
{
	bool IsSupportedSingleCardRequest(const FSelectionRequest* Request)
	{
		if (Request == nullptr
			|| Request->MinCount != 1
			|| Request->MaxCount != 1
			|| Request->Candidates.Num() == 0)
		{
			return false;
		}

		for (const FSelectionCandidate& Candidate : Request->Candidates)
		{
			const UCardInstance* Card = Cast<UCardInstance>(Candidate.RuntimeObject.Get());
			if (!IsValid(Card)
				|| Candidate.RuntimeSequence == INDEX_NONE
				|| Candidate.RuntimeSequence != Card->GetRuntimeId())
			{
				return false;
			}
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
	if (!IsSupportedSingleCardRequest(Request))
	{
		return false;
	}

	OutView.SelectionSource = Request->SelectionSource;
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
	int32 CardRuntimeId
)
{
	if (!IsValid(Battle) || CardRuntimeId == INDEX_NONE)
	{
		return false;
	}

	USelectionResolver* Resolver = Battle->GetSelectionResolver();
	const FSelectionRequest* Request = IsValid(Resolver) ? Resolver->GetPendingRequest() : nullptr;
	if (!IsSupportedSingleCardRequest(Request))
	{
		return false;
	}

	const FSelectionCandidate* Candidate = Request->Candidates.FindByPredicate(
		[CardRuntimeId](const FSelectionCandidate& Item)
		{
			return Item.RuntimeSequence == CardRuntimeId;
		}
	);
	if (Candidate == nullptr)
	{
		return false;
	}

	UCardInstance* Card = Cast<UCardInstance>(Candidate->RuntimeObject.Get());
	if (!IsValid(Card) || Card->GetRuntimeId() != CardRuntimeId)
	{
		return false;
	}

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(Card);
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
