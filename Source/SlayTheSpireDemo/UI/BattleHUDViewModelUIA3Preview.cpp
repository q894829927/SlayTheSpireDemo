#include "BattleHUDViewModel.h"

#include "../Battle/BattleManager.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"

bool UBattleHUDViewModel::SetPreviewTargetById(int32 TargetId)
{
	if (SelectedCardRuntimeId == INDEX_NONE || !IsLiveBindingCurrent())
	{
		ClearImmediatePreviewInternal();
		BroadcastChanged();
		return false;
	}

	ACombatant* Target = FindLegalTargetById(TargetId);
	if (!IsValid(Target))
	{
		ClearImmediatePreviewInternal();
		BroadcastChanged();
		return false;
	}

	if (!TryBuildImmediatePreviewForTarget(Target, TargetId))
	{
		BroadcastChanged();
		return false;
	}

	BroadcastChanged();
	return true;
}

void UBattleHUDViewModel::ClearPreviewTarget()
{
	const bool bHadPreviewState = PreviewTargetId != INDEX_NONE
		|| !PreviewTargetPresentationId.IsNone()
		|| bHasImmediatePreview
		|| ImmediatePreview.BattleId != 0
		|| ImmediatePreview.StateRevision != 0;

	ClearImmediatePreviewInternal();
	if (bHadPreviewState)
	{
		BroadcastChanged();
	}
}

bool UBattleHUDViewModel::TryBuildImmediatePreviewForTarget(
	ACombatant* Target,
	int32 TargetId
)
{
	ClearImmediatePreviewInternal();

	ABattleManager* Battle = BattleManager.Get();
	UCardInstance* Card = FindHandCardByRuntimeId(SelectedCardRuntimeId);
	if (!IsValid(Battle)
		|| !IsValid(Card)
		|| !IsValid(Target)
		|| !IsLiveBindingCurrent())
	{
		return false;
	}

	const FBattleHUDTargetView* TargetView = LegalTargets.FindByPredicate(
		[TargetId](const FBattleHUDTargetView& Candidate)
		{
			return Candidate.TargetId == TargetId;
		}
	);
	if (TargetView == nullptr || TargetView->PresentationId.IsNone())
	{
		return false;
	}

	FImmediateCardPreview Preview;
	if (!Battle->TryBuildImmediateCardPreview(Card, Target, Preview))
	{
		return false;
	}

	if (Preview.BattleId != BattleId
		|| Preview.StateRevision != StateRevision
		|| Preview.CardRuntimeId != SelectedCardRuntimeId
		|| Preview.SourcePresentationId != Player.PresentationId
		|| Preview.TargetPresentationId != TargetView->PresentationId)
	{
		return false;
	}

	PreviewTargetId = TargetId;
	PreviewTargetPresentationId = TargetView->PresentationId;
	ImmediatePreview = MoveTemp(Preview);
	bHasImmediatePreview = true;
	return true;
}

void UBattleHUDViewModel::ClearImmediatePreviewInternal()
{
	PreviewTargetId = INDEX_NONE;
	PreviewTargetPresentationId = NAME_None;
	bHasImmediatePreview = false;
	ImmediatePreview = FImmediateCardPreview{};
}
