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
	if (CounterBefore < RequiredShuffles - 1)
	{
		const int32 CounterAfter = CounterBefore + 1;
		RuntimeRelic->SetCounterFromAction(CounterAfter);
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[Relic] Sundial advanced: %d -> %d / %d"),
			CounterBefore,
			CounterAfter,
			RequiredShuffles
		);
		Finish();
		return;
	}

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

	RuntimeRelic->SetCounterFromAction(0);
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
