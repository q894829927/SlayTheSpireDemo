#include "BattleHUDWidget.h"

#include "BattleCardWidget.h"
#include "BattleHandFanPanel.h"
#include "BattleTargetingArrowWidget.h"
#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "BattleHUDViewModel.h"
#include "BattleHUDWidgetG9BInput.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

void UBattleHUDWidget::EnsureHandInteractionSurfaces()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root) return;

	// G8-A host creation is infrastructure-only. No production Damage path uses
	// it yet, but creating it alongside the other runtime Canvas surfaces makes
	// geometry ownership deterministic before a later detached prepare.
	EnsureTransientVFXHost();

	if (!HB_Hand || FanHand) return;
	// The production asset keeps its BindWidget name. Replace only its empty
	// layout host at initialization, before any formal/runtime card is attached.
	if (HB_Hand->GetChildrenCount() != 0 || HB_Hand->GetParent() != Root) return;
	UCanvasPanelSlot* OldSlot = Cast<UCanvasPanelSlot>(HB_Hand->Slot);
	if (!OldSlot) return;
	const int32 HandZ = OldSlot->GetZOrder();
	FanHand = WidgetTree->ConstructWidget<UBattleHandFanPanel>(UBattleHandFanPanel::StaticClass(), TEXT("FanHand"));
	UCanvasPanelSlot* HandSlot = Root->AddChildToCanvas(FanHand);
	HandSlot->SetAnchors(FAnchors(0.12f, 1.0f, 0.88f, 1.0f));
	const float HandMoveUp = FMath::Max(HandMoveUpPixels, 0.0f);
	HandSlot->SetOffsets(FMargin(0.0f, -280.0f - HandMoveUp, 0.0f, 280.0f - HandMoveUp));
	HandSlot->SetZOrder(HandZ);
	FanHand->SetLayoutParameters(
		HandCardSize,
		HandFanMaxHorizontalStep,
		HandFanBaseVerticalOffset,
		HandFanEdgeVerticalDrop);
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
	// Detached cosmetics share the existing NativeTick owner but are completely
	// independent from Hand/input readiness. Run them even when ViewModel input is
	// unavailable so finite lifetime cleanup cannot be skipped by early returns.
	UpdateDetachedDamageNumbers(DeltaTime);

	if (!IsValid(ViewModel))
	{
		if (TargetingArrow) TargetingArrow->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// Idempotent G9-B binding. After the first valid ViewModel/Button pair this is
	// only an identity check; EndTurn readiness updates are driven by ViewModel
	// change events, not Gameplay polling from NativeTick.
	BattleHUDWidgetG9BInput::EnsureWidgetInputBinding(this);

	const FVector2D Pointer = UWidgetLayoutLibrary::GetMousePositionOnPlatform();
	const bool bPending = ViewModel->HasAuthoritativePendingCardSelection();

	const bool bCanUpdateStructuralHand = FanHand
		&& !HasTrackedPresentationPlayback()
		&& !HasActiveNativePresentation()
		&& (!ViewModel->bInputLocked || bPending);
	if (bCanUpdateStructuralHand)
	{
		FanHand->UpdateInteraction(
			Pointer,
			bPending ? INDEX_NONE : ViewModel->SelectedCardRuntimeId,
			true,
			DeltaTime);
	}
	else if (BattleHUDWidgetG9BInput::IsEnabled()
		&& FanHand
		&& !bPending
		&& HasTrackedPresentationPlayback()
		&& HasActiveNativePresentation())
	{
		// G9-B separates hover affordance from formal Hand layout. Only the exact
		// played-card windows approved for buffered selection may animate surviving
		// Hand cards while the structural source/arrival geometry remains frozen.
		const bool bCardPlayedWindow =
			GetActiveNativePresentationType() == EBattlePresentationRecordType::CardPlayed
			&& GetActiveNativeCardPresentationKind() == ENativeCardPresentationKind::CardPlayed;
		const bool bPlayAreaDestinationWindow =
			GetActiveNativePresentationType() == EBattlePresentationRecordType::CardZoneChanged
			&& GetActiveNativeCardPresentationKind() == ENativeCardPresentationKind::PlayAreaToDestination;
		if (bCardPlayedWindow || bPlayAreaDestinationWindow)
		{
			FanHand->UpdateHoverAffordance(Pointer, INDEX_NONE, true, DeltaTime);
		}
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
