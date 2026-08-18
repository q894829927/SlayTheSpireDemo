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

void ABattleManager::HandleActionQueueResolutionIdle()
{
	if (!IsValid(ActionQueue.Get()))
	{
		return;
	}

	if (ActionQueue->IsResolutionFaulted() || ActionQueue->IsBusy())
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
	ReadStateReadyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ABattleManager::HandleScheduledReadStateReady),
		0.0f
	);
}

bool ABattleManager::HandleScheduledReadStateReady(float /*DeltaTime*/)
{
	bReadStateReadyPublishScheduled = false;
	ReadStateReadyTickerHandle.Reset();
	TryPublishReadStateReady();
	return false;
}

#if WITH_DEV_AUTOMATION_TESTS
void ABattleManager::FlushScheduledReadStateReadyForTesting()
{
	if (!bReadStateReadyPublishScheduled)
	{
		return;
	}

	if (ReadStateReadyTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReadStateReadyTickerHandle);
		ReadStateReadyTickerHandle.Reset();
	}

	HandleScheduledReadStateReady(0.0f);
}
#endif

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
