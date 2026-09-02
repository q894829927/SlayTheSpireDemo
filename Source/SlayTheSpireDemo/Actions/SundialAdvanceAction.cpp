#include "SundialAdvanceAction.h"

#include "BattleActionQueue.h"
#include "GainEnergyAction.h"
#include "../Battle/BattleManager.h"
#include "../Relics/RelicContainer.h"
#include "../Relics/RelicInstance.h"

void USundialAdvanceAction::Initialize(
	URelicInstance* InRelic,
	int32 InRequiredShuffles,
	int32 InEnergyGain
)
{
	Relic = InRelic;
	RequiredShuffles = InRequiredShuffles;
	EnergyGain = InEnergyGain;
}

void USundialAdvanceAction::Execute(UBattleActionQueue* Queue)
{
	URelicInstance* RuntimeRelic = Relic.Get();
	ABattleManager* Battle = IsValid(RuntimeRelic) ? RuntimeRelic->GetBattle() : nullptr;
	const URelicContainer* Container = IsValid(Battle) ? Battle->GetPlayerRelicContainer() : nullptr;

	if (!IsValid(Queue)
		|| !IsValid(RuntimeRelic)
		|| !IsValid(Battle)
		|| !IsValid(Container)
		|| !Container->ContainsRelicInstance(RuntimeRelic)
		|| RequiredShuffles <= 0
		|| EnergyGain <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] SundialAdvanceAction rejected invalid live runtime/configuration."));
		Finish();
		return;
	}

	const int32 CounterBefore = RuntimeRelic->GetCounter();
	const int32 CounterAfterIncrement = CounterBefore + 1;
	if (CounterAfterIncrement < RequiredShuffles)
	{
		RuntimeRelic->SetCounterFromAction(CounterAfterIncrement);
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Relic] Sundial advanced: %d -> %d / %d"),
			CounterBefore,
			CounterAfterIncrement,
			RequiredShuffles
		);
		Finish();
		return;
	}

	RuntimeRelic->SetCounterFromAction(0);

	UGainEnergyAction* GainAction = NewObject<UGainEnergyAction>(Queue);
	GainAction->Initialize(Battle, EnergyGain);
	GainAction->SetPresentationRecordWriter(GetPresentationRecordWriter());
	if (!Queue->AddToFront(GainAction))
	{
		UE_LOG(LogTemp, Error, TEXT("[Relic] Sundial reached threshold but failed to enqueue GainEnergyAction."));
		Queue->RequestResolutionFault(TEXT("Sundial failed to enqueue its dependent GainEnergyAction."));
		Finish();
		return;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[Relic] Sundial threshold reached: %d -> 0 / %d, EnergyGain=%d queued."),
		CounterBefore,
		RequiredShuffles,
		EnergyGain
	);
	Finish();
}
