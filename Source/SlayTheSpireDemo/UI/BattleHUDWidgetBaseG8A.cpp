#include "BattleHUDWidgetBase.h"

bool UBattleHUDWidgetBase::PrepareDetachedDamageVisual(
	const FPresentationSessionToken& /*SessionToken*/,
	const FPresentationRecord& /*Record*/,
	int64 /*SourceFinalStateRevision*/,
	float /*VisualDuration*/,
	FDetachedDamageToken& OutToken)
{
	OutToken = FDetachedDamageToken{};
	return false;
}

bool UBattleHUDWidgetBase::ActivatePreparedDetachedDamageVisual(
	const FDetachedDamageToken& /*Token*/)
{
	return false;
}

bool UBattleHUDWidgetBase::CancelDetachedDamageVisual(
	const FDetachedDamageToken& /*Token*/)
{
	return false;
}

int32 UBattleHUDWidgetBase::CancelDetachedDamageVisualsForSession(
	const FPresentationSessionToken& /*SessionToken*/)
{
	return 0;
}

void UBattleHUDWidgetBase::CancelAllDetachedDamageVisuals()
{
}

void UBattleHUDWidgetBase::PlayCommittedDamageCombatantCues(
	const FPresentationRecord& /*Record*/,
	const FPresentationSessionToken& /*SessionToken*/)
{
}
