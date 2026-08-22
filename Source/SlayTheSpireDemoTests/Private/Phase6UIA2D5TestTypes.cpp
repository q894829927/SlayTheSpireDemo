#include "Phase6UIA2D5TestTypes.h"

bool UPhase6UIA2D5PlaybackWidget::PlayPresentationRecord_Implementation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token
)
{
	++PlayCallCount;
	PlayedRecords.Add(Record);
	PlayedTokens.Add(Token);
	return bAcceptAsyncPlayback;
}

void UPhase6UIA2D5PlaybackWidget::ResetCapture()
{
	PlayCallCount = 0;
	PlayedRecords.Reset();
	PlayedTokens.Reset();
}
