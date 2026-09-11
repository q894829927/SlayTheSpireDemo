#include "PresentationDamageReducer.h"

#include "PresentationTypes.h"

namespace
{
	FBattleHUDCombatantView* ResolveExactlyOneCombatant(
		FPresentationStateSnapshot& Snapshot,
		FName PresentationId)
	{
		if (PresentationId.IsNone())
		{
			return nullptr;
		}

		const bool bPlayer = Snapshot.Player.PresentationId == PresentationId;
		const bool bEnemy = Snapshot.Enemy.PresentationId == PresentationId;
		if (bPlayer == bEnemy)
		{
			return nullptr;
		}
		return bPlayer ? &Snapshot.Player : &Snapshot.Enemy;
	}

	bool IsOptionalKnownParticipant(
		const FPresentationStateSnapshot& Snapshot,
		FName PresentationId)
	{
		if (PresentationId.IsNone())
		{
			return true;
		}
		const bool bPlayer = Snapshot.Player.PresentationId == PresentationId;
		const bool bEnemy = Snapshot.Enemy.PresentationId == PresentationId;
		return bPlayer != bEnemy;
	}
}

bool PresentationDamageReducer::TryApplyDamageRecord(
	FPresentationStateSnapshot& Snapshot,
	const FDamagePresentationPayload& Damage)
{
	FBattleHUDCombatantView* Target = ResolveExactlyOneCombatant(
		Snapshot,
		Damage.TargetPresentationId);
	if (Target == nullptr
		|| !IsOptionalKnownParticipant(Snapshot, Damage.SourcePresentationId)
		|| (Damage.DamageKind != EDamageKind::Attack
			&& Damage.DamageKind != EDamageKind::Effect)
		|| Target->MaxHP <= 0
		|| Damage.IncomingDamage <= 0
		|| Damage.HPBefore <= 0
		|| Damage.HPBefore > Target->MaxHP
		|| Damage.BlockBefore < 0
		|| Damage.HPBefore != Target->HP
		|| Damage.BlockBefore != Target->Block
		|| Damage.HPAfter < 0
		|| Damage.HPAfter > Damage.HPBefore
		|| Damage.BlockAfter < 0
		|| Damage.BlockAfter > Damage.BlockBefore
		|| Damage.BlockedDamage < 0
		|| Damage.HPDamage < 0
		|| Damage.BlockedDamage != Damage.BlockBefore - Damage.BlockAfter
		|| Damage.HPDamage != Damage.HPBefore - Damage.HPAfter)
	{
		return false;
	}

	const int64 AccountedDamage = static_cast<int64>(Damage.BlockedDamage)
		+ static_cast<int64>(Damage.HPDamage);
	if (AccountedDamage <= 0
		|| AccountedDamage > static_cast<int64>(Damage.IncomingDamage))
	{
		return false;
	}

	// All validation is complete before mutation so failure is side-effect free.
	Target->HP = Damage.HPAfter;
	Target->Block = Damage.BlockAfter;
	Target->bDead = Damage.HPAfter <= 0;
	return true;
}
