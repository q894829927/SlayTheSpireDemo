#include "DeferredHandSelectionAction.h"

#include "BattleActionQueue.h"
#include "SelectionRequestAction.h"
#include "../Battle/BattleManager.h"
#include "../Cards/CardInstance.h"
#include "../Deck/DeckRuntime.h"
#include "../Selection/AuthoredContinuation.h"
#include "../Selection/SelectionResolver.h"
#include "../Selection/SelectionTypes.h"

void UDeferredHandSelectionAction::Initialize(
	UDeckRuntime* InDeck,
	USelectionResolver* InResolver,
	UAuthoredContinuation* InContinuation,
	int32 InRequestedCount,
	FName InSelectionSource
)
{
	Deck = InDeck;
	Resolver = InResolver;
	Continuation = InContinuation;
	RequestedCount = InRequestedCount;
	SelectionSource = InSelectionSource;
}

void UDeferredHandSelectionAction::Execute(UBattleActionQueue* Queue)
{
	if (!IsValid(Queue)
		|| !IsValid(Deck.Get())
		|| !IsValid(Resolver.Get())
		|| !IsValid(Continuation.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Action] DeferredHandSelectionAction skipped: invalid Queue, Deck, Resolver or Continuation."));
		Finish();
		return;
	}

	if (RequestedCount <= 0)
	{
		Finish();
		return;
	}

	FSelectionRequest Request;
	Request.SelectionSource = SelectionSource.IsNone()
		? FName(TEXT("DeferredHandSelection"))
		: SelectionSource;
	Request.CancelPolicy = ESelectionCancelPolicy::Forbidden;

	for (const TObjectPtr<UCardInstance>& HandCard : Deck->GetHandCards())
	{
		if (!IsValid(HandCard.Get()))
		{
			continue;
		}

		FSelectionCandidate Candidate;
		Candidate.RuntimeObject = HandCard.Get();
		Candidate.RuntimeSequence = HandCard->GetRuntimeId();
		Candidate.SelectionKey = HandCard->GetCardId();
		Request.Candidates.Add(Candidate);
	}

	if (Request.Candidates.Num() == 0)
	{
		Finish();
		return;
	}

	const int32 RequiredCount = FMath::Min(RequestedCount, Request.Candidates.Num());
	if (RequiredCount <= 0)
	{
		Finish();
		return;
	}
	Request.MinCount = RequiredCount;
	Request.MaxCount = RequiredCount;

	// A deferred current-Hand choice is an interactive boundary only when the
	// Action belongs to a real BattleManager queue. Seal the already-committed
	// prefix (for Warcry: CardPlayed + Draw) without waiting for playback, then
	// move the still-authored tail onto the continuation Presentation segment.
	FPresentationRecordWriter SelectionWriter = GetPresentationRecordWriter();
	if (ABattleManager* Battle = Cast<ABattleManager>(Queue->GetOuter()))
	{
		SelectionWriter = Battle->AdvancePresentationAtInteractiveSelectionBoundary(this);
		if (!Queue->RebindPendingPresentationRecordWriter(this, SelectionWriter))
		{
			Queue->RequestResolutionFault(FString::Printf(
				TEXT("DeferredHandSelectionAction could not preserve the post-selection Action tail for %s."),
				*Request.SelectionSource.ToString()
			));
			Finish();
			return;
		}
	}

	USelectionRequestAction* SelectionAction = NewObject<USelectionRequestAction>(Queue);
	SelectionAction->Initialize(Resolver.Get(), Request, Continuation.Get());
	SelectionAction->SetPresentationRecordWriter(SelectionWriter);

	if (!Queue->AddToFront(SelectionAction))
	{
		Queue->RequestResolutionFault(FString::Printf(
			TEXT("DeferredHandSelectionAction could not enqueue exact-%d current-Hand selection for %s."),
			RequiredCount,
			*Request.SelectionSource.ToString()
		));
	}
	Finish();
}
