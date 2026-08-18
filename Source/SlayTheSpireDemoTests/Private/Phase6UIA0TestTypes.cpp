#include "Phase6UIA0TestTypes.h"

void UPhase6UIA0ManualFinishAction::Execute(UBattleActionQueue* /*Queue*/)
{
	bExecuted = true;
	// Intentionally do not Finish(). The test controls completion explicitly so
	// Queue resolution remains genuinely asynchronous from the caller's view.
}

void UPhase6UIA0ManualFinishAction::CompleteManually()
{
	if (bExecuted && !IsFinished())
	{
		Finish();
	}
}

bool UPhase6UIA0ManualFinishAction::HasExecuted() const
{
	return bExecuted;
}
