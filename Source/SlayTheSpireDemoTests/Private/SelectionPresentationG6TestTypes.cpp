#include "SelectionPresentationG6TestTypes.h"

#include "Presentation/BattlePresentationController.h"
#include "UI/BattleHUDViewModel.h"

bool USelectionPresentationG6ControllerWidget::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	++RecordPlayCallCount;
	LastRecordToken = Token;
	HandCountsAtRecordOffer.Add(IsValid(ViewModel) ? ViewModel->HandCards.Num() : INDEX_NONE);

	if (IsValid(PresentationController))
	{
		if (PresentationController->ConsumeVisuallyPresentedGroupRecordG6(Record, Token))
		{
			++AlreadyPresentedConsumeCount;
			return false;
		}

		if (Record.Group.IsValid()
			&& Record.Group.Kind == EPresentationGroupKind::SelectionDestination
			&& Record.Group.ExpectedMemberCount > 1
			&& PresentationController->TryActivatePresentationGroupG6(Record, Token))
		{
			return true;
		}
	}

	return bAcceptFallbackSingleRecordPlayback;
}

bool USelectionPresentationG6ControllerWidget::BeginPresentationGroupPlayback(
	const TArray<FPresentationRecord>& Records,
	const FPresentationGroupTag& Group,
	const FPresentationPlaybackToken& Token)
{
	++GroupPlayCallCount;
	LastGroup = Group;
	LastGroupToken = Token;
	GroupRecordIndicesObserved.Reset();
	for (const FPresentationRecord& Record : Records)
	{
		GroupRecordIndicesObserved.Add(static_cast<int32>(Record.PresentationSequence));
	}
	return bAcceptGroupPlayback;
}

void USelectionPresentationG6ControllerWidget::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& /*Token*/)
{
	++RecordCancelCallCount;
}

void USelectionPresentationG6ControllerWidget::CancelPresentationGroupPlayback(
	const FPresentationGroupTag& /*Group*/,
	const FPresentationPlaybackToken& /*Token*/)
{
	++GroupCancelCallCount;
}

void USelectionPresentationG6ControllerWidget::CompleteAcceptedGroup()
{
	if (LastGroupToken.IsValid())
	{
		NotifyPresentationGroupFinishedG6(LastGroupToken);
	}
}

void USelectionPresentationG6ControllerWidget::CompleteLastSingleRecord()
{
	if (LastRecordToken.IsValid())
	{
		NotifyPresentationFinished(LastRecordToken);
	}
}
