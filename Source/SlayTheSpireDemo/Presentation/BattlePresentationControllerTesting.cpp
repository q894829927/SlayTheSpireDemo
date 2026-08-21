#include "BattlePresentationController.h"

#if WITH_DEV_AUTOMATION_TESTS
void UBattlePresentationController::ExpireActivePlaybackForTesting()
{
	HandleActiveTimeout(0.0f);
}

bool UBattlePresentationController::TryGetWorkingSnapshotForTesting(
	FPresentationStateSnapshot& OutSnapshot
) const
{
	OutSnapshot = FPresentationStateSnapshot{};
	if (!bHasWorkingPresentationSnapshot)
	{
		return false;
	}
	OutSnapshot = WorkingPresentationSnapshot;
	return true;
}
#endif
