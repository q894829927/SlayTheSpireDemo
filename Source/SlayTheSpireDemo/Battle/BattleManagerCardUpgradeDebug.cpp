#include "BattleManager.h"

#include "../Actions/BattleActionQueue.h"
#include "../Actions/UpgradeCardAction.h"
#include "../Cards/CardInstance.h"
#include "../Deck/DeckRuntime.h"

void ABattleManager::TestUpgradeFirstHandCard()
{
	if (!HasValidActionQueue() || !HasValidDeckRuntime())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestUpgradeFirstHandCard requires an initialized ActionQueue and DeckRuntime."));
		return;
	}

	if (BattleState != EBattleState::PlayerTurn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestUpgradeFirstHandCard rejected: it is not the player's turn."));
		return;
	}

	if (IsActionQueueBusy())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestUpgradeFirstHandCard rejected: action queue is busy."));
		return;
	}

	UCardInstance* Card = DeckRuntime->GetFirstHandCard();
	if (!IsValid(Card))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Battle] TestUpgradeFirstHandCard requires at least one valid card in Hand."));
		return;
	}

	if (!Card->CanUpgrade())
	{
		UE_LOG(LogTemp, Log, TEXT("[Battle] TestUpgradeFirstHandCard skipped: %s is already upgraded or invalid."), *Card->GetDebugLabel());
		return;
	}

	BeginPresentationResolution(EPresentationResolutionOrigin::System);

	UUpgradeCardAction* UpgradeAction = NewObject<UUpgradeCardAction>(ActionQueue.Get());
	UpgradeAction->Initialize(Card);
	if (!ActionQueue->AddToBack(UpgradeAction))
	{
		UE_LOG(LogTemp, Error, TEXT("[Battle] TestUpgradeFirstHandCard failed to enqueue UpgradeCardAction for %s."), *Card->GetDebugLabel());
		AbortPresentationResolution();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Battle] TestUpgradeFirstHandCard queued authoritative upgrade for %s."), *Card->GetDebugLabel());
	if (!ActionQueue->StartProcessing())
	{
		ActionQueue->RequestResolutionFault(TEXT("Debug UpgradeCardAction was enqueued but ActionQueue could not start processing."));
	}
}
