#include "BattleManager.h"

#include "BattleReadSnapshot.h"
#include "../Actions/BattleActionQueue.h"
#include "../Combat/Combatant.h"
#include "../Modifiers/Damage/DamageModifierPipeline.h"
#include "../Modifiers/Damage/DamageSpec.h"
#include "../Relics/RelicContainer.h"
#include "../Relics/RelicData.h"
#include "../Relics/RelicInstance.h"
#include "BattleTextResolver.h"
#include "Containers/Ticker.h"

namespace
{
	void ResolveCombatantStatusDescriptions(FCombatantReadView& CombatantView)
	{
		for (FStatusReadView& StatusView : CombatantView.Statuses)
		{
			StatusView.CurrentDescription = FBattleTextResolver::ResolveStatusDescription(StatusView.Status.Get());
		}
	}

	void ResolveCardDescriptions(TArray<FCardReadView>& CardViews, ACombatant* Source)
	{
		for (FCardReadView& CardView : CardViews)
		{
			CardView.CurrentDescription = FBattleTextResolver::ResolveCardDescription(CardView.Card.Get(), Source);
			CardView.CurrentRichDescription = FBattleTextResolver::ResolveCardRichDescription(CardView.Card.Get(), Source);
		}
	}

	bool BuildRelicReadViews(
		const URelicContainer* RelicContainer,
		TArray<FRelicReadView>& OutRelics)
	{
		OutRelics.Reset();
		if (!IsValid(RelicContainer))
		{
			return true;
		}

		const TArray<TObjectPtr<URelicInstance>>& Relics = RelicContainer->GetRelics();
		OutRelics.Reserve(Relics.Num());
		TSet<FName> SeenRelicIds;

		for (const TObjectPtr<URelicInstance>& RelicPtr : Relics)
		{
			URelicInstance* Relic = RelicPtr.Get();
			URelicData* Definition = IsValid(Relic) ? Relic->GetDefinition() : nullptr;
			const FName RelicId = IsValid(Relic) ? Relic->GetRelicId() : NAME_None;
			const uint64 RuntimeSequence = IsValid(Relic) ? Relic->GetRuntimeSequence() : 0;
			const int32 Counter = IsValid(Relic) ? Relic->GetCounter() : -1;

			if (!IsValid(Relic)
				|| !IsValid(Definition)
				|| RelicId.IsNone()
				|| RuntimeSequence == 0
				|| Counter < 0
				|| SeenRelicIds.Contains(RelicId))
			{
				OutRelics.Reset();
				return false;
			}

			SeenRelicIds.Add(RelicId);

			FRelicReadView View;
			View.Relic = Relic;
			View.Definition = Definition;
			View.RelicId = RelicId;
			View.RuntimeSequence = RuntimeSequence;
			View.Counter = Counter;
			OutRelics.Add(MoveTemp(View));
		}

		OutRelics.Sort(
			[](const FRelicReadView& A, const FRelicReadView& B)
			{
				return A.RuntimeSequence < B.RuntimeSequence;
			}
		);

		for (int32 Index = 1; Index < OutRelics.Num(); ++Index)
		{
			if (OutRelics[Index - 1].RuntimeSequence >= OutRelics[Index].RuntimeSequence)
			{
				OutRelics.Reset();
				return false;
			}
		}

		return true;
	}
}

bool ABattleManager::TryBuildPlayerFacingReadSnapshot(FBattleReadSnapshot& OutSnapshot) const
{
	if (!TryBuildReadSnapshot(OutSnapshot))
	{
		return false;
	}

	if (!BuildRelicReadViews(GetPlayerRelicContainer(), OutSnapshot.Relics))
	{
		OutSnapshot = FBattleReadSnapshot{};
		return false;
	}

	OutSnapshot.EnemyIntentPlayerFacing = FEnemyIntentPlayerFacingReadView{};
	ResolveCombatantStatusDescriptions(OutSnapshot.Player);
	ResolveCombatantStatusDescriptions(OutSnapshot.Enemy);
	ResolveCardDescriptions(OutSnapshot.HandCards, Player.Get());
	ResolveCardDescriptions(OutSnapshot.DiscardCards, Player.Get());
	ResolveCardDescriptions(OutSnapshot.ExhaustCards, Player.Get());

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

	// This is the internal Gameplay-stable boundary. Freeze + Seal completes
	// synchronously here so the active builder is released before another
	// Resolution may begin. Only public delivery remains deferred.
	FinalizePresentationResolutionAtStableBoundary();
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
	// Sealed immutable Envelopes are delivered first and in ResolutionId order.
	// The current-state public edge is keyed by Gameplay revision plus the sticky
	// Presentation-availability state so an availability transition cannot be
	// suppressed merely because Gameplay itself did not mutate.
	DrainPendingPublicPresentationDeliveries();

	FBattleReadSnapshot Snapshot;
	if (!TryBuildPlayerFacingReadSnapshot(Snapshot))
	{
		return;
	}

	if (Snapshot.BattleId == LastPublishedBattleId &&
		Snapshot.StateRevision == LastPublishedReadStateRevision &&
		bPresentationAvailable == bLastPublishedPresentationAvailable)
	{
		return;
	}

	LastPublishedBattleId = Snapshot.BattleId;
	LastPublishedReadStateRevision = Snapshot.StateRevision;
	bLastPublishedPresentationAvailable = bPresentationAvailable;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Battle] ReadStateReady. BattleId=%llu Revision=%llu State=%d PresentationAvailable=%s"),
		Snapshot.BattleId,
		Snapshot.StateRevision,
		static_cast<int32>(Snapshot.BattleState),
		bPresentationAvailable ? TEXT("true") : TEXT("false")
	);

	OnReadStateReady.Broadcast(Snapshot.BattleId, Snapshot.StateRevision);
}
