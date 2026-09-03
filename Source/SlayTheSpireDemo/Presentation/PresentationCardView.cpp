#include "PresentationCardView.h"

#include "PresentationTypes.h"

FBattleHUDCardView PresentationCardView::MakePresentationOnlyCardView(
	const FPresentationCardSnapshot& Snapshot)
{
	FBattleHUDCardView View;
	View.RuntimeId = Snapshot.RuntimeId;
	View.CardId = Snapshot.CardId;
	View.DisplayName = Snapshot.DisplayName;
	View.Cost = Snapshot.Cost;
	View.CardType = Snapshot.CardType;
	View.TargetType = Snapshot.TargetType;
	View.Description = Snapshot.Description;
	View.RichDescription = Snapshot.RichDescription;
	View.CardArt = Snapshot.CardArt;
	View.bGameplayPlayable = false;
	View.UnplayableReason = FText::GetEmpty();
	return View;
}
