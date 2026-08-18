#include "BattleManager.h"

#include "BattleReadSnapshot.h"
#include "../Actions/BattleActionQueue.h"
#include "../Combat/Combatant.h"
#include "../Modifiers/Damage/DamageModifierPipeline.h"
#include "../Modifiers/Damage/DamageSpec.h"
#include "Containers/Ticker.h"

bool ABattleManager::TryBuildPlayerFacingReadSnapshot(FBattleReadSnapshot& OutSnapshot) const
{
	if (!TryBuildReadSnapshot(OutSnapshot))
	{
		return false;
	}

	OutSnapshot.EnemyIntentPlayerFacing = FEnemyIntentPlayerFacingReadView{};

	if (OutSnapshot.EnemyIntent.Type != EEnemyIntentType::Attack ||
		!IsValid(Enemy.Get()) || !IsValid(Player.Get()))
	{
		return true;
	}

	FDamageSpec Spec;
	Spec.Source = Enemy.Get();
	Spec.Target = Player.Get();
	Spec.DamageKind = EDamageKind::Attack;
	Spec.BaseAmount = OutSnapshot.EnemyIntent.BaseAmount;
	FDamageModifierPipeline::Resolve(Spec);

	OutSnapshot.EnemyIntentPlayerFacing.bHasCurrentResolvedDamageAmount = true;
	OutSnapshot.EnemyIntentPlayerFacing.CurrentResolvedDamageAmount = Spec.ResolvedAmount;
	return true;
}

void ABattleManager::NotifyActionQueueResolutionIdle(UBattleActionQueue* SettledQueue)
{
	if (SettledQueue != ActionQueue.Get() || !IsValid(SettledQueue))
	{
		return;
	}

	if (SettledQueue->IsResolutionFaulted() || SettledQueue->IsBusy())
	{
		return;
	}

	ScheduleReadStateReadyPublish();
}

void ABattleManager::NotifyActionQueueResolutionFaultSettled(UBattleActionQueue* FaultedQueue)
{
	if (FaultedQueue != ActionQueue.Get() || !IsValid(FaultedQueue))
	{
		return;
	}

	if (!FaultedQueue->IsResolutionFaulted() || BattleState != EBattleState::ResolutionFaulted)
	{
		return;
	}

	ScheduleReadStateReadyPublish();
}

void ABattleManager::ScheduleReadStateReadyPublish()
{
	if (bReadStateReadyPublishScheduled)
	{
		return;
	}

	bReadStateReadyPublishScheduled = true;
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ABattleManager::HandleScheduledReadStateReady),
		0.0f
	);
}

bool ABattleManager::HandleScheduledReadStateReady(float /*DeltaTime*/)
{
	bReadStateReadyPublishScheduled = false;
	TryPublishReadStateReady();
	return false;
}

void ABattleManager::TryPublishReadStateReady()
{
	FBattleReadSnapshot Snapshot;
	if (!TryBuildPlayerFacingReadSnapshot(Snapshot))
	{
		return;
	}

	if (Snapshot.BattleId == LastPublishedBattleId &&
		Snapshot.StateRevision == LastPublishedReadStateRevision)
	{
		return;
	}

	LastPublishedBattleId = Snapshot.BattleId;
	LastPublishedReadStateRevision = Snapshot.StateRevision;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] ReadStateReady. BattleId=%llu Revision=%llu State=%d"),
		Snapshot.BattleId,
		Snapshot.StateRevision,
		static_cast<int32>(Snapshot.BattleState)
	);

	OnReadStateReady.Broadcast(Snapshot.BattleId, Snapshot.StateRevision);
}
