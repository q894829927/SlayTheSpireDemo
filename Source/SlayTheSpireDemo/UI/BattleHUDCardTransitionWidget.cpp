#include "BattleHUDCardTransitionWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"

namespace
{
	constexpr float NativeCardTransitionDurationSeconds = 0.5f;
	const FVector2D NativeTransitionHandFallback(-300.0f, 120.0f);
	const FVector2D NativeTransitionDrawPileFallback(-420.0f, 90.0f);
	const FVector2D NativeTransitionDiscardFallback(420.0f, 90.0f);
}

void UBattleHUDCardTransitionWidget::NativeDestruct()
{
	ClearNativeCardTransitionFinishTimer();
	if (!ActiveNativeCardTransitions.IsEmpty())
	{
		const int32 RuntimeId = ActiveNativeCardTransitions[0].RuntimeId;
		CleanupNativeCardTransitionsOnDestruct();
		OnNativeCardTransitionEnded(RuntimeId, true);
	}
	if (HasActiveNativePresentation()
		&& NativeCardTransitionToken.IsValid()
		&& GetActiveNativePresentationToken() == NativeCardTransitionToken)
	{
		ResetNativePresentationOwnership();
	}
	ResetNativeCardTransitionState();
	Super::NativeDestruct();
}

void UBattleHUDCardTransitionWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateNativeCardTransitions(InDeltaTime);
}

bool UBattleHUDCardTransitionWidget::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	if (Record.Type == EBattlePresentationRecordType::CardZoneChanged
		&& Record.CardZoneChanged.FromZone == ECardZone::Hand
		&& (Record.CardZoneChanged.ToZone == ECardZone::DrawPile
			|| Record.CardZoneChanged.ToZone == ECardZone::DiscardPile
			|| Record.CardZoneChanged.ToZone == ECardZone::ExhaustPile))
	{
		return BeginNativeOutgoingCardTransition(Record, Token);
	}

	return Super::BeginPresentationRecordPlayback_Implementation(Record, Token);
}

void UBattleHUDCardTransitionWidget::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& Token)
{
	if (!ActiveNativeCardTransitions.IsEmpty()
		&& Token == NativeCardTransitionToken
		&& HasActiveNativePresentation()
		&& GetActiveNativePresentationToken() == Token)
	{
		const int32 RuntimeId = ActiveNativeCardTransitions[0].RuntimeId;
		ClearNativeCardTransitionFinishTimer();
		CancelNativeCardTransitionVisuals();
		ResetNativeCardTransitionState();
		OnNativeCardTransitionEnded(RuntimeId, true);

		// Preserve the existing Native cancellation contract (including retained
		// CardPlayed cleanup) while the exact base token is still active.
		Super::CancelPresentationRecordPlayback_Implementation(Token);
		return;
	}

	Super::CancelPresentationRecordPlayback_Implementation(Token);
}

bool UBattleHUDCardTransitionWidget::BeginNativeOutgoingCardTransition(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	const FCardZoneChangedPresentationPayload& Payload = Record.CardZoneChanged;
	if (!IsNativeRecordTokenConsistent(Record, Token)
		|| Payload.FromZone != ECardZone::Hand
		|| HasActiveNativePresentation()
		|| NativeCardTransitionFinishTimer.IsValid()
		|| !ActiveNativeCardTransitions.IsEmpty()
		|| !IsValid(ViewModel)
		|| !IsValid(HB_Hand)
		|| !IsValid(OV_PlayArea)
		|| CardWidgetClass == nullptr
		|| !IsNativeCardSnapshotValid(Payload.Card))
	{
		return false;
	}

	UBattleCardWidget* HistoricalHandCard = nullptr;
	if (!FindExactHistoricalHandCard(
		Payload.Card,
		Payload.FromIndex,
		HistoricalHandCard))
	{
		return false;
	}

	FNativeCardTransitionInstance Instance;
	Instance.RuntimeId = Payload.Card.RuntimeId;
	Instance.FromZone = Payload.FromZone;
	Instance.ToZone = Payload.ToZone;
	Instance.HistoricalHandVisual = HistoricalHandCard;
	Instance.HistoricalHandVisibility = HistoricalHandCard->GetVisibility();
	Instance.FallbackStartTranslation = HistoricalHandCard->GetRenderTransform().Translation;
	if (Instance.FallbackStartTranslation.IsNearlyZero())
	{
		Instance.FallbackStartTranslation = NativeTransitionHandFallback;
	}

	// Capture the actual displayed source center before any hide/reparent. This
	// preserves confirmed-position compatibility for every migrated Hand source,
	// not only DrawPile, and avoids reconstructing visible continuity by CardId.
	const FGeometry& HistoricalGeometry = HistoricalHandCard->GetCachedGeometry();
	if (HistoricalGeometry.GetLocalSize().SizeSquared() > 0.0f)
	{
		Instance.AbsoluteSourceCenter = HistoricalGeometry.LocalToAbsolute(
			HistoricalGeometry.GetLocalSize() * 0.5f);
		Instance.bHasAbsoluteSourceCenter = true;
	}

	if (!ValidateNativeCardTransitionDestination(Payload, Instance))
	{
		return false;
	}

	FCardPresentationOwnershipEntry OwnershipEntry;
	const bool bHasExplicitOwner = ViewModel->TryGetCardPresentationOwnershipEntry(
		Payload.Card.RuntimeId,
		OwnershipEntry);
	Instance.SourceOwner = bHasExplicitOwner
		? OwnershipEntry.Owner
		: ECardPresentationOwner::Hand;

	if (Instance.SourceOwner == ECardPresentationOwner::Transition
		|| Instance.SourceOwner == ECardPresentationOwner::ConsumedPendingReducer)
	{
		// A committed child already owned by Transition/Consumed must not replay.
		return false;
	}

	FVector2D CompatibilityCenter = FVector2D::ZeroVector;
	if (TryGetCardTransitionCompatibilitySourceCenter(
		Payload.Card.RuntimeId,
		CompatibilityCenter))
	{
		Instance.AbsoluteSourceCenter = CompatibilityCenter;
		Instance.bHasAbsoluteSourceCenter = true;
	}

	if (Instance.SourceOwner == ECardPresentationOwner::SelectionArea)
	{
		if (!bHasExplicitOwner
			|| OwnershipEntry.Phase != ESelectionPresentationVisualPhase::Confirmed
			|| OwnershipEntry.SelectionGeneration <= 0)
		{
			return false;
		}

		UBattleCardWidget* SelectionVisual = nullptr;
		FVector2D SelectionCenter = FVector2D::ZeroVector;
		bool bHasSelectionCenter = false;
		if (!ResolveSelectionAreaTransitionVisual(
			Payload.Card,
			SelectionVisual,
			SelectionCenter,
			bHasSelectionCenter))
		{
			return false;
		}
		Instance.SelectionGeneration = OwnershipEntry.SelectionGeneration;
		Instance.MovingVisual = SelectionVisual;
		Instance.bCreatedMovingVisual = false;
		Instance.StartAnchor = nullptr;
		Instance.SourceRenderTransform = SelectionVisual->GetRenderTransform();
		Instance.SourceRenderOpacity = SelectionVisual->GetRenderOpacity();
		Instance.SourceVisibility = SelectionVisual->GetVisibility();
		Instance.bSourceWasEnabled = SelectionVisual->GetIsEnabled();
		Instance.bSelectionAreaVisualStateCaptured = true;
		if (bHasSelectionCenter)
		{
			Instance.AbsoluteSourceCenter = SelectionCenter;
			Instance.bHasAbsoluteSourceCenter = true;
		}
	}
	else if (Instance.SourceOwner == ECardPresentationOwner::Hand)
	{
		UBattleCardWidget* MovingVisual = CreateNativePresentationCard(Payload.Card);
		if (!IsValid(MovingVisual))
		{
			return false;
		}
		Instance.MovingVisual = MovingVisual;
		Instance.bCreatedMovingVisual = true;
		Instance.StartAnchor = HistoricalHandCard;
	}
	else
	{
		return false;
	}

	if (!CommitNativePresentationOwnership(Record.Type, Token))
	{
		return false;
	}

	NativeCardTransitionToken = Token;
	ActiveNativeCardTransitions.Add(MoveTemp(Instance));
	FNativeCardTransitionInstance& Active = ActiveNativeCardTransitions[0];
	if (!AttachTransitionVisualToPlayArea(Active.MovingVisual.Get()))
	{
		if (Active.SourceOwner == ECardPresentationOwner::SelectionArea)
		{
			RestoreSelectionAreaTransitionVisual(Active);
		}
		ResetNativeCardTransitionState();
		AbortNativePresentationStart();
		return false;
	}

	// Resolve geometry only after the exact moving object is attached to the
	// stable motion surface. A captured absolute source center keeps continuity
	// across the reparent even when the source lived in SelectionArea.
	Active.StartTranslation = ResolveTransitionAnchorTranslation(
		Active,
		Active.StartAnchor,
		Active.FallbackStartTranslation);
	Active.EndTranslation = Active.bKeepDestinationAtSource
		? Active.StartTranslation
		: ResolveTransitionAnchorTranslation(
			Active,
			Active.EndAnchor,
			Active.FallbackEndTranslation);
	Active.bGeometryInitialized = true;
	Active.MovingVisual->SetRenderTranslation(Active.StartTranslation);
	Active.MovingVisual->SetRenderScale(FVector2D(Active.StartScale, Active.StartScale));
	Active.MovingVisual->SetRenderOpacity(Active.StartOpacity);
	Active.MovingVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
	Active.MovingVisual->SetIsEnabled(false);
	HistoricalHandCard->SetVisibility(ESlateVisibility::Hidden);

	if (!StartNativeCardTransitionFinishTimer(NativeCardTransitionDurationSeconds))
	{
		RollbackPreparedSelectionAreaTransition();
		if (Active.SourceOwner == ECardPresentationOwner::Hand
			&& IsValid(Active.MovingVisual))
		{
			Active.MovingVisual->RemoveFromParent();
		}
		HistoricalHandCard->SetVisibility(Active.HistoricalHandVisibility);
		ResetNativeCardTransitionState();
		AbortNativePresentationStart();
		return false;
	}

	if (Active.SourceOwner == ECardPresentationOwner::SelectionArea)
	{
		if (!ViewModel->TryTransferCardPresentationOwnership(
			Active.SelectionGeneration,
			Active.RuntimeId,
			ECardPresentationOwner::SelectionArea,
			ECardPresentationOwner::Transition))
		{
			ClearNativeCardTransitionFinishTimer();
			RollbackPreparedSelectionAreaTransition();
			HistoricalHandCard->SetVisibility(Active.HistoricalHandVisibility);
			ResetNativeCardTransitionState();
			AbortNativePresentationStart();
			return false;
		}
		Active.bSelectionAreaVisualTransferred = true;
	}

	OnNativeCardTransitionAccepted(Active.RuntimeId);
	return true;
}

bool UBattleHUDCardTransitionWidget::ValidateNativeCardTransitionDestination(
	const FCardZoneChangedPresentationPayload& Payload,
	FNativeCardTransitionInstance& OutInstance) const
{
	if (!IsValid(ViewModel))
	{
		return false;
	}

	switch (Payload.ToZone)
	{
	case ECardZone::DrawPile:
		if (!IsValid(Txt_DrawCount) || Payload.ToIndex != ViewModel->DrawCount)
		{
			return false;
		}
		OutInstance.EndAnchor = Txt_DrawCount;
		OutInstance.FallbackEndTranslation = NativeTransitionDrawPileFallback;
		OutInstance.EndScale = 0.72f;
		OutInstance.EndOpacity = 0.0f;
		OutInstance.OpacityFadeStartAlpha = 0.85f;
		return true;

	case ECardZone::DiscardPile:
		if (!IsValid(Txt_DiscardCount) || Payload.ToIndex != ViewModel->DiscardCount)
		{
			return false;
		}
		OutInstance.EndAnchor = Txt_DiscardCount;
		OutInstance.FallbackEndTranslation = NativeTransitionDiscardFallback;
		OutInstance.EndScale = 0.72f;
		OutInstance.EndOpacity = 0.15f;
		return true;

	case ECardZone::ExhaustPile:
		if (Payload.ToIndex != ViewModel->ExhaustCount)
		{
			return false;
		}
		OutInstance.bKeepDestinationAtSource = true;
		OutInstance.EndScale = 1.0f;
		OutInstance.EndOpacity = 0.0f;
		return true;

	default:
		return false;
	}
}

bool UBattleHUDCardTransitionWidget::ResolveSelectionAreaTransitionVisual(
	const FPresentationCardSnapshot& Snapshot,
	UBattleCardWidget*& OutVisual,
	FVector2D& OutAbsoluteCenter,
	bool& bOutHasAbsoluteCenter) const
{
	OutVisual = nullptr;
	OutAbsoluteCenter = FVector2D::ZeroVector;
	bOutHasAbsoluteCenter = false;
	UOverlay* Host = GetCardTransitionSelectionAreaHost();
	if (!IsValid(Host))
	{
		return false;
	}

	int32 MatchCount = 0;
	for (int32 Index = 0; Index < Host->GetChildrenCount(); ++Index)
	{
		UBattleCardWidget* Card = Cast<UBattleCardWidget>(Host->GetChildAt(Index));
		if (!IsValid(Card) || Card->GetRuntimeId() != Snapshot.RuntimeId)
		{
			continue;
		}
		++MatchCount;
		OutVisual = Card;
	}

	if (MatchCount != 1
		|| !IsValid(OutVisual)
		|| OutVisual->GetParent() != Host
		|| OutVisual->GetVisibility() == ESlateVisibility::Collapsed
		|| OutVisual->GetVisibility() == ESlateVisibility::Hidden
		|| !DoesNativeCardViewMatchSnapshot(OutVisual->GetCardView(), Snapshot))
	{
		OutVisual = nullptr;
		return false;
	}

	const FGeometry& Geometry = OutVisual->GetCachedGeometry();
	if (Geometry.GetLocalSize().SizeSquared() > 0.0f)
	{
		OutAbsoluteCenter = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
		bOutHasAbsoluteCenter = true;
	}
	return true;
}

bool UBattleHUDCardTransitionWidget::RestoreSelectionAreaTransitionVisual(
	FNativeCardTransitionInstance& Instance)
{
	UBattleCardWidget* Visual = Instance.MovingVisual.Get();
	UOverlay* Host = GetCardTransitionSelectionAreaHost();
	if (!IsValid(Visual) || !IsValid(Host))
	{
		return false;
	}
	Visual->RemoveFromParent();
	UOverlaySlot* Slot = Host->AddChildToOverlay(Visual);
	if (!IsValid(Slot))
	{
		return false;
	}
	Slot->SetHorizontalAlignment(HAlign_Center);
	Slot->SetVerticalAlignment(VAlign_Center);
	if (Instance.bSelectionAreaVisualStateCaptured)
	{
		Visual->SetRenderTransform(Instance.SourceRenderTransform);
		Visual->SetRenderOpacity(Instance.SourceRenderOpacity);
		Visual->SetVisibility(Instance.SourceVisibility);
		Visual->SetIsEnabled(Instance.bSourceWasEnabled);
	}
	else
	{
		NormalizeNativeCardTransform(Visual);
	}
	return true;
}

bool UBattleHUDCardTransitionWidget::AttachTransitionVisualToPlayArea(
	UBattleCardWidget* Visual)
{
	if (!IsValid(Visual) || !IsValid(OV_PlayArea))
	{
		return false;
	}
	Visual->RemoveFromParent();
	UOverlaySlot* Slot = OV_PlayArea->AddChildToOverlay(Visual);
	if (!IsValid(Slot))
	{
		return false;
	}
	Slot->SetHorizontalAlignment(HAlign_Center);
	Slot->SetVerticalAlignment(VAlign_Center);
	return true;
}

FVector2D UBattleHUDCardTransitionWidget::ResolveTransitionAnchorTranslation(
	const FNativeCardTransitionInstance& Instance,
	UWidget* Anchor,
	const FVector2D& Fallback) const
{
	if (!IsValid(OV_PlayArea))
	{
		return Fallback;
	}

	const FGeometry& AreaGeometry = OV_PlayArea->GetCachedGeometry();
	if (AreaGeometry.GetLocalSize().SizeSquared() <= 0.0f)
	{
		return Fallback;
	}

	if (Anchor == Instance.StartAnchor
		&& Instance.bHasAbsoluteSourceCenter)
	{
		return FVector2D(AreaGeometry.AbsoluteToLocal(Instance.AbsoluteSourceCenter))
			- FVector2D(AreaGeometry.GetLocalSize() * 0.5f);
	}

	if (!IsValid(Anchor))
	{
		return Fallback;
	}
	const FGeometry& AnchorGeometry = Anchor->GetCachedGeometry();
	if (AnchorGeometry.GetLocalSize().SizeSquared() <= 0.0f)
	{
		return Fallback;
	}
	const FVector2D AnchorAbsolute = AnchorGeometry.LocalToAbsolute(
		AnchorGeometry.GetLocalSize() * 0.5f);
	return FVector2D(AreaGeometry.AbsoluteToLocal(AnchorAbsolute))
		- FVector2D(AreaGeometry.GetLocalSize() * 0.5f);
}

void UBattleHUDCardTransitionWidget::UpdateNativeCardTransitions(float DeltaSeconds)
{
	if (ActiveNativeCardTransitions.IsEmpty()
		|| !HasActiveNativePresentation())
	{
		return;
	}

	for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
	{
		UBattleCardWidget* MovingVisual = Instance.MovingVisual.Get();
		if (!IsValid(MovingVisual))
		{
			continue;
		}

		if (!Instance.bGeometryInitialized)
		{
			Instance.StartTranslation = ResolveTransitionAnchorTranslation(
				Instance,
				Instance.StartAnchor,
				Instance.FallbackStartTranslation);
			Instance.EndTranslation = Instance.bKeepDestinationAtSource
				? Instance.StartTranslation
				: ResolveTransitionAnchorTranslation(
					Instance,
					Instance.EndAnchor,
					Instance.FallbackEndTranslation);
			MovingVisual->SetRenderTranslation(Instance.StartTranslation);
			Instance.bGeometryInitialized = true;
		}

		Instance.ElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
		const float LinearAlpha = FMath::Clamp(
			Instance.ElapsedSeconds / NativeCardTransitionDurationSeconds,
			0.0f,
			1.0f);
		const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, LinearAlpha, 3.0f);
		MovingVisual->SetRenderTranslation(FMath::Lerp(
			Instance.StartTranslation,
			Instance.EndTranslation,
			EasedAlpha));
		const float Scale = FMath::Lerp(
			Instance.StartScale,
			Instance.EndScale,
			EasedAlpha);
		MovingVisual->SetRenderScale(FVector2D(Scale, Scale));

		float OpacityAlpha = EasedAlpha;
		if (Instance.OpacityFadeStartAlpha > 0.0f)
		{
			OpacityAlpha = FMath::Clamp(
				(LinearAlpha - Instance.OpacityFadeStartAlpha)
					/ (1.0f - Instance.OpacityFadeStartAlpha),
				0.0f,
				1.0f);
		}
		MovingVisual->SetRenderOpacity(FMath::Lerp(
			Instance.StartOpacity,
			Instance.EndOpacity,
			OpacityAlpha));
	}
}

bool UBattleHUDCardTransitionWidget::StartNativeCardTransitionFinishTimer(
	float DurationSeconds)
{
	if (ActiveNativeCardTransitions.IsEmpty()
		|| !HasActiveNativePresentation()
		|| NativeCardTransitionFinishTimer.IsValid()
		|| DurationSeconds <= 0.0f)
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const FPresentationPlaybackToken ExpectedToken = NativeCardTransitionToken;
	const TWeakObjectPtr<UBattleHUDCardTransitionWidget> WeakThis(this);
	FTimerDelegate FinishDelegate = FTimerDelegate::CreateLambda(
		[WeakThis, ExpectedToken]()
		{
			if (UBattleHUDCardTransitionWidget* Widget = WeakThis.Get())
			{
				Widget->FinishNativeCardTransition(ExpectedToken);
			}
		});
	World->GetTimerManager().SetTimer(
		NativeCardTransitionFinishTimer,
		FinishDelegate,
		DurationSeconds,
		false);
	return NativeCardTransitionFinishTimer.IsValid();
}

void UBattleHUDCardTransitionWidget::FinishNativeCardTransition(
	const FPresentationPlaybackToken& ExpectedToken)
{
	if (ActiveNativeCardTransitions.IsEmpty()
		|| ExpectedToken != NativeCardTransitionToken
		|| !HasActiveNativePresentation()
		|| GetActiveNativePresentationToken() != ExpectedToken)
	{
		return;
	}

	const int32 RuntimeId = ActiveNativeCardTransitions[0].RuntimeId;
	ClearNativeCardTransitionFinishTimer();
	FinishNativeCardTransitionVisuals();
	ResetNativeCardTransitionState();
	OnNativeCardTransitionEnded(RuntimeId, false);

	// Reuse the base exact-token completion/Notify path after the generic child
	// has fulfilled its own visual cleanup obligation.
	FinishNativePresentation(ExpectedToken);
}

void UBattleHUDCardTransitionWidget::FinishNativeCardTransitionVisuals()
{
	for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
	{
		if (UBattleCardWidget* Historical = Instance.HistoricalHandVisual.Get())
		{
			Historical->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (IsValid(Instance.MovingVisual))
		{
			Instance.MovingVisual->RemoveFromParent();
		}
	}
}

void UBattleHUDCardTransitionWidget::CancelNativeCardTransitionVisuals()
{
	for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
	{
		if (UBattleCardWidget* Historical = Instance.HistoricalHandVisual.Get())
		{
			if (Instance.SourceOwner == ECardPresentationOwner::Hand)
			{
				Historical->SetVisibility(Instance.HistoricalHandVisibility);
			}
			else
			{
				Historical->SetVisibility(ESlateVisibility::Hidden);
			}
		}

		if (IsValid(Instance.MovingVisual))
		{
			Instance.MovingVisual->RemoveFromParent();
		}
	}
}

void UBattleHUDCardTransitionWidget::RollbackPreparedSelectionAreaTransition()
{
	for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
	{
		if (Instance.SourceOwner != ECardPresentationOwner::SelectionArea
			|| Instance.bSelectionAreaVisualTransferred
			|| !IsValid(Instance.MovingVisual))
		{
			continue;
		}
		RestoreSelectionAreaTransitionVisual(Instance);
	}
}

void UBattleHUDCardTransitionWidget::CleanupNativeCardTransitionsOnDestruct()
{
	for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
	{
		if (Instance.SourceOwner == ECardPresentationOwner::Hand)
		{
			if (UBattleCardWidget* Historical = Instance.HistoricalHandVisual.Get())
			{
				Historical->SetVisibility(Instance.HistoricalHandVisibility);
			}
		}
		if (IsValid(Instance.MovingVisual))
		{
			Instance.MovingVisual->RemoveFromParent();
		}
	}
}

void UBattleHUDCardTransitionWidget::ClearNativeCardTransitionFinishTimer()
{
	if (!NativeCardTransitionFinishTimer.IsValid())
	{
		return;
	}
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(NativeCardTransitionFinishTimer);
	}
	NativeCardTransitionFinishTimer.Invalidate();
}

void UBattleHUDCardTransitionWidget::ResetNativeCardTransitionState()
{
	ActiveNativeCardTransitions.Reset();
	NativeCardTransitionToken = FPresentationPlaybackToken{};
}
