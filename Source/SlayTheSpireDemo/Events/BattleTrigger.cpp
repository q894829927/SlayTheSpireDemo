#include "BattleTrigger.h"

#include "../Relics/RelicInstance.h"
#include "../Status/StatusInstance.h"

FTriggerRuntimeSource FTriggerRuntimeSource::FromStatus(UStatusInstance* Instance)
{
	FTriggerRuntimeSource Source;
	Source.Kind = ETriggerRuntimeSourceKind::Status;
	Source.RuntimeObject = Instance;
	if (IsValid(Instance))
	{
		Source.SourceId = Instance->GetStatusId();
		Source.RuntimeSequence = Instance->GetRuntimeSequence();
		Source.CombatantOwner = Instance->GetOwner();
	}
	return Source;
}

FTriggerRuntimeSource FTriggerRuntimeSource::FromRelic(URelicInstance* Instance)
{
	FTriggerRuntimeSource Source;
	Source.Kind = ETriggerRuntimeSourceKind::Relic;
	Source.RuntimeObject = Instance;
	if (IsValid(Instance))
	{
		Source.SourceId = Instance->GetRelicId();
		Source.RuntimeSequence = Instance->GetRuntimeSequence();
	}
	return Source;
}

FTriggerContext::FTriggerContext(
	UStatusInstance* InRuntimeSource,
	UObject* InActionOuter,
	const FPresentationRecordWriter& InPresentationRecordWriter
)
	: FTriggerContext(
		FTriggerRuntimeSource::FromStatus(InRuntimeSource),
		InActionOuter,
		nullptr,
		InPresentationRecordWriter
	)
{
}

FTriggerContext::FTriggerContext(
	UStatusInstance* InRuntimeSource,
	UObject* InActionOuter,
	ABattleManager* InBattle,
	const FPresentationRecordWriter& InPresentationRecordWriter
)
	: FTriggerContext(
		FTriggerRuntimeSource::FromStatus(InRuntimeSource),
		InActionOuter,
		InBattle,
		InPresentationRecordWriter
	)
{
}

FTriggerContext::FTriggerContext(
	const FTriggerRuntimeSource& InRuntimeSource,
	UObject* InActionOuter,
	ABattleManager* InBattle,
	const FPresentationRecordWriter& InPresentationRecordWriter
)
	: RuntimeSource(InRuntimeSource)
	, ActionOuter(InActionOuter)
	, Battle(InBattle)
	, PresentationRecordWriter(InPresentationRecordWriter)
{
}

UObject* FTriggerContext::GetRuntimeSourceObject() const
{
	return RuntimeSource.RuntimeObject;
}

UStatusInstance* FTriggerContext::GetRuntimeSource() const
{
	return RuntimeSource.Kind == ETriggerRuntimeSourceKind::Status
		? Cast<UStatusInstance>(RuntimeSource.RuntimeObject)
		: nullptr;
}

URelicInstance* FTriggerContext::GetRelicSource() const
{
	return RuntimeSource.Kind == ETriggerRuntimeSourceKind::Relic
		? Cast<URelicInstance>(RuntimeSource.RuntimeObject)
		: nullptr;
}

ETriggerRuntimeSourceKind FTriggerContext::GetSourceKind() const
{
	return RuntimeSource.Kind;
}

FName FTriggerContext::GetSourceId() const
{
	return RuntimeSource.SourceId;
}

uint64 FTriggerContext::GetRuntimeSequence() const
{
	return RuntimeSource.RuntimeSequence;
}

ACombatant* FTriggerContext::GetOwner() const
{
	return RuntimeSource.CombatantOwner;
}

UObject* FTriggerContext::GetActionOuter() const
{
	return ActionOuter;
}

ABattleManager* FTriggerContext::GetBattle() const
{
	return Battle;
}

const FPresentationRecordWriter& FTriggerContext::GetPresentationRecordWriter() const
{
	return PresentationRecordWriter;
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
