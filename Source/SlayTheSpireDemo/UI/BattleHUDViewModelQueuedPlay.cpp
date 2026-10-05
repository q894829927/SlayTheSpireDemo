#include "BattleHUDViewModel.h"
#include "../Presentation/PresentationG9Types.h"
#include "../Battle/BattleManager.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"

void UBattleHUDViewModel::ReportQueuedPlayFeedback(const FText& Text)
{
	LastFeedback = Text;
	BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
}

FGameplayRequestResult UBattleHUDViewModel::RequestQueuedCardPlay(const FQueuedCardPlayIntent& Intent)
{
	ABattleManager* Battle = BattleManager.Get();
	FPlayerTurnAuthorityToken Turn;
	FGameplayRequestResult Result = FGameplayRequestResult::Rejected(EGameplayRequestFailureReason::InvalidBattle);
	if (IsValid(Battle) && Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn) && Turn == Intent.Turn
		&& BattleId == static_cast<int64>(Turn.BattleId) && !HasAuthoritativePendingCardSelection())
	{
		if (bInputLocked || !IsLiveBindingCurrent())
			return FGameplayRequestResult::Rejected(EGameplayRequestFailureReason::ResolutionBusy);
		UCardInstance* Card = FindHandCardByRuntimeId(Intent.RuntimeId);
		ACombatant* Target = nullptr;
		if (!Intent.TargetPresentationId.IsNone())
		{
			const auto* Binding = LiveCombatantBindings.Find(Intent.TargetPresentationId);
			Target = Binding ? Binding->Get() : nullptr;
		}
		if (!IsValid(Card) || Card->GetCardId() != Intent.CardId)
			Result = FGameplayRequestResult::Rejected(EGameplayRequestFailureReason::CardNoLongerInHand);
		else if ((Intent.TargetType == ECardTargetType::None) != Intent.TargetPresentationId.IsNone()
			|| (!Intent.TargetPresentationId.IsNone() && !IsValid(Target)))
			Result = FGameplayRequestResult::Rejected(EGameplayRequestFailureReason::InvalidTarget);
		else Result = Battle->RequestPlayCard(Card, Target);
	}
	if (Result.IsAcceptedForResolution())
	{
		// Following automatic plays must leave skip feedback visible for a frame.
		ClearSelectionInternal(); ClearLiveInputBindings(); SetResolving();
		BroadcastChanged(EBattleHUDDirtyFlags::Input | EBattleHUDDirtyFlags::Combatants | EBattleHUDDirtyFlags::Feedback);
	}
	else if (Result.FailureReason != EGameplayRequestFailureReason::ResolutionBusy)
	{
		SetFeedback(Result.FailureReason);
		LastFeedback = FText::Format(NSLOCTEXT("BattleHUD", "QueuedPlaySkipped", "已跳过排队卡牌：{0}"), LastFeedback);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback);
	}
	return Result;
}
