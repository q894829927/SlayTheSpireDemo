#include "BattlePresentationController.h"

#if WITH_DEV_AUTOMATION_TESTS
void UBattlePresentationController::ExpireActivePlaybackForTesting()
{
	HandleActiveTimeout(0.0f);
}
#endif
