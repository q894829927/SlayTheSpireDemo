#include "BattleHUDViewModel.h"

#include "../Battle/BattleManager.h"
#include "../Cards/CardInstance.h"
#include "../Combat/Combatant.h"

bool UBattleHUDViewModel::SetPreviewTargetById(int32 TargetId)
{
	if (SelectedCardRuntimeId == INDEX_NONE || !IsLiveBindingCurrent())
	{
		ClearImmediatePreviewInternal();
		BroadcastPreviewChanged();
		return false;
	}

	ACombatant* Target = FindLegalTargetById(TargetId);
	if (!IsValid(Target))
	{
		ClearImmediatePreviewInternal();
		BroadcastPreviewChanged();
		return false;
	}

	if (!TryBuildImmediatePreviewForTarget(Target, TargetId))
	{
		BroadcastPreviewChanged();
		return false;
	}

	// Preview is transient card-face presentation only. Never route target hover
	// through the structural OnChanged channel: Native HUD rebuilds HB_Hand on
	// that channel, which destroys the historical card Widget/geometry A2 needs
	// as the CardPlayed animation start anchor.
	BroadcastPreviewChanged();
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
		BroadcastPreviewChanged();
	}
}

FText UBattleHUDViewModel::GetImmediatePreviewDisplayText() const
{
	// A3 target-specific values now render only on the selected card face. Keep
	// this historical API fail-closed so an older hot-reloaded standalone Preview
	// TextBlock cannot resurrect the removed Damage/Block/Energy overlay surface.
	return FText::GetEmpty();
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

void UBattleHUDViewModel::BroadcastPreviewChanged()
{
	OnPreviewChanged.Broadcast();
}
