#include "BattleManager.h"

#include "../Relics/RelicContainer.h"
#include "../Relics/RelicData.h"

URelicContainer* ABattleManager::GetPlayerRelicContainer()
{
	if (!IsValid(PlayerRelicContainer.Get()))
	{
		PlayerRelicContainer = NewObject<URelicContainer>(this);
		bPlayerRelicContainerInitializedForBattle = false;
	}

	if (!IsValid(PlayerRelicContainer.Get()))
	{
		return nullptr;
	}

	if (!bPlayerRelicContainerInitializedForBattle || PlayerRelicContainerBattleId != BattleId)
	{
		PlayerRelicContainer->Initialize(this);

		for (const TObjectPtr<URelicData>& Definition : DebugStartingRelics)
		{
			const FRelicAddResult Result = PlayerRelicContainer->AddRelic(Definition.Get());
			if (Result.Outcome == ERelicAddOutcome::Invalid)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Relic] Ignored invalid DebugStartingRelics entry."));
			}
		}

		PlayerRelicContainerBattleId = BattleId;
		bPlayerRelicContainerInitializedForBattle = true;
	}

	return PlayerRelicContainer.Get();
}
