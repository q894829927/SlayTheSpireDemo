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
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "../Presentation/BattlePresentationController.h"
#include "InputCoreTypes.h"

void UBattleHUDWidget::EnsureHandInteractionSurfaces()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root) return;
	EnsureDetachedCardVFXHost();
	if (!PointerInputBackdrop)
	{
		// Below every authored control: blank viewport clicks still enter the
		// HUD preview route, while EndTurn/Selection/buttons keep their routes.
		int32 BackdropZ = 0;
		for (UWidget* Child : Root->GetAllChildren())
			if (const UCanvasPanelSlot* ChildCanvasSlot = Cast<UCanvasPanelSlot>(Child->Slot)) BackdropZ = FMath::Min(BackdropZ, ChildCanvasSlot->GetZOrder());
		PointerInputBackdrop = WidgetTree->ConstructWidget<UBorder>();
		PointerInputBackdrop->SetBrushColor(FLinearColor::Transparent);
		UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(PointerInputBackdrop);
		BackdropSlot->SetAnchors(FAnchors(0, 0, 1, 1)); BackdropSlot->SetOffsets(FMargin(0)); BackdropSlot->SetZOrder(BackdropZ - 1);
		PointerInputBackdrop->SetVisibility(ESlateVisibility::Hidden);
	}

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
	const FVector2D Pointer = UWidgetLayoutLibrary::GetMousePositionOnPlatform();
	UpdatePointerCardVisuals(Pointer);
	const bool bPending = ViewModel->HasAuthoritativePendingCardSelection();
	const int32 SelectedRuntimeId = IsBufferedPlayerInputEnabled() ? GetBufferedCardDraftRuntimeId() : ViewModel->SelectedCardRuntimeId;
	const auto InputState = IsBufferedPlayerInputEnabled() ? GetBufferedCardDraftState() : ViewModel->InteractionState;
	// The panel arranges independently of playback and Tick. Protected moving
	// cards retain their base geometry; only eligible Hand cards receive hover.
	if (FanHand)
	{
		const bool bAllowHover = (!HasTrackedPresentationPlayback() && !HasActiveNativePresentation()
			&& (!ViewModel->bInputLocked || bPending)) || bBufferedHandHoverAvailable;
		FanHand->UpdateHoverAffordance(Pointer, bPending ? INDEX_NONE : SelectedRuntimeId, bAllowHover, DeltaTime);
	}
	if (!TargetingArrow) return;
	UBattleCardWidget* Selected = nullptr;
	if (HB_Hand)
		for (UWidget* Child : HB_Hand->GetAllChildren())
				if (UBattleCardWidget* Card = Cast<UBattleCardWidget>(Child); Card && Card->GetRuntimeId() == SelectedRuntimeId) Selected = Card;
	if (!Selected || !Selected->IsVisible()
		|| !UBattleTargetingArrowWidget::ShouldShow(Selected->GetCardView(), InputState, IsBufferedPlayerInputEnabled() ? false : ViewModel->bInputLocked, bPending))
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
		FBattleHUDTargetView DraftTarget;
		bLegalEnemy = bInsideEnemy && (IsBufferedPlayerInputEnabled()
			? TryGetBufferedDraftTarget(Combatant_EnemyPresentation->CombatantView.PresentationId, DraftTarget)
			: ViewModel->LegalTargets.ContainsByPredicate([this](const FBattleHUDTargetView& Target)
		{
			return !Target.bPlayer && Target.TargetId == Combatant_EnemyPresentation->TargetId
				&& Target.PresentationId == Combatant_EnemyPresentation->CombatantView.PresentationId;
		}));
	}
	TargetingArrow->SetAim(Start, ArrowGeometry.AbsoluteToLocal(Pointer), bLegalEnemy);
	TargetingArrow->SetVisibility(ESlateVisibility::HitTestInvisible);
}

bool UBattleHUDWidget::CardFollowsPointer(const FBattleHUDCardView& Card)
{
	return Card.RuntimeId != INDEX_NONE && (Card.CardType == ECardType::Skill || Card.CardType == ECardType::Power
		|| (Card.CardType == ECardType::Attack && Card.TargetType == ECardTargetType::None));
}

const FGeometry& UBattleHUDWidget::GetCardInputRootGeometry() const
{
	return WidgetTree && WidgetTree->RootWidget ? WidgetTree->RootWidget->GetCachedGeometry()
		: OV_PlayArea ? OV_PlayArea->GetCachedGeometry() : GetCachedGeometry();
}

TOptional<FCardPlayVisualOrigin> UBattleHUDWidget::CaptureCardPlayVisualOrigin(int32 RuntimeId) const
{
	if (!HB_Hand) return {};
	const FGeometry& RootGeometry = GetCardInputRootGeometry();
	const FVector2D RootSize = RootGeometry.GetLocalSize();
	if (RootSize.X <= 0 || RootSize.Y <= 0) return {};
	for (UWidget* Child : HB_Hand->GetAllChildren())
	{
		const UBattleCardWidget* Card = Cast<UBattleCardWidget>(Child);
		if (!Card || Card->GetRuntimeId() != RuntimeId || !Card->IsVisible()) continue;
		FGeometry Geometry = Card->GetCachedGeometry();
		if (FanHand && !FanHand->GetCardVisualGeometry(Child, Geometry)) return {};
		const FVector2D Size = Geometry.GetLocalSize();
		if (Size.X <= 0 || Size.Y <= 0) return {};
		FCardPlayVisualOrigin Origin;
		const FVector2D Center = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(Size * 0.5f));
		const FVector2D X = FVector2D(RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D(Size.X, Size.Y * 0.5f)))) - Center;
		const FVector2D Y = FVector2D(RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D(Size.X * 0.5f, Size.Y)))) - Center;
		Origin.Center = Center / RootSize;
		Origin.Size = FVector2D(X.Size() * 2.0, Y.Size() * 2.0);
		Origin.Angle = FMath::RadiansToDegrees(FMath::Atan2(X.Y, X.X));
		Origin.bPointerHeld = CardFollowsPointer(Card->GetCardView());
		return Origin;
	}
	return {};
}

void UBattleHUDWidget::NativeOnCardPlayRequestStarting(int32 RuntimeId, const FQueuedCardPlayIntent* Intent)
{
	SubmittedCardVisual.Reset();
	bCardPlayRequestInFlight = true;
	if (!ViewModel) return;
	FQueuedCardPlayIntent Receipt = Intent ? *Intent : FQueuedCardPlayIntent{};
	if (!Intent)
	{
		Receipt.Turn.BattleId = ViewModel->BattleId;
		Receipt.RuntimeId = RuntimeId;
		if (const auto* Card = ViewModel->HandCards.FindByPredicate([RuntimeId](const auto& C) { return C.RuntimeId == RuntimeId; })) { Receipt.CardId = Card->CardId; Receipt.TargetType = Card->TargetType; }
		FPresentationSessionToken Session;
		if (PresentationController && PresentationController->TryGetPresentationSessionToken(Session)) Receipt.PresentationFence = Session;
	}
	if (!Receipt.VisualOrigin.IsSet()) Receipt.VisualOrigin = CaptureCardPlayVisualOrigin(RuntimeId);
	SubmittedCardVisual = Receipt;
	SubmittedCardViewModel = ViewModel;
	SubmittedCardController = PresentationController;
}

void UBattleHUDWidget::NativeOnCardPlayRequestFinished(bool bAccepted)
{
	bCardPlayRequestInFlight = false;
	if (!bAccepted) SubmittedCardVisual.Reset();
	RefreshCardInputVisuals();
}

void UBattleHUDWidget::RefreshCardInputVisuals()
{
	RetireCollidingPlayedCardVisualJobs();
	if (HandPointerGesture.IsSet() && !IsHandPointerGestureCurrent(HandPointerGesture.GetValue())) HandPointerGesture.Reset();
	if (SubmittedCardVisual.IsSet() && (!ViewModel || SubmittedCardViewModel.Get() != ViewModel.Get()
		|| SubmittedCardController.Get() != PresentationController.Get()
		|| SubmittedCardVisual->Turn.BattleId != static_cast<uint64>(ViewModel->BattleId)
		|| (SubmittedCardVisual->PresentationFence.IsSet() && (!PresentationController
			|| !PresentationController->IsCurrentPresentationSession(SubmittedCardVisual->PresentationFence.GetValue())))
		|| (!bCardPlayRequestInFlight && !ViewModel->bInputLocked && ViewModel->SelectedCardRuntimeId == INDEX_NONE))) SubmittedCardVisual.Reset();
	const int32 Selected = ViewModel && !ViewModel->HasAuthoritativePendingCardSelection()
		? (IsBufferedPlayerInputEnabled() ? GetBufferedCardDraftRuntimeId() : ViewModel->SelectedCardRuntimeId) : INDEX_NONE;
	bool bPointerDraft = false;
	TSet<int32> Visuals;
	if (ViewModel && (IsBufferedPlayerInputEnabled() || !ViewModel->bInputLocked))
		if (const auto* Card = ViewModel->HandCards.FindByPredicate([Selected](const auto& C) { return C.RuntimeId == Selected; });
			Card && CardFollowsPointer(*Card) && ViewModel->GetCardPresentationOwner(Selected) == ECardPresentationOwner::Hand)
		{ Visuals.Add(Selected); bPointerDraft = true; }
	if (IsBufferedPlayerInputEnabled())
		for (const auto& Intent : GetBufferedPlayerInput().GetConfirmedPlays())
			if (Intent.VisualOrigin.IsSet() && Intent.VisualOrigin->bPointerHeld) Visuals.Add(Intent.RuntimeId);
	if (SubmittedCardVisual.IsSet() && SubmittedCardVisual->VisualOrigin.IsSet()
		&& SubmittedCardVisual->VisualOrigin->bPointerHeld) Visuals.Add(SubmittedCardVisual->RuntimeId);
	if (FanHand) FanHand->SetInputVisualCards(Visuals);
	if (PointerInputBackdrop) PointerInputBackdrop->SetVisibility(bPointerDraft ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UBattleHUDWidget::UpdatePointerCardVisuals(const FVector2D& Pointer)
{
	RefreshCardInputVisuals();
	if (!ViewModel || !FanHand) return;
	const FGeometry& RootGeometry = GetCardInputRootGeometry();
	auto MoveReceipt = [&](const FQueuedCardPlayIntent& Intent)
	{
		if (Intent.VisualOrigin.IsSet()) FanHand->MoveInputVisualTo(Intent.RuntimeId,
			RootGeometry.LocalToAbsolute(Intent.VisualOrigin->Center * FVector2D(RootGeometry.GetLocalSize())));
	};
	if (IsBufferedPlayerInputEnabled()) for (const auto& Intent : GetBufferedPlayerInput().GetConfirmedPlays()) MoveReceipt(Intent);
	if (SubmittedCardVisual.IsSet()) MoveReceipt(SubmittedCardVisual.GetValue());
	FanHand->MoveInputVisualTo(IsBufferedPlayerInputEnabled() ? GetBufferedCardDraftRuntimeId() : ViewModel->SelectedCardRuntimeId, Pointer);
}

bool UBattleHUDWidget::ConfirmPointerCard()
{
	if (!ViewModel || ViewModel->HasAuthoritativePendingCardSelection() || (!IsBufferedPlayerInputEnabled() && ViewModel->bInputLocked)) return false;
	const int32 RuntimeId = IsBufferedPlayerInputEnabled() ? GetBufferedCardDraftRuntimeId() : ViewModel->SelectedCardRuntimeId;
	const auto* Card = ViewModel->HandCards.FindByPredicate([RuntimeId](const auto& C) { return C.RuntimeId == RuntimeId; });
	if (!Card || !CardFollowsPointer(*Card)) return false;
	if (Card->TargetType == ECardTargetType::Self) return SelectTarget(1);
	return Card->TargetType == ECardTargetType::None && ConfirmSelectedCard();
}

FReply UBattleHUDWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		HandPointerGesture.Reset();
		if (!IsExplicitPointerControlAt(Event.GetScreenSpacePosition()))
		{
			UpdatePointerCardVisuals(Event.GetScreenSpacePosition());
			if (ConfirmPointerCard()) return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}
