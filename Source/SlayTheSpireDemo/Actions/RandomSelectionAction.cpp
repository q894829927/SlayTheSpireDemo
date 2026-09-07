#include "RandomSelectionAction.h"

#include "BattleActionQueue.h"
#include "../Selection/AuthoredContinuation.h"

namespace
{
	bool ValidateExactRandomRequest(const FSelectionRequest& Request, FString& OutReason)
	{
		if (Request.Candidates.Num() == 0
			|| Request.MinCount <= 0
			|| Request.MinCount != Request.MaxCount
			|| Request.MaxCount > Request.Candidates.Num())
		{
			OutReason = TEXT("random selection requires a non-empty exact-N candidate request");
			return false;
		}

		TSet<const UObject*> SeenObjects;
		for (const FSelectionCandidate& Candidate : Request.Candidates)
		{
			UObject* RuntimeObject = Candidate.RuntimeObject.Get();
			if (!IsValid(RuntimeObject))
			{
				OutReason = TEXT("random selection candidate object is invalid");
				return false;
			}
			if (SeenObjects.Contains(RuntimeObject))
			{
				OutReason = TEXT("random selection candidate object appears more than once");
				return false;
			}
			SeenObjects.Add(RuntimeObject);
		}
		return true;
	}
}

void URandomSelectionAction::Initialize(
	const FSelectionRequest& InRequest,
	UAuthoredContinuation* InContinuation,
	const FSelectionRandomIndexChooser& InRandomIndexChooser
)
{
	Request = InRequest;
	Continuation = InContinuation;
	RandomIndexChooser = InRandomIndexChooser;
}

void URandomSelectionAction::Execute(UBattleActionQueue* Queue)
{
	if (!IsValid(Queue))
	{
		Finish();
		return;
	}

	if (!IsValid(Continuation.Get()) || !RandomIndexChooser.IsBound())
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("RandomSelectionAction missing Continuation or RNG provider for %s."),
			*Request.SelectionSource.ToString()
		));
		Finish();
		return;
	}

	FString ValidationReason;
	if (!ValidateExactRandomRequest(Request, ValidationReason))
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("RandomSelectionAction received malformed request for %s: %s."),
			*Request.SelectionSource.ToString(),
			*ValidationReason
		));
		Finish();
		return;
	}

	const int32 RequiredCount = Request.MinCount;
	TSet<int32> SelectedCandidateIndices;

	// Selecting every candidate has no random decision to make. Preserve RNG state
	// rather than advancing it for a result whose membership is already forced.
	if (RequiredCount == Request.Candidates.Num())
	{
		for (int32 CandidateIndex = 0; CandidateIndex < Request.Candidates.Num(); ++CandidateIndex)
		{
			SelectedCandidateIndices.Add(CandidateIndex);
		}
	}
	else
	{
		TArray<int32> RemainingCandidateIndices;
		RemainingCandidateIndices.Reserve(Request.Candidates.Num());
		for (int32 CandidateIndex = 0; CandidateIndex < Request.Candidates.Num(); ++CandidateIndex)
		{
			RemainingCandidateIndices.Add(CandidateIndex);
		}

		for (int32 PickNumber = 0; PickNumber < RequiredCount; ++PickNumber)
		{
			int32 RemainingIndex = INDEX_NONE;
			if (!RandomIndexChooser.Execute(RemainingCandidateIndices.Num(), RemainingIndex)
				|| !RemainingCandidateIndices.IsValidIndex(RemainingIndex))
			{
				Queue->RequestResolutionFault(FString::Printf(
					TEXT("RandomSelectionAction RNG provider failed for %s at pick %d/%d."),
					*Request.SelectionSource.ToString(),
					PickNumber + 1,
					RequiredCount
				));
				Finish();
				return;
			}

			SelectedCandidateIndices.Add(RemainingCandidateIndices[RemainingIndex]);
			// Preserve the remaining ordered candidate list. Randomness decides only
			// membership; later Gameplay ordering is canonical candidate order.
			RemainingCandidateIndices.RemoveAt(RemainingIndex);
		}
	}

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Reserve(RequiredCount);
	for (int32 CandidateIndex = 0; CandidateIndex < Request.Candidates.Num(); ++CandidateIndex)
	{
		if (SelectedCandidateIndices.Contains(CandidateIndex))
		{
			Result.SelectedObjects.Add(Request.Candidates[CandidateIndex].RuntimeObject.Get());
		}
	}

	if (Result.SelectedObjects.Num() != RequiredCount)
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("RandomSelectionAction could not build exact result count for %s."),
			*Request.SelectionSource.ToString()
		));
		Finish();
		return;
	}

	TArray<UBattleAction*> ContinuationBatch;
	if (!Continuation->BuildNextActions(Result, Queue, ContinuationBatch))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Selection] RandomSelectionAction continuation declined resolved result for %s."),
			*Request.SelectionSource.ToString()
		);
		Finish();
		return;
	}

	for (UBattleAction* ContinuationAction : ContinuationBatch)
	{
		if (!IsValid(ContinuationAction)
			|| ContinuationAction->IsFinished()
			|| ContinuationAction->GetOuter() != Queue)
		{
			Queue->RequestResolutionFault(FString::Printf(
				TEXT("RandomSelectionAction received invalid continuation Action for %s."),
				*Request.SelectionSource.ToString()
			));
			Finish();
			return;
		}
		ContinuationAction->SetPresentationRecordWriter(GetPresentationRecordWriter());
	}

	if (ContinuationBatch.Num() > 0
		&& !Queue->AddBatchToFrontPreserveOrder(ContinuationBatch))
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("RandomSelectionAction could not insert continuation batch (%d actions) for %s."),
			ContinuationBatch.Num(),
			*Request.SelectionSource.ToString()
		));
		Finish();
		return;
	}

	Finish();
}
