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
	EG9BCardClickDisposition TryHandleBufferedCardClick(
		UBattleHUDWidget* Widget,
		int32 RuntimeId);

	void RefreshBufferedCardIntent(UBattleHUDWidget* Widget);
	void ClearBufferedCardIntent(UBattleHUDWidget* Widget);
	bool HasBufferedCardIntent(const UBattleHUDWidget* Widget);
}
