#include "BattleHUDWidget.h"

#include "BattleCardWidget.h"
#include "BattleHandFanPanel.h"
#include "BattleTargetingArrowWidget.h"
#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "BattleHUDViewModel.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

void UBattleHUDWidget::EnsureHandInteractionSurfaces()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root || !HB_Hand || FanHand) return;
	// The production asset keeps its BindWidget name. Replace only its empty
	// layout host at initialization, before any formal/runtime card is attached.
	if (HB_Hand->GetChildrenCount() != 0 || HB_Hand->GetParent() != Root) return;
	UCanvasPanelSlot* OldSlot = Cast<UCanvasPanelSlot>(HB_Hand->Slot);
	if (!OldSlot) return;
	const int32 HandZ = OldSlot->GetZOrder();
	FanHand = WidgetTree->ConstructWidget<UBattleHandFanPanel>(UBattleHandFanPanel::StaticClass(), TEXT("FanHand"));
	UCanvasPanelSlot* HandSlot = Root->AddChildToCanvas(FanHand);
	HandSlot->SetAnchors(FAnchors(0.12f, 1.0f, 0.88f, 1.0f));
	HandSlot->SetOffsets(FMargin(0.0f, -280.0f, 0.0f, 280.0f));
	HandSlot->SetZOrder(HandZ);
	FanHand->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	HB_Hand->RemoveFromParent();
	HB_Hand = FanHand;
	TargetingArrow = CreateWidget<UBattleTargetingArrowWidget>(this);
	if (TargetingArrow)
	{
		UCanvasPanelSlot* ArrowSlot = Root->AddChildToCanvas(TargetingArrow);
		ArrowSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		ArrowSlot->SetOffsets(FMargin(0.0f));
		ArrowSlot->SetZOrder(HandZ + 1);
		TargetingArrow->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UBattleHUDWidget::UpdateHandInteraction(float DeltaTime)
{
	if (!IsValid(ViewModel))
	{
		if (TargetingArrow) TargetingArrow->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	const FVector2D Pointer = UWidgetLayoutLibrary::GetMousePositionOnPlatform();
	const bool bPending = ViewModel->HasAuthoritativePendingCardSelection();
	// Playback keeps its exact source/arrival transform. Fan updates resume only
	// once the reducer has reconciled the structural Hand slots.
	if (FanHand && !HasTrackedPresentationPlayback() && !HasActiveNativePresentation()
		&& (!ViewModel->bInputLocked || bPending))
	{
		FanHand->UpdateInteraction(Pointer, bPending ? INDEX_NONE : ViewModel->SelectedCardRuntimeId, true, DeltaTime);
	}
	if (!TargetingArrow) return;
	UBattleCardWidget* Selected = nullptr;
	if (HB_Hand)
		for (UWidget* Child : HB_Hand->GetAllChildren())
			if (UBattleCardWidget* Card = Cast<UBattleCardWidget>(Child); Card && Card->GetRuntimeId() == ViewModel->SelectedCardRuntimeId) Selected = Card;
	if (!Selected || !Selected->IsVisible()
		|| !UBattleTargetingArrowWidget::ShouldShow(Selected->GetCardView(), ViewModel->InteractionState, ViewModel->bInputLocked, bPending))
	{
		TargetingArrow->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	// Arrow fills the root Canvas exactly. Use that persistent visible geometry:
	// a Hidden/Collapsed child may never have received its first Slate tick.
	const FGeometry& ArrowGeometry = WidgetTree->RootWidget->GetCachedGeometry();
	const FGeometry& CardGeometry = Selected->GetCachedGeometry();
	if (ArrowGeometry.GetLocalSize().IsNearlyZero() || CardGeometry.GetLocalSize().IsNearlyZero())
	{
		TargetingArrow->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	const FVector2D Start = ArrowGeometry.AbsoluteToLocal(CardGeometry.LocalToAbsolute(CardGeometry.GetLocalSize() * FVector2D(0.5f, 0.25f)));
	bool bLegalEnemy = false;
	if (Combatant_EnemyPresentation && Combatant_EnemyPresentation->IsVisible()
		&& Combatant_EnemyPresentation->bLegalTarget && Combatant_EnemyPresentation->TargetId != INDEX_NONE)
	{
		const FGeometry& EnemyGeometry = Combatant_EnemyPresentation->GetCachedGeometry();
		const FVector2D EnemyLocal = EnemyGeometry.AbsoluteToLocal(Pointer);
		const bool bInsideEnemy = EnemyLocal.X >= 0.0 && EnemyLocal.Y >= 0.0
			&& EnemyLocal.X <= EnemyGeometry.GetLocalSize().X && EnemyLocal.Y <= EnemyGeometry.GetLocalSize().Y;
		bLegalEnemy = bInsideEnemy && ViewModel->LegalTargets.ContainsByPredicate([this](const FBattleHUDTargetView& Target)
		{
			return !Target.bPlayer && Target.TargetId == Combatant_EnemyPresentation->TargetId
				&& Target.PresentationId == Combatant_EnemyPresentation->CombatantView.PresentationId;
		});
	}
	TargetingArrow->SetAim(Start, ArrowGeometry.AbsoluteToLocal(Pointer), bLegalEnemy);
	TargetingArrow->SetVisibility(ESlateVisibility::HitTestInvisible);
}
