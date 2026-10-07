#include "BattleHUDWidgetBase.h"
void UBattleHUDWidgetBase::CancelPlayedCardVisualsForSession(const FPresentationSessionToken& Session)
{
	if (Session.IsValid()) NativeCancelPlayedCardVisualsForSession(Session);
}
void UBattleHUDWidgetBase::CancelAllPlayedCardVisuals()
{
	NativeCancelAllPlayedCardVisuals();
}
