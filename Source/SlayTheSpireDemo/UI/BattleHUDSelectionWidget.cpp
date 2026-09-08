#include "BattleHUDSelectionWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "../Battle/BattleSelectionRequest.h"
#include "Components/Button.h"
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
	Super::NativeDestruct();
}

void UBattleHUDSelectionWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateSharedHandToDrawPileAnimation(InDeltaTime);
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
		return BeginSharedHandToDrawPilePresentation(Record, Token);
	}

	return Super::BeginPresentationRecordPlayback_Implementation(Record, Token);
}

void UBattleHUDSelectionWidget::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& Token)
{
	if (bSharedTransferActive && Token == SharedTransferToken)
	{
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

	// Authoritative pending selection is a fail-closed input boundary. Candidate
	// identities and confirmation remain gated by the displayed Presentation
	// revision, but an unreadable pending request must never fall through to the
	// ordinary card-play Confirm path.
	if (ViewModel->HasAuthoritativePendingCardSelection())
	{
		if (ViewModel->HasPendingCardSelection())
		{
			if (ViewModel->ConfirmPendingCardSelection())
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

	ConfirmSelectedCard();
}

void UBattleHUDSelectionWidget::HandleSelectionAwareCancelClicked()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	// Same fail-closed rule as Confirm: while Gameplay owns a pending selection,
	// Cancel may operate only through the readable selection request. It must not
	// cancel a normal card-play selection merely because Presentation is one edge
	// behind the authoritative request.
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
		}
	}
}

void UBattleHUDSelectionWidget::ClearSharedSelectionControlsAfterSubmit()
{
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
	SharedTransferMovingCard = MovingCard;
	SharedTransferToken = Token;
	SharedTransferElapsedSeconds = 0.0f;
	SharedTransferStartTranslation = SharedSelectionHandFallbackTranslation;
	SharedTransferEndTranslation = SharedSelectionDrawPileFallbackTranslation;
	bSharedTransferGeometryInitialized = false;
	bSharedTransferActive = true;

	MovingCard->SetRenderTranslation(SharedSelectionHandFallbackTranslation);
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
	auto ResolveAnchorTranslation = [MovingCard](UWidget* Anchor, const FVector2D& Fallback) -> FVector2D
	{
		if (!IsValid(Anchor))
		{
			return Fallback;
		}
		const FGeometry& AnchorGeometry = Anchor->GetCachedGeometry();
		const FGeometry& CardGeometry = MovingCard->GetCachedGeometry();
		if (AnchorGeometry.GetLocalSize().SizeSquared() <= 0.0f
			|| CardGeometry.GetLocalSize().SizeSquared() <= 0.0f)
		{
			return Fallback;
		}
		const FVector2D AnchorAbsolute = AnchorGeometry.LocalToAbsolute(
			AnchorGeometry.GetLocalSize() * 0.5f);
		const FVector2D CardAbsolute = CardGeometry.LocalToAbsolute(
			CardGeometry.GetLocalSize() * 0.5f);
		return FVector2D(CardGeometry.AbsoluteToLocal(AnchorAbsolute))
			- FVector2D(CardGeometry.AbsoluteToLocal(CardAbsolute));
	};

	if (!bSharedTransferGeometryInitialized)
	{
		SharedTransferStartTranslation = ResolveAnchorTranslation(
			SharedTransferHistoricalCard.Get(),
			SharedSelectionHandFallbackTranslation);
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
	MovingCard->SetRenderOpacity(FMath::Lerp(1.0f, 0.15f, EasedAlpha));
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
		SharedTransferMovingCard->RemoveFromParent();
	}

	const FPresentationPlaybackToken CompletedToken = SharedTransferToken;
	ResetSharedHandToDrawPileState();
	ResetNativePresentationOwnership();
	NotifyPresentationFinished(CompletedToken);
}

void UBattleHUDSelectionWidget::CancelSharedHandToDrawPilePresentation()
{
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
