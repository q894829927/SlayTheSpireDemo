#include "BattleBufferedPlayerIntent.h"

#include "../Battle/BattleManager.h"
#include "../Selection/SelectionResolver.h"

bool FBufferedPlayerIntentShadowState::StoreCardSelection(
	const FBufferedCardIntent& Intent
)
{
	if (!Intent.IsValid() || HasEndTurn())
	{
		return false;
	}

	CardIntent = Intent;
	EndTurnIntent = FBufferedEndTurnIntent{};
	Kind = EBufferedPlayerIntentKind::CardSelection;
	return true;
}

bool FBufferedPlayerIntentShadowState::StoreEndTurn(FBufferedEndTurnIntent Intent)
{
	if (!Intent.Turn.IsValid())
	{
		return false;
	}

	if (NextIntentGeneration == 0)
	{
		NextIntentGeneration = 1;
	}
	Intent.LocalIntentGeneration = NextIntentGeneration++;
	if (NextIntentGeneration == 0)
	{
		NextIntentGeneration = 1;
	}

	CardIntent = FBufferedCardIntent{};
	EndTurnIntent = Intent;
	Kind = EBufferedPlayerIntentKind::EndTurn;
	return true;
}

bool FBufferedPlayerIntentShadowState::TryTakeCardSelection(
	FBufferedCardIntent& OutIntent
)
{
	OutIntent = FBufferedCardIntent{};
	if (!HasCardSelection())
	{
		return false;
	}

	OutIntent = CardIntent;
	Clear();
	return true;
}

bool FBufferedPlayerIntentShadowState::TryTakeEndTurn(
	FBufferedEndTurnIntent& OutIntent
)
{
	OutIntent = FBufferedEndTurnIntent{};
	if (!HasEndTurn())
	{
		return false;
	}

	OutIntent = EndTurnIntent;
	Clear();
	return true;
}

void FBufferedPlayerIntentShadowState::Clear()
{
	Kind = EBufferedPlayerIntentKind::None;
	CardIntent = FBufferedCardIntent{};
	EndTurnIntent = FBufferedEndTurnIntent{};
}

bool TryCaptureBufferedEndTurnShadow(
	const ABattleManager* Battle,
	FBufferedEndTurnIntent& OutIntent
)
{
	OutIntent = FBufferedEndTurnIntent{};
	if (!IsValid(Battle))
	{
		return false;
	}

	FPlayerTurnAuthorityToken Turn;
	if (!Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn))
	{
		return false;
	}

	const USelectionResolver* Resolver = Battle->GetSelectionResolver();
	if (IsValid(Resolver) && Resolver->HasPendingSelection())
	{
		return false;
	}

	const FGameplayValidationResult EndTurn = Battle->QueryEndPlayerTurn();
	if (!EndTurn.bAllowed
		&& EndTurn.FailureReason != EGameplayRequestFailureReason::ResolutionBusy)
	{
		return false;
	}

	OutIntent.Turn = Turn;
	return true;
}

EBufferedIntentShadowEvaluation EvaluateBufferedEndTurnShadow(
	const ABattleManager* Battle,
	const FBufferedEndTurnIntent& Intent
)
{
	if (!IsValid(Battle) || !Intent.IsValid())
	{
		return EBufferedIntentShadowEvaluation::Stale;
	}

	FPlayerTurnAuthorityToken CurrentTurn;
	if (!Battle->TryGetCurrentPlayerTurnAuthorityToken(CurrentTurn)
		|| CurrentTurn != Intent.Turn)
	{
		return EBufferedIntentShadowEvaluation::Stale;
	}

	const USelectionResolver* Resolver = Battle->GetSelectionResolver();
	if (IsValid(Resolver) && Resolver->HasPendingSelection())
	{
		return EBufferedIntentShadowEvaluation::Stale;
	}

	const FGameplayValidationResult EndTurn = Battle->QueryEndPlayerTurn();
	if (EndTurn.bAllowed)
	{
		return EBufferedIntentShadowEvaluation::Ready;
	}

	if (EndTurn.FailureReason == EGameplayRequestFailureReason::ResolutionBusy)
	{
		FPlayerTurnAuthorityToken RecheckedTurn;
		return Battle->TryGetCurrentPlayerTurnAuthorityToken(RecheckedTurn)
			&& RecheckedTurn == Intent.Turn
			? EBufferedIntentShadowEvaluation::Waiting
			: EBufferedIntentShadowEvaluation::Stale;
	}

	return EBufferedIntentShadowEvaluation::Stale;
}
