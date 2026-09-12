#include "BattleManager.h"

bool ABattleManager::TryGetCurrentPlayerTurnAuthorityToken(
	FPlayerTurnAuthorityToken& OutToken
) const
{
	OutToken = FPlayerTurnAuthorityToken{};

	if (BattleState != EBattleState::PlayerTurn
		|| BattleId == 0
		|| PlayerTurnSerial == 0)
	{
		return false;
	}

	OutToken.BattleId = BattleId;
	OutToken.PlayerTurnSerial = PlayerTurnSerial;
	return OutToken.IsValid();
}
