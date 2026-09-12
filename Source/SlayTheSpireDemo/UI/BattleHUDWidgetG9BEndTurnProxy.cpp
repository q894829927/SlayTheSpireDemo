#include "BattleHUDWidgetG9BEndTurnProxy.h"

#include "BattleHUDWidget.h"
#include "BattleHUDWidgetG9BInput.h"

void UBattleHUDWidgetG9BEndTurnProxy::Initialize(UBattleHUDWidget* InWidget)
{
	Widget = InWidget;
}

void UBattleHUDWidgetG9BEndTurnProxy::HandleClicked()
{
	UBattleHUDWidget* Current = Widget.Get();
	if (!IsValid(Current))
	{
		return;
	}

	if (BattleHUDWidgetG9BInput::TryHandleEndTurn(Current))
	{
		return;
	}

	// G9 disabled or current authority not accepted: preserve the sealed G8
	// RequestEndTurn path exactly, including ChoosingTarget rejection.
	Current->EndTurn();
}
