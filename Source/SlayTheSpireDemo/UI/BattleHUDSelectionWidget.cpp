#include "BattleHUDSelectionWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "../Battle/BattleSelectionRequest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UBattleHUDSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureSelectionAreaHost();
	if (IsValid(Btn_Confirm))
	{
		Btn_Confirm->OnClicked.RemoveDynamic(this, &UBattleHUDSelectionWidget::HandleConfirmClicked);
		Btn_Confirm->OnClicked.AddUniqueDynamic(this, &UBattleHUDSelectionWidget::HandleSelectionAwareConfirmClicked);
	}
	if (IsValid(Btn_Cancel))
	{
		Btn_Cancel->OnClicked.RemoveDynamic(this, &UBattleHUDSelectionWidget::HandleCancelClicked);
		Btn_Cancel->OnClicked.AddUniqueDynamic(this, &UBattleHUDSelectionWidget::HandleSelectionAwareCancelClicked);
	}
	bSelectionAwareDelegatesBound = true;
	RefreshSharedSelectionPresentation();
}

void UBattleHUDSelectionWidget::NativeDestruct()
{
	// Base releases exact active transitions; it must still have a live Host.
	Super::NativeDestruct();
	if (UBattleHUDViewModel* Bound = SelectionBoundViewModel.Get())
	{
		if (Bound->CancelCardPresentationSelectionLifecycle(PendingGeneration)) Bound->ClearPendingCardSelectionInputState();
	}
	ReleaseSelectionAreaVisuals();
	SelectionBoundViewModel.Reset();
	PendingGeneration = PendingBattleId = PendingBoundaryRevision = 0;
	SetSelectionLayoutActive(false);
	if (bSelectionAwareDelegatesBound)
	{
		if (IsValid(Btn_Confirm)) Btn_Confirm->OnClicked.RemoveDynamic(this, &UBattleHUDSelectionWidget::HandleSelectionAwareConfirmClicked);
		if (IsValid(Btn_Cancel)) Btn_Cancel->OnClicked.RemoveDynamic(this, &UBattleHUDSelectionWidget::HandleSelectionAwareCancelClicked);
		bSelectionAwareDelegatesBound = false;
	}
	if (IsValid(SelectionAreaHost))
	{
		SelectionAreaHost->RemoveFromParent();
		SelectionAreaHost = nullptr;
	}
}

void UBattleHUDSelectionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdatePendingSelectionAreaLayout();
}

bool UBattleHUDSelectionWidget::EnsureSelectionAreaHost()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!IsValid(Root)) return false;
	if (!IsValid(SelectionAreaHost))
	{
		SelectionAreaHost = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SelectionAreaHost_Runtime"));
		if (!IsValid(SelectionAreaHost)) return false;
		UCanvasPanelSlot* HostSlot = Root->AddChildToCanvas(SelectionAreaHost);
		if (!IsValid(HostSlot)) { SelectionAreaHost = nullptr; return false; }
		HostSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		HostSlot->SetOffsets(FMargin(0.0f));
		int32 TopZ = 0;
		for (UWidget* Child : Root->GetAllChildren())
			if (Child != SelectionAreaHost)
				if (const UCanvasPanelSlot* ChildSlot = Cast<UCanvasPanelSlot>(Child->Slot))
					TopZ = FMath::Max(TopZ, ChildSlot->GetZOrder());
		HostSlot->SetZOrder(TopZ + 1);
		SelectionAreaHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	return SelectionAreaHost->GetParent() == Root
		&& Cast<UCanvasPanelSlot>(SelectionAreaHost->Slot) != nullptr
		&& SelectionAreaHost != OV_PlayArea;
}

FVector2D UBattleHUDSelectionWidget::GetSelectionAreaLayoutSize() const
{
	return IsValid(SelectionAreaHost) ? FVector2D(SelectionAreaHost->GetCachedGeometry().GetLocalSize()) : FVector2D::ZeroVector;
}

bool UBattleHUDSelectionWidget::SelectCard(int32 RuntimeId, bool bAllowFastPresentationCatchUp)
{
	if (!IsValid(ViewModel)) return false;
	if (!ViewModel->HasAuthoritativePendingCardSelection())
		return Super::SelectCard(RuntimeId, bAllowFastPresentationCatchUp);
	// A physical selection click never falls through into normal card play.
	if (ViewModel->IsSelectionPresentationSubmitInProgress()) return false;
	FPendingCardSelectionReadView Pending;
	if (!ViewModel->TryGetPendingCardSelectionReadView(Pending)
		|| !Pending.CandidateRuntimeIds.Contains(RuntimeId)
		|| !IsValid(HB_Hand) || !EnsureSelectionAreaHost()
		|| GetSelectionAreaLayoutSize().GetMin() <= 0.0) return false;
	if (PendingBattleId != ViewModel->BattleId || PendingBoundaryRevision != ViewModel->StateRevision)
	{
		ViewModel->CancelCardPresentationSelectionLifecycle(PendingGeneration);
		PendingGeneration = 0;
	}
	if (PendingGeneration == 0)
	{
		PendingGeneration = ViewModel->BeginCardPresentationSelectionLifecycle(ViewModel->StateRevision);
		PendingBattleId = ViewModel->BattleId;
		PendingBoundaryRevision = ViewModel->StateRevision;
	}
	if (PendingGeneration == 0) return false;
	if (ViewModel->IsPendingCardSelectionRuntimeIdSelected(RuntimeId))
	{
		if (!ViewModel->SubmitPendingCardSelectionByRuntimeId(RuntimeId)) return false;
		if (!ViewModel->SetPendingCardPresentationSelection(PendingGeneration, RuntimeId, false))
		{
			ViewModel->SubmitPendingCardSelectionByRuntimeId(RuntimeId);
			return false;
		}
		RefreshSharedSelectionPresentation();
		return true;
	}
	if (ViewModel->GetPendingCardSelectionSelectedCount() >= Pending.RequiredCount) return false;
	const FBattleHUDCardView* Frozen = ViewModel->HandCards.FindByPredicate(
		[RuntimeId](const FBattleHUDCardView& Card) { return Card.RuntimeId == RuntimeId; });
	if (!Frozen || CardWidgetClass == nullptr) return false;
	UBattleCardWidget* Visual = GetOwningPlayer()
		? CreateWidget<UBattleCardWidget>(GetOwningPlayer(), CardWidgetClass)
		: CreateWidget<UBattleCardWidget>(GetWorld(), CardWidgetClass);
	if (!IsValid(Visual)) return false;
	// Root the prepared object before any delegate can collect garbage.
	FSelectionAreaVisualState State;
	State.Card = Visual;
	State.BattleId = PendingBattleId;
	State.Generation = PendingGeneration;
	State.BoundaryRevision = PendingBoundaryRevision;
	SelectionVisuals.Add(RuntimeId, State);
	Visual->SetVisibility(ESlateVisibility::Hidden);
	Visual->SetIsEnabled(false);
	Visual->SetCardView(*Frozen);
	Visual->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UOverlaySlot* VisualSlot = SelectionAreaHost->AddChildToOverlay(Visual);
	if (!IsValid(VisualSlot))
	{
		SelectionVisuals.Remove(RuntimeId);
		return false;
	}
	VisualSlot->SetHorizontalAlignment(HAlign_Center);
	VisualSlot->SetVerticalAlignment(VAlign_Center);
	Visual->OnBattleCardRequested.AddUniqueDynamic(this, &UBattleHUDSelectionWidget::HandleSelectionAreaCardRequested);
	if (!ViewModel->SubmitPendingCardSelectionByRuntimeId(RuntimeId)
		|| !ViewModel->SetPendingCardPresentationSelection(PendingGeneration, RuntimeId, true))
	{
		if (ViewModel->IsPendingCardSelectionRuntimeIdSelected(RuntimeId)) ViewModel->SubmitPendingCardSelectionByRuntimeId(RuntimeId);
		Visual->RemoveFromParent();
		SelectionVisuals.Remove(RuntimeId);
		return false;
	}
	RefreshSharedSelectionPresentation();
	return true;
}

void UBattleHUDSelectionWidget::HandleSelectionAreaCardRequested(int32 RuntimeId)
{
	const FSelectionAreaVisualState* State = SelectionVisuals.Find(RuntimeId);
	FCardPresentationOwnershipEntry Entry;
	if (!State || !IsValid(ViewModel) || !IsValid(State->Card)
		|| !State->Card->GetIsEnabled()
		|| !ViewModel->TryGetCardPresentationOwnershipEntry(RuntimeId, Entry)
		|| Entry.BattleId != State->BattleId || Entry.SelectionGeneration != State->Generation
		|| Entry.Owner != ECardPresentationOwner::SelectionArea
		|| Entry.Phase != ESelectionPresentationVisualPhase::Pending) return;
	SelectCard(RuntimeId, false);
}

void UBattleHUDSelectionWidget::HandleSelectionAwareConfirmClicked()
{
	if (!IsValid(ViewModel)) return;
	if (!ViewModel->HasAuthoritativePendingCardSelection()) { ConfirmSelectedCard(); return; }
	if (PendingGeneration <= 0 || !ViewModel->CanConfirmPendingCardSelection()) return;
	// Freeze layout before submitting; Confirm never re-centers surviving children.
	UpdatePendingSelectionAreaLayout();
	if (ViewModel->ConfirmPendingCardSelectionWithPresentation(PendingGeneration))
	{
		PendingGeneration = 0;
		ClearSharedSelectionControlsAfterSubmit();
	}
	RefreshSharedSelectionPresentation();
}

void UBattleHUDSelectionWidget::HandleSelectionAwareCancelClicked()
{
	if (!IsValid(ViewModel)) return;
	if (!ViewModel->HasAuthoritativePendingCardSelection()) { CancelSelection(); return; }
	const int64 Generation = PendingGeneration;
	if (ViewModel->SubmitPendingCardSelectionCancel())
	{
		ViewModel->CancelCardPresentationSelectionLifecycle(Generation);
		PendingGeneration = 0;
		ClearSharedSelectionControlsAfterSubmit();
	}
	RefreshSharedSelectionPresentation();
}

bool UBattleHUDSelectionWidget::HandleRightMouseButtonCancel()
{
	if (IsValid(ViewModel) && ViewModel->HasAuthoritativePendingCardSelection())
	{
		// A pending Gameplay selection has its own cancel policy. Right-click must
		// use the same transactional path as the visible Cancel button and must not
		// fall through into ordinary card-play cancellation.
		if (!ViewModel->CanCancelPendingCardSelection())
		{
			return false;
		}
		HandleSelectionAwareCancelClicked();
		return true;
	}

	return Super::HandleRightMouseButtonCancel();
}

void UBattleHUDSelectionWidget::NativeOnBattleHUDViewModelChanged()
{
	if (SelectionBoundViewModel.Get() != ViewModel.Get())
	{
		if (UBattleHUDViewModel* Previous = SelectionBoundViewModel.Get())
			if (Previous->CancelCardPresentationSelectionLifecycle(PendingGeneration)) Previous->ClearPendingCardSelectionInputState();
		ReleaseSelectionAreaVisuals();
		PendingGeneration = PendingBattleId = PendingBoundaryRevision = 0;
		SelectionBoundViewModel = ViewModel;
	}
	Super::NativeOnBattleHUDViewModelChanged();
	SynchronizeSelectionSurfaces({});
	RefreshSharedSelectionPresentation();
}

void UBattleHUDSelectionWidget::HandleCardPresentationOwnershipChanged(const TArray<int32>& RuntimeIds)
{
	SynchronizeSelectionSurfaces(RuntimeIds);
}

void UBattleHUDSelectionWidget::SynchronizeSelectionSurfaces(const TArray<int32>& ChangedIds)
{
	if (bSynchronizingSelection || !IsValid(ViewModel) || !IsValid(HB_Hand)) return;
	TGuardValue<bool> Guard(bSynchronizingSelection, true);
	ReleaseObsoleteSelectionTransitions();
	TArray<int32> VisualIds;
	SelectionVisuals.GetKeys(VisualIds);
	VisualIds.Sort();
	TArray<int32> RestoreIds = ChangedIds;
	// Retire old visible owners before revealing formal replacements.
	for (int32 RuntimeId : VisualIds)
	{
		FSelectionAreaVisualState& State = SelectionVisuals.FindChecked(RuntimeId);
		FCardPresentationOwnershipEntry Entry;
		const bool bSameScope = ViewModel->TryGetCardPresentationOwnershipEntry(RuntimeId, Entry)
			&& Entry.BattleId == State.BattleId && Entry.SelectionGeneration == State.Generation
			&& Entry.SelectionBoundaryRevision == State.BoundaryRevision;
		if (!bSameScope || Entry.Owner != ECardPresentationOwner::SelectionArea)
		{
			if (IsValid(State.Card))
			{
				State.Card->OnBattleCardRequested.RemoveDynamic(this, &UBattleHUDSelectionWidget::HandleSelectionAreaCardRequested);
				// Accepted G4 transition already reparented this exact object.
				if (!bSameScope || Entry.Owner != ECardPresentationOwner::Transition) State.Card->RemoveFromParent();
			}
			SelectionVisuals.Remove(RuntimeId);
			RestoreIds.AddUnique(RuntimeId);
		}
	}
	bool bHandChanged = HB_Hand->GetChildrenCount() != ViewModel->HandCards.Num();
	for (int32 Index = 0; !bHandChanged && Index < ViewModel->HandCards.Num(); ++Index)
	{
		const UBattleCardWidget* Card = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index));
		bHandChanged = !Card || Card->GetRuntimeId() != ViewModel->HandCards[Index].RuntimeId;
	}
	// Complete structural reconciliation before external ownership/snapshot listeners.
	if (bHandChanged && !HasActiveNativePresentation()) Super::RefreshHand();
	Super::HandleCardPresentationOwnershipChanged(RestoreIds);
	for (UWidget* Child : HB_Hand->GetAllChildren())
	{
		if (UBattleCardWidget* Card = Cast<UBattleCardWidget>(Child))
		{
			if (ViewModel->GetCardPresentationOwner(Card->GetRuntimeId()) != ECardPresentationOwner::Hand)
			{
				Card->SetVisibility(ESlateVisibility::Hidden);
				Card->SetIsEnabled(false);
			}
		}
	}
	UpdatePendingSelectionAreaLayout();
	for (const auto& Pair : SelectionVisuals)
	{
		const auto& State = Pair.Value;
		FCardPresentationOwnershipEntry Entry;
		if (IsValid(State.Card) && ViewModel->TryGetCardPresentationOwnershipEntry(Pair.Key, Entry))
		{
			const bool bPending = Entry.Phase == ESelectionPresentationVisualPhase::Pending;
			State.Card->SetPendingSelectionPresentation(bPending, true, bPending);
			State.Card->SetIsEnabled(bPending);
			State.Card->SetVisibility(bPending ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
		}
	}
}

void UBattleHUDSelectionWidget::UpdatePendingSelectionAreaLayout()
{
	if (!IsValid(ViewModel) || !IsValid(SelectionAreaHost)) return;
	TArray<int32> PendingIds;
	// Frozen Hand order, not click order or TMap iteration.
	for (const FBattleHUDCardView& Card : ViewModel->HandCards)
	{
		FCardPresentationOwnershipEntry Entry;
		if (SelectionVisuals.Contains(Card.RuntimeId)
			&& ViewModel->TryGetCardPresentationOwnershipEntry(Card.RuntimeId, Entry)
			&& Entry.Owner == ECardPresentationOwner::SelectionArea
			&& Entry.Phase == ESelectionPresentationVisualPhase::Pending) PendingIds.Add(Card.RuntimeId);
	}
	const FVector2D Size = GetSelectionAreaLayoutSize();
	for (int32 Index = 0; Index < PendingIds.Num(); ++Index)
	{
		UBattleCardWidget* Card = SelectionVisuals.FindChecked(PendingIds[Index]).Card;
		if (IsValid(Card)) Card->SetRenderTranslation(FVector2D((Index - (PendingIds.Num() - 1) * 0.5f) * 190.0f, -Size.Y * 0.08f));
	}
}

void UBattleHUDSelectionWidget::RefreshSharedSelectionPresentation()
{
	if (!IsValid(ViewModel)) return;
	FPendingCardSelectionReadView Pending;
	const bool bPending = ViewModel->TryGetPendingCardSelectionReadView(Pending);
	SetSelectionLayoutActive(bPending);
	SetPlayedCardSelectionHidden(bPending || !SelectionVisuals.IsEmpty() || HasActiveNativePresentation());
	if (IsValid(HB_Hand))
		for (UWidget* Child : HB_Hand->GetAllChildren())
			if (UBattleCardWidget* Card = Cast<UBattleCardWidget>(Child))
				Card->SetPendingSelectionPresentation(bPending, bPending && Pending.CandidateRuntimeIds.Contains(Card->GetRuntimeId()), false);
	if (!bPending) return;
	if (IsValid(Btn_EndTurn)) Btn_EndTurn->SetIsEnabled(false);
	if (IsValid(Btn_Confirm))
	{
		Btn_Confirm->SetVisibility(ESlateVisibility::Visible);
		Btn_Confirm->SetIsEnabled(PendingGeneration > 0 && ViewModel->CanConfirmPendingCardSelection());
	}
	if (IsValid(Btn_Cancel))
	{
		Btn_Cancel->SetVisibility(Pending.bCanCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_Cancel->SetIsEnabled(ViewModel->CanCancelPendingCardSelection());
	}
	if (IsValid(Txt_Feedback))
		Txt_Feedback->SetText(FText::Format(NSLOCTEXT("BattleHUDSelectionWidget", "SelectionProgress", "选择卡牌 {0}/{1}，然后确认"),
			FText::AsNumber(ViewModel->GetPendingCardSelectionSelectedCount()), FText::AsNumber(Pending.RequiredCount)));
}

void UBattleHUDSelectionWidget::ClearSharedSelectionControlsAfterSubmit()
{
	SetSelectionLayoutActive(false);
	if (IsValid(Btn_Confirm)) { Btn_Confirm->SetIsEnabled(false); Btn_Confirm->SetVisibility(ESlateVisibility::Collapsed); }
	if (IsValid(Btn_Cancel)) { Btn_Cancel->SetIsEnabled(false); Btn_Cancel->SetVisibility(ESlateVisibility::Collapsed); }
	if (IsValid(Btn_EndTurn)) Btn_EndTurn->SetIsEnabled(false);
	if (IsValid(Txt_Feedback)) Txt_Feedback->SetText(FText::GetEmpty());
}

void UBattleHUDSelectionWidget::ReleaseSelectionAreaVisuals()
{
	for (const auto& Pair : SelectionVisuals)
		if (IsValid(Pair.Value.Card))
		{
			Pair.Value.Card->OnBattleCardRequested.RemoveDynamic(this, &UBattleHUDSelectionWidget::HandleSelectionAreaCardRequested);
			Pair.Value.Card->RemoveFromParent();
		}
	SelectionVisuals.Reset();
}

void UBattleHUDSelectionWidget::SetPlayedCardSelectionHidden(bool bHidden)
{
	if (UBattleCardWidget* Card = GetNativePlayedCardWidget())
		Card->SetVisibility(bHidden ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
}
void UBattleHUDSelectionWidget::OnNativeCardTransitionAccepted(int32 RuntimeId) { SetPlayedCardSelectionHidden(true); }
void UBattleHUDSelectionWidget::OnNativeCardTransitionEnded(int32 RuntimeId, bool bCancelled)
{
	SetPlayedCardSelectionHidden(!SelectionVisuals.IsEmpty());
}

void UBattleHUDSelectionWidget::SetSelectionLayoutActive(bool bActive)
{
	if (!bActive)
	{
		if (SelectionBackdrop) SelectionBackdrop->SetVisibility(ESlateVisibility::Collapsed);
		for (const auto& Entry : SelectionOriginalLayouts)
			if (UCanvasPanelSlot* CanvasSlot = Entry.Key.Get()) CanvasSlot->SetLayout(Entry.Value);
		for (const auto& Entry : SelectionOriginalZOrders)
			if (UCanvasPanelSlot* CanvasSlot = Entry.Key.Get()) CanvasSlot->SetZOrder(Entry.Value);
		SelectionOriginalLayouts.Reset();
		SelectionOriginalZOrders.Reset();
		return;
	}

	EnsureSelectionAreaHost();
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root || !SelectionOriginalZOrders.IsEmpty()) return;
	if (!SelectionBackdrop)
	{
		SelectionBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		SelectionBackdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
		UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(SelectionBackdrop);
		BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackdropSlot->SetOffsets(FMargin(0.0f));
	}
	int32 BackdropZ = 0;
	for (UWidget* Child : Root->GetAllChildren())
		if (Child != SelectionBackdrop && Child != SelectionAreaHost)
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Child->Slot)) BackdropZ = FMath::Max(BackdropZ, CanvasSlot->GetZOrder());
	CastChecked<UCanvasPanelSlot>(SelectionBackdrop->Slot)->SetZOrder(BackdropZ + 1);
	SelectionBackdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
	const TArray<UWidget*> Foreground = { HB_Hand, SelectionAreaHost, Btn_Confirm, Btn_Cancel, Txt_Feedback };
	for (UWidget* Surface : Foreground)
	{
		if (!Surface) continue;
		UWidget* RootChild = Surface;
		while (RootChild->GetParent() && RootChild->GetParent() != Root) RootChild = RootChild->GetParent();
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(RootChild->Slot))
		{
			SelectionOriginalZOrders.FindOrAdd(CanvasSlot, CanvasSlot->GetZOrder());
			CanvasSlot->SetZOrder(BackdropZ + 2);
			if (RootChild == Surface && (Surface == Btn_Confirm || Surface == Txt_Feedback))
			{
				SelectionOriginalLayouts.Add(CanvasSlot, CanvasSlot->GetLayout());
				CanvasSlot->SetAnchors(FAnchors(0.5f, Surface == Btn_Confirm ? 0.65f : 0.16f));
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				CanvasSlot->SetPosition(FVector2D::ZeroVector);
			}
		}
	}
}
