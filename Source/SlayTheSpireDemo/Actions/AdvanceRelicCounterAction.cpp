#include "AdvanceRelicCounterAction.h"

#include "BattleActionQueue.h"
#include "../Battle/BattleManager.h"
#include "../Relics/RelicContainer.h"
#include "../Relics/RelicInstance.h"

bool UAdvanceRelicCounterAction::Initialize(
	URelicInstance* InRelic,
	int32 InRequiredCount,
	const TArray<UBattleAction*>& InRewardActions
)
{
	UBattleActionQueue* Queue = Cast<UBattleActionQueue>(GetOuter());
	if (!IsValid(Queue)
		|| !IsValid(InRelic)
		|| InRequiredCount <= 0
		|| InRewardActions.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] AdvanceRelicCounterAction initialization rejected invalid core input."));
		return false;
	}

	TSet<UBattleAction*> Seen;
	Seen.Reserve(InRewardActions.Num());
	for (UBattleAction* RewardAction : InRewardActions)
	{
		if (!IsValid(RewardAction)
			|| RewardAction->IsFinished()
			|| RewardAction->GetOuter() != Queue
			|| Seen.Contains(RewardAction))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Relic] AdvanceRelicCounterAction initialization rejected invalid prepared reward batch."));
			return false;
		}
		Seen.Add(RewardAction);
	}

	Relic = InRelic;
	RequiredCount = InRequiredCount;
	RewardActions.Reset(InRewardActions.Num());
	for (UBattleAction* RewardAction : InRewardActions)
	{
		RewardActions.Add(RewardAction);
	}
	return true;
}

void UAdvanceRelicCounterAction::Execute(UBattleActionQueue* Queue)
{
	URelicInstance* RuntimeRelic = Relic.Get();
	ABattleManager* Battle = IsValid(RuntimeRelic) ? RuntimeRelic->GetBattle() : nullptr;
	const URelicContainer* Container = IsValid(Battle) ? Battle->GetPlayerRelicContainer() : nullptr;

	if (!IsValid(Queue)
		|| Queue != GetOuter()
		|| !IsValid(RuntimeRelic)
		|| !IsValid(Battle)
		|| !IsValid(Container)
		|| !Container->ContainsRelicInstance(RuntimeRelic)
		|| RequiredCount <= 0
		|| RewardActions.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Relic] AdvanceRelicCounterAction rejected invalid live runtime/configuration."));
		Finish();
		return;
	}

	const int32 CounterBefore = RuntimeRelic->GetCounter();
	if (CounterBefore < RequiredCount - 1)
	{
		RuntimeRelic->SetCounterFromAction(CounterBefore + 1);
		Finish();
		return;
	}

	TArray<UBattleAction*> PreparedRewards;
	PreparedRewards.Reserve(RewardActions.Num());
	for (const TObjectPtr<UBattleAction>& RewardActionPtr : RewardActions)
	{
		UBattleAction* RewardAction = RewardActionPtr.Get();
		if (IsValid(RewardAction))
		{
			RewardAction->SetPresentationRecordWriter(GetPresentationRecordWriter());
		}
		PreparedRewards.Add(RewardAction);
	}

	if (!Queue->AddBatchToFrontPreserveOrder(PreparedRewards))
	{
		UE_LOG(LogTemp, Error, TEXT("[Relic] Counter threshold reached but prepared reward batch insertion failed."));
		Queue->RequestResolutionFault(TEXT("Relic counter threshold failed to enqueue its prepared reward batch."));
		Finish();
		return;
	}

	RuntimeRelic->SetCounterFromAction(0);
	Finish();
}
