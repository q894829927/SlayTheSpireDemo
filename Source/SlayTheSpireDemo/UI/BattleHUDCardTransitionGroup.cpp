#include "BattleHUDCardTransitionWidget.h"

#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

namespace
{
	constexpr float NativeGroupCardTransitionDurationSeconds = 0.5f;
	const FVector2D NativeGroupTransitionHandFallback(-300.0f, 120.0f);
	const FVector2D NativeGroupTransitionDrawPileFallback(-420.0f, 90.0f);
	const FVector2D NativeGroupTransitionDiscardFallback(420.0f, 90.0f);
}

bool UBattleHUDCardTransitionWidget::BeginPresentationGroupPlayback(
	const TArray<FPresentationRecord>& Records,
	const FPresentationGroupTag& Group,
	const FPresentationPlaybackToken& Token)
{
	if (Group.Kind == EPresentationGroupKind::SelectionDestination)
	{
		return BeginNativeOutgoingCardTransitionGroup(Records, Group, Token);
	}
	return Super::BeginPresentationGroupPlayback(Records, Group, Token);
}

void UBattleHUDCardTransitionWidget::CancelPresentationGroupPlayback(
	const FPresentationGroupTag& Group,
	const FPresentationPlaybackToken& Token)
{
	if (Group.Kind == EPresentationGroupKind::SelectionDestination
		&& Token.UnitKind == EPresentationPlaybackUnitKind::Group
		&& Token == NativeCardTransitionToken
		&& HasActiveNativePresentation()
		&& GetActiveNativePresentationToken() == Token)
	{
		const int32 RepresentativeRuntimeId = ActiveNativeCardTransitions.IsEmpty()
			? INDEX_NONE
			: ActiveNativeCardTransitions[0].RuntimeId;
		ClearNativeCardTransitionFinishTimer();
		CancelNativeCardTransitionVisuals();
		ResetNativeCardTransitionState();
		if (RepresentativeRuntimeId != INDEX_NONE)
		{
			OnNativeCardTransitionEnded(RepresentativeRuntimeId, true);
		}
		ResetNativePresentationOwnership();
	}
	Super::CancelPresentationGroupPlayback(Group, Token);
}

bool UBattleHUDCardTransitionWidget::BeginNativeOutgoingCardTransitionGroup(
	const TArray<FPresentationRecord>& Records,
	const FPresentationGroupTag& Group,
	const FPresentationPlaybackToken& Token)
{
	if (!Group.IsValid()
		|| Group.Kind != EPresentationGroupKind::SelectionDestination
		|| Group.ExpectedMemberCount <= 1
		|| Records.Num() != Group.ExpectedMemberCount
		|| !Token.IsValid()
		|| Token.UnitKind != EPresentationPlaybackUnitKind::Group
		|| Token.GroupId != Group.GroupId
		|| HasActiveNativePresentation()
		|| NativeCardTransitionFinishTimer.IsValid()
		|| !ActiveNativeCardTransitions.IsEmpty()
		|| !IsValid(ViewModel)
		|| !IsValid(HB_Hand)
		|| !IsValid(OV_PlayArea)
		|| !IsValid(GetCardTransitionSelectionAreaHost())
		|| CardWidgetClass == nullptr)
	{
		return false;
	}

	TArray<FNativeCardTransitionInstance> PreparedInstances;
	PreparedInstances.Reserve(Records.Num());
	TArray<int32> RuntimeIds;
	RuntimeIds.Reserve(Records.Num());
	TSet<int32> UniqueRuntimeIds;
	int64 SelectionGeneration = 0;
	int64 PreviousSequence = 0;

	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		const FPresentationRecord& Record = Records[Index];
		if (Record.BattleId != Token.BattleId
			|| Record.ResolutionId != Token.ResolutionId
			|| Record.Type != EBattlePresentationRecordType::CardZoneChanged
			|| Record.Group.Kind != Group.Kind
			|| Record.Group.GroupId != Group.GroupId
			|| Record.Group.ExpectedMemberCount != Group.ExpectedMemberCount
			|| Record.PresentationSequence <= 0
			|| (Index == 0 && Record.PresentationSequence != Token.PresentationSequence)
			|| (PreviousSequence > 0 && Record.PresentationSequence <= PreviousSequence))
		{
			return false;
		}
		PreviousSequence = Record.PresentationSequence;

		const int32 RuntimeId = Record.CardZoneChanged.Card.RuntimeId;
		if (RuntimeId == INDEX_NONE || UniqueRuntimeIds.Contains(RuntimeId))
		{
			return false;
		}

		FNativeCardTransitionInstance Instance;
		if (!PrepareNativeOutgoingCardTransitionGroupMember(
			Record,
			SelectionGeneration,
			Instance))
		{
			return false;
		}
		UniqueRuntimeIds.Add(RuntimeId);
		RuntimeIds.Add(RuntimeId);
		PreparedInstances.Add(MoveTemp(Instance));
	}

	if (SelectionGeneration <= 0 || RuntimeIds.Num() != Group.ExpectedMemberCount)
	{
		return false;
	}

	// No visible object has moved and no ownership entry has changed before this
	// point. Establish the one Native playback owner, then commit all prepared
	// visuals. Any attach failure restores every child to SelectionArea and lets
	// the Controller use the unchanged G5 sequential path.
	if (!CommitNativePresentationOwnership(
		EBattlePresentationRecordType::CardZoneChanged,
		Token))
	{
		return false;
	}

	NativeCardTransitionToken = Token;
	ActiveNativeCardTransitions = MoveTemp(PreparedInstances);
	for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
	{
		if (!AttachTransitionVisualToPlayArea(Instance.MovingVisual.Get()))
		{
			RestorePreparedGroupSelectionAreaVisuals();
			for (FNativeCardTransitionInstance& RestoreInstance : ActiveNativeCardTransitions)
			{
				if (UBattleCardWidget* Historical = RestoreInstance.HistoricalHandVisual.Get())
				{
					Historical->SetVisibility(RestoreInstance.HistoricalHandVisibility);
				}
			}
			ResetNativeCardTransitionState();
			AbortNativePresentationStart();
			return false;
		}
	}

	for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
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
		Instance.bGeometryInitialized = true;
		Instance.MovingVisual->SetRenderTranslation(Instance.StartTranslation);
		Instance.MovingVisual->SetRenderScale(FVector2D(Instance.StartScale, Instance.StartScale));
		Instance.MovingVisual->SetRenderOpacity(Instance.StartOpacity);
		Instance.MovingVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
		Instance.MovingVisual->SetIsEnabled(false);
		if (UBattleCardWidget* Historical = Instance.HistoricalHandVisual.Get())
		{
			Historical->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (!StartNativeCardTransitionGroupFinishTimer(
		NativeGroupCardTransitionDurationSeconds))
	{
		RestorePreparedGroupSelectionAreaVisuals();
		for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
		{
			if (UBattleCardWidget* Historical = Instance.HistoricalHandVisual.Get())
			{
				Historical->SetVisibility(Instance.HistoricalHandVisibility);
			}
		}
		ResetNativeCardTransitionState();
		AbortNativePresentationStart();
		return false;
	}

	// Mark every prepared child before the one batched publication. The Native
	// surface synchronization callback may run synchronously, but it can only see
	// the complete cohort after the ViewModel's all-or-nothing mutation succeeds.
	for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
	{
		Instance.bSelectionAreaVisualTransferred = true;
	}
	if (!ViewModel->TryTransferCardPresentationOwnershipBatch(
		SelectionGeneration,
		RuntimeIds,
		ECardPresentationOwner::SelectionArea,
		ECardPresentationOwner::Transition))
	{
		for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
		{
			Instance.bSelectionAreaVisualTransferred = false;
		}
		ClearNativeCardTransitionFinishTimer();
		RestorePreparedGroupSelectionAreaVisuals();
		for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
		{
			if (UBattleCardWidget* Historical = Instance.HistoricalHandVisual.Get())
			{
				Historical->SetVisibility(Instance.HistoricalHandVisibility);
			}
		}
		ResetNativeCardTransitionState();
		AbortNativePresentationStart();
		return false;
	}

	if (ActiveNativeCardTransitions.IsEmpty() || NativeCardTransitionToken != Token)
	{
		return false;
	}

	OnNativeCardTransitionAccepted(RuntimeIds[0]);
	return true;
}

bool UBattleHUDCardTransitionWidget::PrepareNativeOutgoingCardTransitionGroupMember(
	const FPresentationRecord& Record,
	int64& InOutSelectionGeneration,
	FNativeCardTransitionInstance& OutInstance) const
{
	const FCardZoneChangedPresentationPayload& Payload = Record.CardZoneChanged;
	if (!IsValid(ViewModel)
		|| Payload.FromZone != ECardZone::Hand
		|| !IsNativeCardSnapshotValid(Payload.Card))
	{
		return false;
	}

	// A future member's committed FromIndex belongs to the chronological reducer
	// cursor after earlier members have been consumed. Group visual preparation
	// runs against the leader's displayed Hand, so current visual lookup is by
	// exact RuntimeId rather than the future committed index.
	const int32 CurrentHandIndex = ViewModel->HandCards.IndexOfByPredicate(
		[RuntimeId = Payload.Card.RuntimeId](const FBattleHUDCardView& Card)
		{
			return Card.RuntimeId == RuntimeId;
		});
	if (CurrentHandIndex == INDEX_NONE)
	{
		return false;
	}

	int32 CurrentRuntimeMatches = 0;
	for (const FBattleHUDCardView& Card : ViewModel->HandCards)
	{
		CurrentRuntimeMatches += Card.RuntimeId == Payload.Card.RuntimeId ? 1 : 0;
	}
	if (CurrentRuntimeMatches != 1)
	{
		return false;
	}

	UBattleCardWidget* HistoricalHandCard = nullptr;
	if (!FindExactHistoricalHandCard(
		Payload.Card,
		CurrentHandIndex,
		HistoricalHandCard))
	{
		return false;
	}

	FCardPresentationOwnershipEntry OwnershipEntry;
	if (!ViewModel->TryGetCardPresentationOwnershipEntry(
		Payload.Card.RuntimeId,
		OwnershipEntry)
		|| OwnershipEntry.Owner != ECardPresentationOwner::SelectionArea
		|| OwnershipEntry.Phase != ESelectionPresentationVisualPhase::Confirmed
		|| OwnershipEntry.SelectionGeneration <= 0)
	{
		return false;
	}
	if (InOutSelectionGeneration == 0)
	{
		InOutSelectionGeneration = OwnershipEntry.SelectionGeneration;
	}
	else if (InOutSelectionGeneration != OwnershipEntry.SelectionGeneration)
	{
		return false;
	}

	OutInstance = FNativeCardTransitionInstance{};
	OutInstance.RuntimeId = Payload.Card.RuntimeId;
	OutInstance.FromZone = Payload.FromZone;
	OutInstance.ToZone = Payload.ToZone;
	OutInstance.SourceOwner = ECardPresentationOwner::SelectionArea;
	OutInstance.SelectionGeneration = OwnershipEntry.SelectionGeneration;
	OutInstance.HistoricalHandVisual = HistoricalHandCard;
	OutInstance.HistoricalHandVisibility = HistoricalHandCard->GetVisibility();
	OutInstance.FallbackStartTranslation = HistoricalHandCard->GetRenderTransform().Translation;
	if (OutInstance.FallbackStartTranslation.IsNearlyZero())
	{
		OutInstance.FallbackStartTranslation = NativeGroupTransitionHandFallback;
	}

	if (!ValidateNativeCardTransitionGroupDestination(Payload, OutInstance))
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

	OutInstance.MovingVisual = SelectionVisual;
	OutInstance.bCreatedMovingVisual = false;
	OutInstance.StartAnchor = nullptr;
	OutInstance.SourceRenderTransform = SelectionVisual->GetRenderTransform();
	OutInstance.SourceRenderOpacity = SelectionVisual->GetRenderOpacity();
	OutInstance.SourceVisibility = SelectionVisual->GetVisibility();
	OutInstance.bSourceWasEnabled = SelectionVisual->GetIsEnabled();
	OutInstance.bSelectionAreaVisualStateCaptured = true;
	if (bHasSelectionCenter)
	{
		OutInstance.AbsoluteSourceCenter = SelectionCenter;
		OutInstance.bHasAbsoluteSourceCenter = true;
	}
	return true;
}

bool UBattleHUDCardTransitionWidget::ValidateNativeCardTransitionGroupDestination(
	const FCardZoneChangedPresentationPayload& Payload,
	FNativeCardTransitionInstance& OutInstance) const
{
	// G2 already dry-runs every member at its exact chronological cursor. Future
	// members may therefore have ToIndex values greater than the leader's current
	// pile count. The visual preflight validates only the concrete destination
	// surface/style and never reinterprets reducer chronology from current counts.
	switch (Payload.ToZone)
	{
	case ECardZone::DrawPile:
		if (!IsValid(Txt_DrawCount))
		{
			return false;
		}
		OutInstance.EndAnchor = Txt_DrawCount;
		OutInstance.FallbackEndTranslation = NativeGroupTransitionDrawPileFallback;
		OutInstance.EndScale = 0.72f;
		OutInstance.EndOpacity = 0.0f;
		OutInstance.OpacityFadeStartAlpha = 0.85f;
		return true;

	case ECardZone::DiscardPile:
		if (!IsValid(Txt_DiscardCount))
		{
			return false;
		}
		OutInstance.EndAnchor = Txt_DiscardCount;
		OutInstance.FallbackEndTranslation = NativeGroupTransitionDiscardFallback;
		OutInstance.EndScale = 0.72f;
		OutInstance.EndOpacity = 0.15f;
		return true;

	case ECardZone::ExhaustPile:
		OutInstance.bKeepDestinationAtSource = true;
		OutInstance.EndScale = 1.0f;
		OutInstance.EndOpacity = 0.0f;
		return true;

	default:
		return false;
	}
}

bool UBattleHUDCardTransitionWidget::StartNativeCardTransitionGroupFinishTimer(
	float DurationSeconds)
{
	if (ActiveNativeCardTransitions.IsEmpty()
		|| !HasActiveNativePresentation()
		|| GetActiveNativePresentationToken().UnitKind != EPresentationPlaybackUnitKind::Group
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
				Widget->FinishNativeCardTransitionGroup(ExpectedToken);
			}
		});
	World->GetTimerManager().SetTimer(
		NativeCardTransitionFinishTimer,
		FinishDelegate,
		DurationSeconds,
		false);
	return NativeCardTransitionFinishTimer.IsValid();
}

void UBattleHUDCardTransitionWidget::FinishNativeCardTransitionGroup(
	const FPresentationPlaybackToken& ExpectedToken)
{
	if (ActiveNativeCardTransitions.IsEmpty()
		|| ExpectedToken != NativeCardTransitionToken
		|| ExpectedToken.UnitKind != EPresentationPlaybackUnitKind::Group
		|| !HasActiveNativePresentation()
		|| GetActiveNativePresentationToken() != ExpectedToken)
	{
		return;
	}

	const int64 SelectionGeneration = ActiveNativeCardTransitions[0].SelectionGeneration;
	const int32 RepresentativeRuntimeId = ActiveNativeCardTransitions[0].RuntimeId;
	TArray<int32> RuntimeIds;
	RuntimeIds.Reserve(ActiveNativeCardTransitions.Num());
	for (const FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
	{
		if (Instance.SelectionGeneration != SelectionGeneration)
		{
			return;
		}
		RuntimeIds.Add(Instance.RuntimeId);
	}

	ClearNativeCardTransitionFinishTimer();
	FinishNativeCardTransitionVisuals();

	// Clear the active child array before publishing Transition -> Consumed. The
	// Selection surface's ownership listener validates live Transition children;
	// after visual completion these children are intentionally no longer live.
	ResetNativeCardTransitionState();
	if (!IsValid(ViewModel)
		|| !ViewModel->TryTransferCardPresentationOwnershipBatch(
			SelectionGeneration,
			RuntimeIds,
			ECardPresentationOwner::Transition,
			ECardPresentationOwner::ConsumedPendingReducer))
	{
		// Do not report successful Group completion. The Controller's independent
		// exact Group timeout will cancel the remaining Native owner and reconcile
		// the ActiveEnvelope FinalSnapshot without changing Gameplay chronology.
		return;
	}

	OnNativeCardTransitionEnded(RepresentativeRuntimeId, false);
	// Visual work is complete and ownership is now reducer-pending. Clear only
	// the local Native visual owner, then use the G6 exact tracked-group callback;
	// the historical G3 Group callback intentionally remains a recovery path.
	ResetNativePresentationOwnership();
	NotifyPresentationGroupFinishedG6(ExpectedToken);
}

void UBattleHUDCardTransitionWidget::RestorePreparedGroupSelectionAreaVisuals()
{
	for (FNativeCardTransitionInstance& Instance : ActiveNativeCardTransitions)
	{
		if (Instance.SourceOwner == ECardPresentationOwner::SelectionArea
			&& !Instance.bSelectionAreaVisualTransferred
			&& IsValid(Instance.MovingVisual))
		{
			RestoreSelectionAreaTransitionVisual(Instance);
		}
	}
}
