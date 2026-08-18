#include "BattleTrigger.h"

#include "../Status/StatusInstance.h"

FTriggerContext::FTriggerContext(UStatusInstance* InRuntimeSource, UObject* InActionOuter)
	: RuntimeSource(InRuntimeSource)
	, Owner(IsValid(InRuntimeSource) ? InRuntimeSource->GetOwner() : nullptr)
	, ActionOuter(InActionOuter)
{
}

UStatusInstance* FTriggerContext::GetRuntimeSource() const
{
	return RuntimeSource;
}

ACombatant* FTriggerContext::GetOwner() const
{
	return Owner;
}

UObject* FTriggerContext::GetActionOuter() const
{
	return ActionOuter;
}

bool UBattleTrigger::CanReact(const FBattleEvent& /*Event*/, const FTriggerContext& /*Context*/) const
{
	return false;
}

void UBattleTrigger::BuildReactions(
	const FBattleEvent& /*Event*/,
	const FTriggerContext& /*Context*/,
	TArray<UBattleAction*>& /*OutActions*/
) const
{
}
