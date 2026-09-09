#include "SelectionPresentationG3TestTypes.h"

bool USelectionPresentationG3PlaybackWidget::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& /*Record*/,
	const FPresentationPlaybackToken& Token
)
{
	++RecordPlayCallCount;
	LastRecordToken = Token;
	if (bNotifySynchronouslyFromRecordPlay)
	{
		NotifyPresentationFinished(Token);
	}
	return bAcceptAsyncRecordPlayback;
}

bool USelectionPresentationG3PlaybackWidget::BeginPresentationGroupPlayback(
	const TArray<FPresentationRecord>& /*Records*/,
	const FPresentationGroupTag& Group,
	const FPresentationPlaybackToken& Token
)
{
	++GroupPlayCallCount;
	LastGroup = Group;
	LastGroupToken = Token;
	if (bNotifySynchronouslyFromGroupPlay)
	{
		NotifyPresentationFinished(Token);
	}
	return bAcceptAsyncGroupPlayback;
}

void USelectionPresentationG3PlaybackWidget::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& Token
)
{
	++RecordCancelCallCount;
	LastCancelledRecordToken = Token;
}

void USelectionPresentationG3PlaybackWidget::CancelPresentationGroupPlayback(
	const FPresentationGroupTag& Group,
	const FPresentationPlaybackToken& Token
)
{
	++GroupCancelCallCount;
	LastCancelledGroup = Group;
	LastCancelledGroupToken = Token;
}