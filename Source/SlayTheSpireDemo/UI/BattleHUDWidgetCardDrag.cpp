#include "BattleHUDWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "InputCoreTypes.h"
#include "../Presentation/BattlePresentationController.h"

FReply UBattleHUDWidget::HandleHandCardPointerPressed(UBattleCardWidget* Card, const FPointerEvent& Event)
{
	if (!bNativeBindingsValid || Event.GetEffectingButton() != EKeys::LeftMouseButton
		|| !ViewModel || ViewModel->HasAuthoritativePendingCardSelection() || !Card
		|| Card->GetParent() != HB_Hand.Get() || !FormalHandCards.Contains(Card)
		|| !Card->IsVisible() || !Card->GetIsEnabled()) return FReply::Unhandled();
	const int32 RuntimeId = ResolveHandCardRequest(Card->GetRuntimeId());
	const auto* View = ViewModel->HandCards.FindByPredicate([RuntimeId](const auto& C) { return C.RuntimeId == RuntimeId; });
	TSharedPtr<SWidget> Root = GetCachedWidget();
	if (!View || !CardFollowsPointer(*View) || !Root) return FReply::Unhandled();
	const TWeakObjectPtr<UBattleHUDViewModel> PressViewModel = ViewModel;
	const TWeakObjectPtr<UBattlePresentationController> PressController = PresentationController;
	const TWeakObjectPtr<UPanelWidget> PressPanel = HB_Hand;
	const uint64 PressSurface = HandSurfaceGeneration;
	// Claim the complete press before the button: waiting for the first move
	// loses fast drags through blank space before the backdrop's next Slate pass.
	if (SelectCard(RuntimeId) && PressViewModel.Get() == ViewModel.Get()
		&& PressController.Get() == PresentationController.Get() && PressPanel.Get() == HB_Hand.Get()
		&& PressSurface == HandSurfaceGeneration && FormalHandCards.Contains(Card)
		&& BeginHandCardDrag(RuntimeId, Event.GetScreenSpacePosition(), Event.GetPointerIndex(), Event.GetUserIndex()))
		return HasMouseCaptureByUser(Event.GetUserIndex(), Event.GetPointerIndex())
			? FReply::Handled() : FReply::Handled().CaptureMouse(Root.ToSharedRef());
	return FReply::Handled(); // a declined request must not retry at OnPressed
}

bool UBattleHUDWidget::IsExplicitPointerControlAt(const FVector2D& Position) const
{
	for (const UWidget* Control : {Btn_EndTurn.Get(), Btn_Cancel.Get(), Btn_Confirm.Get()})
		if (Control && Control->IsVisible() && Control->GetCachedGeometry().IsUnderLocation(Position)) return true;
	return false;
}

bool UBattleHUDWidget::IsHandPointerGestureCurrent(const FNativeHandPointerGesture& Gesture) const
{
	const UBattleCardWidget* Card = Gesture.Card.Get();
	return ViewModel && Gesture.Press.ViewModel.Get() == ViewModel.Get()
		&& Gesture.Controller.Get() == PresentationController.Get()
		&& Gesture.Panel.Get() == HB_Hand.Get() && Gesture.SurfaceGeneration == HandSurfaceGeneration
		&& ViewModel->BattleId == Gesture.BattleId && !ViewModel->HasAuthoritativePendingCardSelection()
		&& ViewModel->Outcome == EBattleHUDOutcome::None
		&& ViewModel->InteractionState != EBattleHUDInteractionState::Terminal
		&& ViewModel->InteractionState != EBattleHUDInteractionState::PresentationUnavailable
		&& (Gesture.bBuffered || !ViewModel->bInputLocked)
		&& Gesture.bBuffered == IsBufferedPlayerInputEnabled()
		&& Gesture.bDisplayOwned == ViewModel->IsPresentationDisplayOwned()
		&& (!Gesture.bBuffered || Gesture.InputGeneration == GetBufferedPlayerInput().GetBindingGeneration())
		&& (Gesture.bBuffered ? GetBufferedCardDraftRuntimeId() : ViewModel->SelectedCardRuntimeId) == Gesture.RuntimeId
		&& Card && Card->GetParent() == HB_Hand.Get() && Card->IsVisible() && Card->GetIsEnabled()
		&& Card->GetRuntimeId() == Gesture.RuntimeId && Card->GetCardId() == Gesture.CardId
		&& CardFollowsPointer(Card->GetCardView())
		&& ViewModel->GetCardPresentationOwner(Gesture.RuntimeId) == ECardPresentationOwner::Hand
		&& (!Gesture.Session.IsSet() || (PresentationController
			&& PresentationController->IsCurrentPresentationSession(Gesture.Session.GetValue())));
}

bool UBattleHUDWidget::BeginHandCardDrag(int32 RuntimeId, const FVector2D& Position, uint32 PointerIndex, int32 UserIndex)
{
	HandPointerGesture.Reset();
	if (!ViewModel || !HB_Hand) return false;
	FNativeHandPointerGesture Gesture;
	Gesture.Press.Position = Position; Gesture.Press.PointerIndex = PointerIndex; Gesture.Press.UserIndex = UserIndex;
	Gesture.Press.ViewModel = ViewModel; Gesture.Controller = PresentationController; Gesture.Panel = HB_Hand;
	Gesture.BattleId = ViewModel->BattleId; Gesture.SurfaceGeneration = HandSurfaceGeneration;
	Gesture.RuntimeId = RuntimeId; Gesture.bBuffered = IsBufferedPlayerInputEnabled();
	Gesture.bDisplayOwned = ViewModel->IsPresentationDisplayOwned();
	Gesture.InputGeneration = GetBufferedPlayerInput().GetBindingGeneration();
	for (UWidget* Child : HB_Hand->GetAllChildren())
		if (UBattleCardWidget* Card = Cast<UBattleCardWidget>(Child); Card && Card->GetRuntimeId() == RuntimeId)
		{ Gesture.Card = Card; Gesture.CardId = Card->GetCardId(); break; }
	if (Gesture.bDisplayOwned)
	{
		FPresentationSessionToken Session;
		if (!PresentationController || !PresentationController->TryGetPresentationSessionToken(Session)) return false;
		Gesture.Session = Session;
	}
	if (!IsHandPointerGestureCurrent(Gesture)) return false;
	HandPointerGesture = Gesture;
	return true;
}

bool UBattleHUDWidget::UpdateHandCardDrag(const FVector2D& Position, bool bLeftDown, uint32 PointerIndex, int32 UserIndex)
{
	if (!HandPointerGesture.IsSet()) return false;
	if (HandPointerGesture->Press.PointerIndex != PointerIndex || HandPointerGesture->Press.UserIndex != UserIndex) return false;
	if (!bLeftDown || !IsHandPointerGestureCurrent(HandPointerGesture.GetValue()))
	{ HandPointerGesture.Reset(); return false; }
	// Eight HUD logical pixels distinguish dragging from short-click jitter;
	// holding duration and frame rate do not decide submission.
	const FGeometry& Root = GetCardInputRootGeometry();
	if ((Root.AbsoluteToLocal(Position) - Root.AbsoluteToLocal(HandPointerGesture->Press.Position)).SizeSquared() >= 64.0)
		HandPointerGesture->bDragged = true;
	if (HandPointerGesture->bDragged) UpdatePointerCardVisuals(Position);
	return HandPointerGesture.IsSet() && HandPointerGesture->bDragged;
}

bool UBattleHUDWidget::ReleaseHandCardDrag(const FVector2D& Position, uint32 PointerIndex, int32 UserIndex)
{
	if (!HandPointerGesture.IsSet() || HandPointerGesture->Press.PointerIndex != PointerIndex
		|| HandPointerGesture->Press.UserIndex != UserIndex) return false;
	const FNativeHandPointerGesture Gesture = HandPointerGesture.GetValue();
	HandPointerGesture.Reset(); // consume before a normal Request may publish/re-enter
	if (!Gesture.bDragged) return false;
	if (!IsHandPointerGestureCurrent(Gesture)) return true;
	UpdatePointerCardVisuals(Position);
	if (!IsHandPointerGestureCurrent(Gesture)) return true;
	const FGeometry& HandGeometry = HB_Hand->GetCachedGeometry();
	if (HandGeometry.GetLocalSize().X <= 0 || HandGeometry.GetLocalSize().Y <= 0
		|| HandGeometry.IsUnderLocation(Position) || IsExplicitPointerControlAt(Position)) return true;
	// Self/None use the same confirmation as a fresh left click. Enemy-target
	// Skills retain the exact legal-target surface; single-target Attacks never drag.
	if (Gesture.Card->GetCardView().TargetType == ECardTargetType::Enemy)
	{
		if (Combatant_EnemyPresentation && Combatant_EnemyPresentation->IsVisible()
			&& Combatant_EnemyPresentation->bLegalTarget
			&& Combatant_EnemyPresentation->GetCachedGeometry().IsUnderLocation(Position))
			SelectTarget(Combatant_EnemyPresentation->TargetId);
	}
	else ConfirmPointerCard();
	return true;
}

FReply UBattleHUDWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const bool bWasDragging = HandPointerGesture.IsSet() && HandPointerGesture->bDragged;
	if (UpdateHandCardDrag(Event.GetScreenSpacePosition(), Event.IsMouseButtonDown(EKeys::LeftMouseButton), Event.GetPointerIndex(), Event.GetUserIndex()))
	{
		// Capture belongs to the original press. Re-capturing here would release
		// the old captor, firing CaptureLost and retiring our own gesture.
		return FReply::Handled();
	}
	else if (!HandPointerGesture.IsSet() && (bWasDragging || HasMouseCaptureByUser(Event.GetUserIndex(), Event.GetPointerIndex())))
		return FReply::Handled().ReleaseMouseCapture();
	return Super::NativeOnMouseMove(Geometry, Event);
}

FReply UBattleHUDWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const bool bCaptured = HasMouseCaptureByUser(Event.GetUserIndex(), Event.GetPointerIndex());
		const bool bHandled = ReleaseHandCardDrag(Event.GetScreenSpacePosition(), Event.GetPointerIndex(), Event.GetUserIndex());
		// Cancellation/forced choice may have retired the gesture before this up.
		// Still release our capture, consuming the old up without a new request.
		if (bHandled || bCaptured) return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(Geometry, Event);
}

void UBattleHUDWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	HandPointerGesture.Reset(); // focus/capture loss never submits a command
	Super::NativeOnMouseCaptureLost(Event);
}
