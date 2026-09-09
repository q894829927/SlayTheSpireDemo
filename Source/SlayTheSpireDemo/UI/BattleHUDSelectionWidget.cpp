#include "BattleHUDSelectionWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "../Battle/BattleSelectionRequest.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

namespace
{
	constexpr float SharedSelectionTransferDurationSeconds = 0.5f;
	const FVector2D SharedSelectionHandFallbackTranslation(-300.0f, 120.0f);
	const FVector2D SharedSelectionDrawPileFallbackTranslation(-420.0f, 90.0f);
}

void UBattleHUDSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!EnsureSelectionAreaHost())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BattleHUD][G0-C] SelectionAreaHost could not be created for '%s'."),
			*GetPathName());
	}

	if (IsValid(Btn_Confirm))
	{
		Btn_Confirm->OnClicked.RemoveDynamic(
			this,
			&UBattleHUDSelectionWidget::HandleConfirmClicked);
		Btn_Confirm->OnClicked.AddUniqueDynamic(
			this,
			&UBattleHUDSelectionWidget::HandleSelectionAwareConfirmClicked);
	}
	if (IsValid(Btn_Cancel))
	{
		Btn_Cancel->OnClicked.RemoveDynamic(
			this,
			&UBattleHUDSelectionWidget::HandleCancelClicked);
		Btn_Cancel->OnClicked.AddUniqueDynamic(
			this,
			&UBattleHUDSelectionWidget::HandleSelectionAwareCancelClicked);
	}
	bSelectionAwareDelegatesBound = true;
	RefreshSharedSelectionPresentation();
}

void UBattleHUDSelectionWidget::NativeDestruct()
{
	SetSelectionLayoutActive(false);
	ResetSharedSelectionCardVisuals();
	if (bSelectionAwareDelegatesBound)
	{
		if (IsValid(Btn_Confirm))
		{
			Btn_Confirm->OnClicked.RemoveDynamic(
				this,
				&UBattleHUDSelectionWidget::HandleSelectionAwareConfirmClicked);
		}
		if (IsValid(Btn_Cancel))
		{
			Btn_Cancel->OnClicked.RemoveDynamic(
				this,
				&UBattleHUDSelectionWidget::HandleSelectionAwareCancelClicked);
		}
		bSelectionAwareDelegatesBound = false;
	}

	CancelSharedHandToDrawPilePresentation();
	if (IsValid(SelectionAreaHost))
	{
		SelectionAreaHost->ClearChildren();
		SelectionAreaHost->RemoveFromParent();
		SelectionAreaHost = nullptr;
	}
	Super::NativeDestruct();
}

void UBattleHUDSelectionWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateSelectionCardPositions();
	UpdateSharedHandToDrawPileAnimation(InDeltaTime);
}

bool UBattleHUDSelectionWidget::SelectCard(int32 RuntimeId, bool bAllowFastPresentationCatchUp)
{
	const bool bAccepted = Super::SelectCard(RuntimeId, bAllowFastPresentationCatchUp);
	RefreshSharedSelectionPresentation();
	UpdateSelectionCardPositions();
	return bAccepted;
}

bool UBattleHUDSelectionWidget::EnsureSelectionAreaHost()
{
	if (IsValid(SelectionAreaHost))
	{
		return true;
	}

	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!IsValid(Root) || !IsValid(WidgetTree))
	{
		return false;
	}

	SelectionAreaHost = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("SelectionAreaHost_Runtime"));
	if (!IsValid(SelectionAreaHost))
	{
		return false;
	}

	UCanvasPanelSlot* HostSlot = Root->AddChildToCanvas(SelectionAreaHost);
	if (!IsValid(HostSlot))
	{
		SelectionAreaHost = nullptr;
		return false;
	}

	HostSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	HostSlot->SetOffsets(FMargin(0.0f));
	SelectionAreaHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return true;
}

void UBattleHUDSelectionWidget::SetPlayedCardSelectionHidden(bool bHidden)
{
	if (UBattleCardWidget* PlayedCard = GetNativePlayedCardWidget())
	{
		PlayedCard->SetVisibility(bHidden ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
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

void UBattleHUDSelectionWidget::UpdateSelectionCardPositions()
{
	if (!bSelectionVisualMode || !IsValid(HB_Hand) || !IsValid(ViewModel)) return;
	const FGeometry& HUDGeometry = GetCachedGeometry();
	if (HUDGeometry.GetLocalSize().SizeSquared() <= 0.0f) return;
	int32 SelectedIndex = 0;
	const int32 SelectedCount = ViewModel->GetPendingCardSelectionSelectedCount();
	for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
	{
		UBattleCardWidget* Card = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index));
		if (!Card) continue;
		if (!ViewModel->IsPendingCardSelectionRuntimeIdSelected(Card->GetRuntimeId()))
		{
			Card->SetRenderTranslation(FVector2D::ZeroVector);
			continue;
		}
		const FGeometry& Geometry = Card->GetCachedGeometry();
		const FVector2D Target = HUDGeometry.GetAccumulatedLayoutTransform().TransformPoint(
			HUDGeometry.GetLocalSize() * FVector2D(0.5f, 0.42f)
			+ FVector2D((SelectedIndex++ - (SelectedCount - 1) * 0.5f) * 190.0f, 0.0f));
		const FVector2D Origin = Geometry.GetAccumulatedLayoutTransform().TransformPoint(Geometry.GetLocalSize() * 0.5f);
		Card->SetRenderTranslation((Target - Origin) / Geometry.GetAccumulatedLayoutTransform().GetScale());
	}
}

void UBattleHUDSelectionWidget::NativeOnBattleHUDViewModelChanged()
{
	Super::NativeOnBattleHUDViewModelChanged();
	RefreshSharedSelectionPresentation();
}

bool UBattleHUDSelectionWidget::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	if (Record.Type == EBattlePresentationRecordType::CardZoneChanged
		&& Record.CardZoneChanged.FromZone == ECardZone::Hand
		&& Record.CardZoneChanged.ToZone == ECardZone::DrawPile)
	{
		const bool bStarted = BeginSharedHandToDrawPilePresentation(Record, Token);
		if (!bStarted)
		{
			ConfirmedCardCenters.Remove(Record.CardZoneChanged.Card.RuntimeId);
			SetPlayedCardSelectionHidden(!ConfirmedCardCenters.IsEmpty());
		}
		return bStarted;
	}
	if (Record.Type == EBattlePresentationRecordType::CardZoneChanged)
	{
		if (ConfirmedCardCenters.Contains(Record.CardZoneChanged.Card.RuntimeId) && IsValid(HB_Hand))
		{
			for (UWidget* Child : HB_Hand->GetAllChildren())
				if (UBattleCardWidget* Card = Cast<UBattleCardWidget>(Child))
					if (Card->GetRuntimeId() == Record.CardZoneChanged.Card.RuntimeId)
						Card->SetVisibility(ESlateVisibility::Visible);
		}
		ConfirmedCardCenters.Remove(Record.CardZoneChanged.Card.RuntimeId);
		if (Record.CardZoneChanged.FromZone == ECardZone::PlayArea)
		{
			ConfirmedCardCenters.Reset();
			SetPlayedCardSelectionHidden(false);
		}
	}

	return Super::BeginPresentationRecordPlayback_Implementation(Record, Token);
}

void UBattleHUDSelectionWidget::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& Token)
{
	if (bSharedTransferActive && Token == SharedTransferToken)
	{
		Super::CancelPresentationRecordPlayback_Implementation(Token);
		CancelSharedHandToDrawPilePresentation();
		return;
	}

	Super::CancelPresentationRecordPlayback_Implementation(Token);
}

void UBattleHUDSelectionWidget::HandleSelectionAwareConfirmClicked()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	if (ViewModel->HasAuthoritativePendingCardSelection())
	{
		if (ViewModel->HasPendingCardSelection())
		{
			ConfirmedCardCenters.Reset();
			if (IsValid(HB_Hand))
			{
				for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
				{
					UBattleCardWidget* Card = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index));
					if (Card && ViewModel->IsPendingCardSelectionRuntimeIdSelected(Card->GetRuntimeId()))
					{
						const FGeometry& Geometry = Card->GetCachedGeometry();
						ConfirmedCardCenters.Add(Card->GetRuntimeId(), Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f));
					}
				}
			}
			if (ViewModel->ConfirmPendingCardSelection())
			{
				ResetSharedSelectionCardVisuals();
				ClearSharedSelectionControlsAfterSubmit();
			}
			else
			{
				ConfirmedCardCenters.Reset();
				RefreshSharedSelectionPresentation();
			}
		}
		return;
	}

	ConfirmSelectedCard();
}

void UBattleHUDSelectionWidget::HandleSelectionAwareCancelClicked()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	if (ViewModel->HasAuthoritativePendingCardSelection())
	{
		if (ViewModel->HasPendingCardSelection())
		{
			if (ViewModel->SubmitPendingCardSelectionCancel())
			{
				ResetSharedSelectionCardVisuals();
				ClearSharedSelectionControlsAfterSubmit();
			}
			else
			{
				RefreshSharedSelectionPresentation();
			}
		}
		return;
	}

	CancelSelection();
}

void UBattleHUDSelectionWidget::RefreshSharedSelectionPresentation()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	FPendingCardSelectionReadView PendingView;
	const bool bPending = ViewModel->TryGetPendingCardSelectionReadView(PendingView);
	if (!bPending && !ViewModel->bInputLocked && !HasActiveNativePresentation()) ConfirmedCardCenters.Reset();
	bSelectionVisualMode = bPending;
	SetSelectionLayoutActive(bPending);
	SetPlayedCardSelectionHidden(bPending || !ConfirmedCardCenters.IsEmpty() || bSharedTransferActive);
	if (IsValid(HB_Hand))
	{
		for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
		{
			if (UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index)))
			{
				const int32 RuntimeId = CardWidget->GetRuntimeId();
				const bool bCandidate = bPending
					&& PendingView.CandidateRuntimeIds.Contains(RuntimeId);
				const bool bSelected = bPending
					&& ViewModel->IsPendingCardSelectionRuntimeIdSelected(RuntimeId);
				CardWidget->SetPendingSelectionPresentation(
					bPending,
					bCandidate,
					bSelected);
				// G0-B preserves the same formal Widget object across Hand reconciliation.
				// Until G5 switches production ownership to SelectionArea, keep the
				// existing confirmed-position compatibility behavior unchanged.
				const bool bConfirmedCard = !bPending && ConfirmedCardCenters.Contains(RuntimeId);
				if (!bConfirmedCard) CardWidget->SetRenderTranslation(FVector2D::ZeroVector);
				if (bConfirmedCard) CardWidget->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}

	if (!bPending)
	{
		return;
	}

	if (IsValid(Btn_EndTurn))
	{
		Btn_EndTurn->SetIsEnabled(false);
	}
	if (IsValid(Btn_Confirm))
	{
		Btn_Confirm->SetVisibility(ESlateVisibility::Visible);
		Btn_Confirm->SetIsEnabled(ViewModel->CanConfirmPendingCardSelection());
	}
	if (IsValid(Btn_Cancel))
	{
		const bool bCanCancel = ViewModel->CanCancelPendingCardSelection();
		Btn_Cancel->SetVisibility(
			bCanCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Btn_Cancel->SetIsEnabled(bCanCancel);
	}
	if (IsValid(Txt_Feedback))
	{
		const int32 SelectedCount = ViewModel->GetPendingCardSelectionSelectedCount();
		Txt_Feedback->SetText(ViewModel->CanConfirmPendingCardSelection()
			? FText::Format(
				NSLOCTEXT("BattleHUDSelectionWidget", "PendingSelectionReady", "已选择卡牌 {0}/{1}，请确认"),
				FText::AsNumber(SelectedCount),
				FText::AsNumber(PendingView.RequiredCount))
			: FText::Format(
				NSLOCTEXT("BattleHUDSelectionWidget", "PendingSelectionProgress", "选择卡牌 {0}/{1}"),
				FText::AsNumber(SelectedCount),
				FText::AsNumber(PendingView.RequiredCount)));
	}
}

void UBattleHUDSelectionWidget::ResetSharedSelectionCardVisuals()
{
	if (!IsValid(HB_Hand))
	{
		return;
	}

	for (int32 Index = 0; Index < HB_Hand->GetChildrenCount(); ++Index)
	{
		if (UBattleCardWidget* CardWidget = Cast<UBattleCardWidget>(HB_Hand->GetChildAt(Index)))
		{
			CardWidget->SetPendingSelectionPresentation(false, false, false);
			if (!ConfirmedCardCenters.Contains(CardWidget->GetRuntimeId()))
			{
				CardWidget->SetRenderTranslation(FVector2D::ZeroVector);
			}
			else
			{
				CardWidget->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
}

void UBattleHUDSelectionWidget::ClearSharedSelectionControlsAfterSubmit()
{
	bSelectionVisualMode = false;
	SetSelectionLayoutActive(false);
	if (IsValid(Btn_Confirm))
	{
		Btn_Confirm->SetIsEnabled(false);
		Btn_Confirm->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(Btn_Cancel))
	{
		Btn_Cancel->SetIsEnabled(false);
		Btn_Cancel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(Btn_EndTurn))
	{
		Btn_EndTurn->SetIsEnabled(false);
	}
	if (IsValid(Txt_Feedback))
	{
		Txt_Feedback->SetText(FText::GetEmpty());
	}
}

bool UBattleHUDSelectionWidget::BeginSharedHandToDrawPilePresentation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FCardZoneChangedPresentationPayload& Payload = Record.CardZoneChanged;
	UBattleCardWidget* HistoricalHandCard = nullptr;
	if (!IsNativeRecordTokenConsistent(Record, Token)
		|| bSharedTransferActive
		|| HasActiveNativePresentation()
		|| !IsValid(ViewModel)
		|| !IsValid(HB_Hand)
		|| !IsValid(OV_PlayArea)
		|| !IsValid(Txt_DrawCount)
		|| CardWidgetClass == nullptr
		|| !IsNativeCardSnapshotValid(Payload.Card)
		|| Payload.ToIndex != ViewModel->DrawCount
		|| !FindExactHistoricalHandCard(Payload.Card, Payload.FromIndex, HistoricalHandCard))
	{
		return false;
	}

	UBattleCardWidget* MovingCard = CreateNativePresentationCard(Payload.Card);
	if (!IsValid(MovingCard)
		|| !CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	UOverlaySlot* MotionSlot = OV_PlayArea->AddChildToOverlay(MovingCard);
	if (!IsValid(MotionSlot))
	{
		MovingCard->RemoveFromParent();
		AbortNativePresentationStart();
		return false;
	}
	MotionSlot->SetHorizontalAlignment(HAlign_Center);
	MotionSlot->SetVerticalAlignment(VAlign_Center);

	SharedTransferHistoricalCard = HistoricalHandCard;
	SharedTransferHistoricalVisibility = HistoricalHandCard->GetVisibility();
	if (ConfirmedCardCenters.Contains(Payload.Card.RuntimeId)) SharedTransferHistoricalVisibility = ESlateVisibility::Visible;
	SharedTransferMovingCard = MovingCard;
	SharedTransferToken = Token;
	SharedTransferElapsedSeconds = 0.0f;
	SharedTransferStartTranslation = SharedSelectionHandFallbackTranslation;
	SharedTransferEndTranslation = SharedSelectionDrawPileFallbackTranslation;
	bSharedTransferGeometryInitialized = false;
	bSharedTransferActive = true;

	if (const FVector2D* Center = ConfirmedCardCenters.Find(Payload.Card.RuntimeId))
	{
		const FGeometry& AreaGeometry = OV_PlayArea->GetCachedGeometry();
		if (AreaGeometry.GetLocalSize().SizeSquared() > 0.0f)
			SharedTransferStartTranslation = FVector2D(AreaGeometry.AbsoluteToLocal(*Center)) - FVector2D(AreaGeometry.GetLocalSize() * 0.5f);
	}
	MovingCard->SetRenderTranslation(SharedTransferStartTranslation);
	MovingCard->SetRenderScale(FVector2D(1.0f, 1.0f));
	MovingCard->SetRenderOpacity(1.0f);
	HistoricalHandCard->SetVisibility(ESlateVisibility::Hidden);

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		CancelSharedHandToDrawPilePresentation();
		return false;
	}

	const FPresentationPlaybackToken ExpectedToken = SharedTransferToken;
	const TWeakObjectPtr<UBattleHUDSelectionWidget> WeakThis(this);
	FTimerDelegate FinishDelegate = FTimerDelegate::CreateLambda(
		[WeakThis, ExpectedToken]()
		{
			if (UBattleHUDSelectionWidget* Widget = WeakThis.Get())
			{
				Widget->FinishSharedHandToDrawPilePresentation(ExpectedToken);
			}
		});
	World->GetTimerManager().SetTimer(
		SharedTransferFinishTimer,
		FinishDelegate,
		SharedSelectionTransferDurationSeconds,
		false);
	if (!SharedTransferFinishTimer.IsValid())
	{
		CancelSharedHandToDrawPilePresentation();
		return false;
	}
	return true;
}

void UBattleHUDSelectionWidget::UpdateSharedHandToDrawPileAnimation(float DeltaSeconds)
{
	if (!bSharedTransferActive || !IsValid(SharedTransferMovingCard))
	{
		return;
	}

	UBattleCardWidget* MovingCard = SharedTransferMovingCard.Get();
	auto ResolveAnchorTranslation = [this](UWidget* Anchor, const FVector2D& Fallback) -> FVector2D
	{
		if (!IsValid(Anchor))
		{
			return Fallback;
		}
		const FGeometry& AnchorGeometry = Anchor->GetCachedGeometry();
		const FGeometry& CardGeometry = OV_PlayArea->GetCachedGeometry();
		if (AnchorGeometry.GetLocalSize().SizeSquared() <= 0.0f
			|| CardGeometry.GetLocalSize().SizeSquared() <= 0.0f)
		{
			return Fallback;
		}
		const FVector2D AnchorAbsolute = AnchorGeometry.LocalToAbsolute(
			AnchorGeometry.GetLocalSize() * 0.5f);
		return FVector2D(CardGeometry.AbsoluteToLocal(AnchorAbsolute))
			- FVector2D(CardGeometry.GetLocalSize() * 0.5f);
	};

	if (!bSharedTransferGeometryInitialized)
	{
		SharedTransferStartTranslation = ResolveAnchorTranslation(
			SharedTransferHistoricalCard.Get(),
			SharedSelectionHandFallbackTranslation);
		if (const FVector2D* ConfirmedCenter = ConfirmedCardCenters.Find(MovingCard->GetRuntimeId()))
		{
			const FGeometry& AreaGeometry = OV_PlayArea->GetCachedGeometry();
			if (AreaGeometry.GetLocalSize().SizeSquared() > 0.0f)
			{
				SharedTransferStartTranslation = FVector2D(AreaGeometry.AbsoluteToLocal(*ConfirmedCenter))
					- FVector2D(AreaGeometry.GetLocalSize() * 0.5f);
			}
		}
		SharedTransferEndTranslation = ResolveAnchorTranslation(
			Txt_DrawCount,
			SharedSelectionDrawPileFallbackTranslation);
		MovingCard->SetRenderTranslation(SharedTransferStartTranslation);
		bSharedTransferGeometryInitialized = true;
	}

	SharedTransferElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
	const float LinearAlpha = FMath::Clamp(
		SharedTransferElapsedSeconds / SharedSelectionTransferDurationSeconds,
		0.0f,
		1.0f);
	const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, LinearAlpha, 3.0f);
	MovingCard->SetRenderTranslation(FMath::Lerp(
		SharedTransferStartTranslation,
		SharedTransferEndTranslation,
		EasedAlpha));
	const float Scale = FMath::Lerp(1.0f, 0.72f, EasedAlpha);
	MovingCard->SetRenderScale(FVector2D(Scale, Scale));
	MovingCard->SetRenderOpacity(1.0f - FMath::Clamp((LinearAlpha - 0.85f) / 0.15f, 0.0f, 1.0f));
}

void UBattleHUDSelectionWidget::FinishSharedHandToDrawPilePresentation(
	const FPresentationPlaybackToken& ExpectedToken)
{
	if (!bSharedTransferActive
		|| ExpectedToken != SharedTransferToken
		|| !HasActiveNativePresentation()
		|| GetActiveNativePresentationToken() != ExpectedToken)
	{
		return;
	}

	if (UWorld* World = GetWorld(); IsValid(World) && SharedTransferFinishTimer.IsValid())
	{
		World->GetTimerManager().ClearTimer(SharedTransferFinishTimer);
	}
	SharedTransferFinishTimer.Invalidate();

	if (UBattleCardWidget* HistoricalCard = SharedTransferHistoricalCard.Get())
	{
		HistoricalCard->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsValid(SharedTransferMovingCard))
	{
		ConfirmedCardCenters.Remove(SharedTransferMovingCard->GetRuntimeId());
		SharedTransferMovingCard->RemoveFromParent();
	}

	const FPresentationPlaybackToken CompletedToken = SharedTransferToken;
	ResetSharedHandToDrawPileState();
	ResetNativePresentationOwnership();
	SetPlayedCardSelectionHidden(!ConfirmedCardCenters.IsEmpty());
	NotifyPresentationFinished(CompletedToken);
}

void UBattleHUDSelectionWidget::CancelSharedHandToDrawPilePresentation()
{
	ConfirmedCardCenters.Reset();
	SetPlayedCardSelectionHidden(false);
	const bool bOwnsNativePresentation =
		bSharedTransferActive
		&& HasActiveNativePresentation()
		&& GetActiveNativePresentationToken() == SharedTransferToken;

	if (UWorld* World = GetWorld(); IsValid(World) && SharedTransferFinishTimer.IsValid())
	{
		World->GetTimerManager().ClearTimer(SharedTransferFinishTimer);
	}
	SharedTransferFinishTimer.Invalidate();

	if (UBattleCardWidget* HistoricalCard = SharedTransferHistoricalCard.Get())
	{
		HistoricalCard->SetVisibility(SharedTransferHistoricalVisibility);
	}
	if (IsValid(SharedTransferMovingCard))
	{
		SharedTransferMovingCard->RemoveFromParent();
	}
	ResetSharedHandToDrawPileState();
	if (bOwnsNativePresentation)
	{
		ResetNativePresentationOwnership();
	}
}

void UBattleHUDSelectionWidget::ResetSharedHandToDrawPileState()
{
	SharedTransferMovingCard = nullptr;
	SharedTransferHistoricalCard.Reset();
	SharedTransferHistoricalVisibility = ESlateVisibility::Visible;
	SharedTransferToken = FPresentationPlaybackToken{};
	SharedTransferStartTranslation = FVector2D::ZeroVector;
	SharedTransferEndTranslation = FVector2D::ZeroVector;
	SharedTransferElapsedSeconds = 0.0f;
	bSharedTransferGeometryInitialized = false;
	bSharedTransferActive = false;
}
