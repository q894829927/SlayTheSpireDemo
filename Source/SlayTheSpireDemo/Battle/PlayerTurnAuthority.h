#pragma once

#include "CoreMinimal.h"

// Gameplay identity, independent of display revisions and Presentation sessions.
struct SLAYTHESPIREDEMO_API FPlayerTurnAuthorityToken
{
	uint64 BattleId = 0;
	uint64 PlayerTurnSerial = 0;

	bool IsValid() const { return BattleId != 0 && PlayerTurnSerial != 0; }
	bool operator==(const FPlayerTurnAuthorityToken& Other) const
	{
		return BattleId == Other.BattleId && PlayerTurnSerial == Other.PlayerTurnSerial;
	}
	bool operator!=(const FPlayerTurnAuthorityToken& Other) const { return !(*this == Other); }
};
