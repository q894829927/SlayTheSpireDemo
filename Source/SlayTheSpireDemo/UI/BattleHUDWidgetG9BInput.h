#pragma once

#include "CoreMinimal.h"

class UBattleHUDWidget;

enum class EG9BCardClickDisposition : uint8
{
	NotHandled,
	Buffered,
	Rejected
};

namespace BattleHUDWidgetG9BInput
{
	bool IsEnabled();

	EG9BCardClickDisposition TryHandleBufferedCardClick(
		UBattleHUDWidget* Widget,
		int32 RuntimeId);

	bool CanAcceptEndTurn(const UBattleHUDWidget* Widget);
	bool TryHandleEndTurn(UBattleHUDWidget* Widget);

	void RefreshBufferedCardIntent(UBattleHUDWidget* Widget);
	void ClearBufferedCardIntent(UBattleHUDWidget* Widget);
	bool HasBufferedCardIntent(const UBattleHUDWidget* Widget);

	// Accepted EndTurn has higher priority than a pending G8 FastInput retry.
	// The fence is consumed by RetryPendingFastCardSelection before that old click
	// can fire after the decisive EndTurn input.
	void MarkFastInputRetiredByEndTurn(UBattleHUDWidget* Widget);
	bool ConsumeFastInputRetirementFence(UBattleHUDWidget* Widget);
	void ClearFastInputRetirementFence(UBattleHUDWidget* Widget);
	void ClearWidgetState(UBattleHUDWidget* Widget);

#if WITH_DEV_AUTOMATION_TESTS
	void SetEnabledForTesting(bool bEnabled);
#endif
}
