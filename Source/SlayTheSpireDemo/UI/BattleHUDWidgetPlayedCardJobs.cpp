#include "BattleHUDWidget.h"
#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "../Presentation/BattlePresentationController.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogBattleCardVisual, Log, All);

bool UBattleHUDWidget::EnsureDetachedCardVFXHost()
{
	if (DetachedCardVFXHost) return true;
	auto* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root) return false;
	int32 Layer = 0;
	for (UWidget* Child : Root->GetAllChildren())
		if (auto* ChildSlot = Cast<UCanvasPanelSlot>(Child->Slot)) Layer = FMath::Max(Layer, ChildSlot->GetZOrder());
	if (Layer == MAX_int32) return false;
	DetachedCardVFXHost = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DetachedCardVFXHost"));
	auto* HostSlot = Root->AddChildToCanvas(DetachedCardVFXHost);
	if (!HostSlot) { DetachedCardVFXHost = nullptr; return false; }
	HostSlot->SetAnchors(FAnchors(0,0,1,1)); HostSlot->SetOffsets(FMargin(0)); HostSlot->SetZOrder(Layer+1);
	DetachedCardVFXHost->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

bool UBattleHUDWidget::BeginNativeCardPlayedPresentation(const FPresentationRecord& Record, const FPresentationPlaybackToken& Token)
{
	FPlayedCardPresentationLifecycleToken Lifecycle;
	if (IsValid(PresentationController))
	{
		if (!PresentationController->TryGetPlayedCardLifecycleForRecord(Record, Lifecycle)) return false;
		if (BeginHostedCardPlayed(Record, Token, Lifecycle)) return true;
		// Preparation may decline only before the formal Controller commit. Recheck
		// after Widget construction in case it replaced the binding synchronously.
		FPlayedCardPresentationLifecycleToken Current;
		if (!PresentationController || !PresentationController->TryGetPlayedCardLifecycleForRecord(Record, Current) || Current != Lifecycle) return false;
	}
	// Sealed Native Blocking renderer also remains the standalone R8 contract.
	const bool bStarted = BeginBlockingFallbackCardPlayed(Record, Token);
	UE_LOG(LogBattleCardVisual, Verbose, TEXT("Blocking fallback Runtime=%d Started=%d Root=%s Play=%s"),
		Record.CardPlayed.Card.RuntimeId, bStarted, *GetCardInputRootGeometry().GetLocalSize().ToString(),
		OV_PlayArea ? *OV_PlayArea->GetCachedGeometry().GetLocalSize().ToString() : TEXT("missing"));
	if (bStarted) BlockingFallbackPlayedLifecycle = Lifecycle;
	return bStarted;
}

bool UBattleHUDWidget::BeginHostedCardPlayed(const FPresentationRecord& Record, const FPresentationPlaybackToken& Token,
	const FPlayedCardPresentationLifecycleToken& Lifecycle)
{
	UBattleCardWidget* Source = nullptr;
	if (!Lifecycle.IsValid() || PlayedCardVisualJobs.Num() >= 32 || NextCardVisualGeneration <= 0
		|| NextCardVisualGeneration == MAX_int64 || !ViewModel || !OV_PlayArea
		|| !FindExactHistoricalHandCard(Record.CardPlayed.Card, Record.CardPlayed.HandIndexBefore, Source)
		|| !EnsureDetachedCardVFXHost()) return false;
	const auto& RootGeometry = GetCardInputRootGeometry();
	const auto& PlayGeometry = OV_PlayArea->GetCachedGeometry();
	if (RootGeometry.GetLocalSize().IsNearlyZero() || PlayGeometry.GetLocalSize().IsNearlyZero()) return false;
	const auto Origin = ResolveHostedCardPlayOrigin(Record, Lifecycle);
	if (!Origin.IsSet() || !FMath::IsFinite(Origin->Angle) || Origin->Size.X <= 0 || Origin->Size.Y <= 0) return false;
	FDetachedCardVisualToken VisualToken;
	if (!PreparePlayedCardVisualJob(Record,Token,Lifecycle,Origin.GetValue(),Source,VisualToken)) return false;
	const int32 Index = PlayedCardVisualJobs.IndexOfByPredicate([&](const auto& Job) { return Job.Token == VisualToken; });
	if (Index == INDEX_NONE) return false;
	FPlayedCardPresentationLifecycleToken Current;
	if (!PresentationController || !PresentationController->TryGetPlayedCardLifecycleForRecord(Record,Current)
		|| Current != Lifecycle || Source->GetParent() != HB_Hand.Get() || !CommitNativePresentationOwnership(Record.Type,Token))
	{ RetirePlayedCardVisualJob(Index); return false; }
	PlayedCardVisualJobs[Index].Phase = ECardVisualPhase::EnteringPlayArea;
	PlayedCardVisualJobs[Index].Widget->SetVisibility(bPlayedCardVisualsSelectionHidden ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	ActivePlayedCardVisualToken = VisualToken;
	ActiveNativeCardPresentationKind = ENativeCardPresentationKind::CardPlayed;
	ActiveNativeHistoricalHandCardWidget = Source; ActiveNativeHistoricalHandVisibility = Source->GetVisibility();
	Source->SetVisibility(ESlateVisibility::Hidden);
	UpdatePlayedCardVisualJobs(0);
	if (!StartNativePresentationFinishTimer(0.5f))
	{
		const int32 FailedIndex = PlayedCardVisualJobs.IndexOfByPredicate([&](const auto& Job) { return Job.Token == VisualToken; });
		RetirePlayedCardVisualJob(FailedIndex,true);
		ResetNativeCardRecordState(); AbortNativePresentationStart(); return false;
	}
	if (Record.CardPlayed.Card.CardType == ECardType::Attack)
		PlayNativeCombatantAnimation(Record.CardPlayed.SourcePresentationId, EBattleHUDCombatantAnimation::Attack);
	UE_LOG(LogBattleCardVisual, Verbose, TEXT("Hosted arrival Runtime=%d Life=%lld Visual=%lld"),
		Lifecycle.RuntimeId, Lifecycle.LocalLifecycleGeneration, VisualToken.LocalVisualGeneration);
	SubmittedCardVisual.Reset(); return true;
}

bool UBattleHUDWidget::BeginNativePlayAreaToDestinationPresentation(const FPresentationRecord& Record, const FPresentationPlaybackToken& Token)
{
	FPlayedCardPresentationLifecycleToken Lifecycle;
	if (IsValid(PresentationController))
	{
		if (!PresentationController->TryGetPlayedCardLifecycleForRecord(Record,Lifecycle)) return false;
		for (const auto& Job : PlayedCardVisualJobs)
			if (Job.Token.Lifecycle == Lifecycle) return BeginHostedCardDestination(Record,Token,Lifecycle);
		if (!NativeBlockingFallbackPlayedCardWidget.IsValid() || BlockingFallbackPlayedLifecycle != Lifecycle) return false;
	}
	return BeginBlockingFallbackPlayAreaDestination(Record,Token);
}

bool UBattleHUDWidget::PreparePlayedCardVisualJob(const FPresentationRecord& Record, const FPresentationPlaybackToken& BlockingToken,
	const FPlayedCardPresentationLifecycleToken& Lifecycle, const FCardPlayVisualOrigin& Origin,
	UBattleCardWidget* Source, FDetachedCardVisualToken& OutToken, bool bDetachedArrival)
{
	OutToken = {};
	if (!Lifecycle.IsValid() || !IsValid(ViewModel) || ViewModel->BattleId != Lifecycle.BattleId
		|| !PresentationController || !PresentationController->IsCurrentPresentationSession(Lifecycle.SessionToken)
		|| Record.Type != EBattlePresentationRecordType::CardPlayed || Record.BattleId != Lifecycle.BattleId
		|| Record.ResolutionId != Lifecycle.SourceResolutionId || Record.PresentationSequence != Lifecycle.CardPlayedPresentationSequence
		|| Record.CardPlayed.Card.RuntimeId != Lifecycle.RuntimeId || Record.CardPlayed.Card.CardId != Lifecycle.CardId
		|| (!bDetachedArrival && !IsNativeRecordTokenConsistent(Record,BlockingToken))
		|| (bDetachedArrival && BlockingToken.IsValid()) || !IsNativeCardSnapshotValid(Record.CardPlayed.Card)
		|| PlayedCardVisualJobs.Num() >= 32 || NextCardVisualGeneration <= 0 || NextCardVisualGeneration == MAX_int64
		|| !FMath::IsFinite(Origin.Center.X) || !FMath::IsFinite(Origin.Center.Y) || !FMath::IsFinite(Origin.Angle)
		|| !FMath::IsFinite(Origin.Size.X) || !FMath::IsFinite(Origin.Size.Y) || Origin.Size.X <= 0 || Origin.Size.Y <= 0
		|| !EnsureDetachedCardVFXHost()) return false;
	for (const auto& Job : PlayedCardVisualJobs) if (Job.Token.Lifecycle == Lifecycle) return false;
	const TStrongObjectPtr<UBattleCardWidget> KeepSource(Source);
	UBattleCardWidget* Card = CreateNativePresentationCard(Record.CardPlayed.Card);
	if (!Card) return false;
	Card->ForceLayoutPrepass();
	FNativePlayedCardVisualJob Job; Job.Token.Lifecycle = Lifecycle; Job.Token.LocalVisualGeneration = NextCardVisualGeneration++;
	Job.BlockingToken = BlockingToken; Job.Widget = Card; Job.ViewModel = ViewModel; Job.SurfaceGeneration = HandSurfaceGeneration;
	Job.bDetachedArrival = bDetachedArrival;
	Job.Origin = Origin; Job.SourceHandWidget = Source; Job.SourceVisibility = Source ? Source->GetVisibility() : ESlateVisibility::Visible;
	Job.DesiredSize = Card->GetDesiredSize();
	if (Job.DesiredSize.X <= 0 || Job.DesiredSize.Y <= 0) Job.DesiredSize = Origin.Size;
	Card->SetIsEnabled(false); Card->SetVisibility(ESlateVisibility::Hidden); Card->SetRenderTransformPivot(FVector2D(0.5f));
	auto* CardSlot = DetachedCardVFXHost->AddChildToCanvas(Card); if (!CardSlot) return false;
	CardSlot->SetSize(Job.DesiredSize); PlayedCardVisualJobs.Add(Job); OutToken = Job.Token; return true;
}

bool UBattleHUDWidget::BeginHostedCardDestination(const FPresentationRecord& Record, const FPresentationPlaybackToken& Token,
	const FPlayedCardPresentationLifecycleToken& Lifecycle)
{
	for (auto& Job : PlayedCardVisualJobs)
	{
		if (Job.Token.Lifecycle != Lifecycle) continue;
		if (Job.Phase != ECardVisualPhase::AtPlayArea || !Job.Widget || Job.Widget->GetParent() != DetachedCardVFXHost
			|| Job.ViewModel.Get() != ViewModel.Get() || Job.SurfaceGeneration != HandSurfaceGeneration
			|| !DoesNativeCardViewMatchSnapshot(Job.Widget->GetCardView(),Record.CardZoneChanged.Card)) return false;
		FPreparedCardDestinationVisual Spec;
		if (!BuildPlayedCardDestinationVisual(Job, Record, Spec)) return false;
		if (!CommitNativePresentationOwnership(Record.Type,Token)) return false;
		Job.Phase = ECardVisualPhase::DestinationTail; Job.BlockingToken = Token; Job.Elapsed = 0;
		Job.DestinationCenter = Spec.EndCenter; Job.EndScale = Spec.EndScale;
		Job.EndOpacity = Spec.EndOpacity;
		ActiveNativeCardPresentationKind = ENativeCardPresentationKind::PlayAreaToDestination;
		ActivePlayedCardVisualToken = Job.Token;
		UpdatePlayedCardVisualJobs(0);
		if (!StartNativePresentationFinishTimer(0.5f))
		{
			for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
				if (PlayedCardVisualJobs[I].Token.Lifecycle == Lifecycle) RetirePlayedCardVisualJob(I);
			ResetNativeCardRecordState(); AbortNativePresentationStart(); return false;
		}
		UE_LOG(LogBattleCardVisual, Verbose, TEXT("Hosted destination Runtime=%d Life=%lld Visual=%lld Resolution=%lld"),
			Lifecycle.RuntimeId, Lifecycle.LocalLifecycleGeneration, Job.Token.LocalVisualGeneration, Record.ResolutionId);
		return true;
	}
	return false;
}

void UBattleHUDWidget::UpdatePlayedCardVisualJobs(float DeltaSeconds)
{
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
	{
		auto& Job = PlayedCardVisualJobs[I];
		if (!Job.Widget || !DetachedCardVFXHost || !OV_PlayArea || Job.Widget->GetParent() != DetachedCardVFXHost
			|| Job.ViewModel.Get() != ViewModel.Get() || Job.SurfaceGeneration != HandSurfaceGeneration
			|| !FMath::IsFinite(Job.Duration) || Job.Duration <= 0
			|| !PresentationController || !PresentationController->IsCurrentPresentationSession(Job.Token.Lifecycle.SessionToken))
		{ RetirePlayedCardVisualJob(I); continue; }
		if (Job.Phase == ECardVisualPhase::Prepared) continue;
		const auto& Root = GetCardInputRootGeometry(); const auto& Play = OV_PlayArea->GetCachedGeometry();
		const FVector2D RootSize = Root.GetLocalSize();
		if (!FMath::IsFinite(RootSize.X) || !FMath::IsFinite(RootSize.Y) || RootSize.X <= 0 || RootSize.Y <= 0
			|| Play.GetLocalSize().IsNearlyZero()) { RetirePlayedCardVisualJob(I); continue; }
		Job.Elapsed += FMath::IsFinite(DeltaSeconds) ? FMath::Max(DeltaSeconds,0.f) : 0.f;
		if (Job.bDetachedArrival && Job.Phase == ECardVisualPhase::EnteringPlayArea && Job.Elapsed >= Job.Duration)
		{
			const float Remaining = Job.Elapsed-Job.Duration;
			Job.Phase = ECardVisualPhase::AtPlayArea; Job.Elapsed = 0;
			if (Job.bArrivalDestinationCommitted) BeginDetachedArrivalDestinationTail(Job, Remaining);
		}
		const float Alpha = FMath::InterpEaseOut(0.f,1.f,FMath::Clamp(Job.Elapsed/Job.Duration,0.f,1.f),3.f);
		const FVector2D Size = Root.GetLocalSize();
		const FVector2D PlayCenter = Job.bDetachedArrival ? Job.ArrivalDestination.StartCenter*Size
			: Root.AbsoluteToLocal(Play.LocalToAbsolute(Play.GetLocalSize()*0.5f));
		FVector2D Center = PlayCenter, Scale(1); float Angle = 0, Opacity = 1;
		if (Job.Phase == ECardVisualPhase::EnteringPlayArea)
		{
			Center = FMath::Lerp(Job.Origin.Center*Size,PlayCenter,Alpha);
			Scale = FMath::Lerp(Job.Origin.Size/Job.DesiredSize,FVector2D(1),Alpha);
			Angle = FMath::Lerp(Job.Origin.Angle,0.f,Alpha);
		}
		else if (Job.Phase == ECardVisualPhase::DestinationTail)
		{
			Center = FMath::Lerp(Job.bDetachedDestination ? Job.DetachedStartCenter*Size : PlayCenter,Job.DestinationCenter*Size,Alpha);
			Scale = FVector2D(FMath::Lerp(1.f,Job.EndScale,Alpha)); Opacity = FMath::Lerp(1.f,Job.EndOpacity,Alpha);
		}
		if (!FMath::IsFinite(Center.X) || !FMath::IsFinite(Center.Y) || !FMath::IsFinite(PlayCenter.X) || !FMath::IsFinite(PlayCenter.Y)
			|| !FMath::IsFinite(Scale.X) || !FMath::IsFinite(Scale.Y)) { RetirePlayedCardVisualJob(I); continue; }
		if (auto* CardSlot = Cast<UCanvasPanelSlot>(Job.Widget->Slot)) CardSlot->SetPosition(PlayCenter-Job.DesiredSize*0.5f);
		Job.Widget->SetRenderTranslation(Center-PlayCenter); Job.Widget->SetRenderScale(Scale);
		Job.Widget->SetRenderTransformAngle(Angle); Job.Widget->SetRenderOpacity(Opacity);
		if (Job.bDetachedDestination && Job.Elapsed >= Job.Duration) RetirePlayedCardVisualJob(I);
	}
}

void UBattleHUDWidget::RetirePlayedCardVisualJob(int32 Index, bool bRestoreSource)
{
	if (!PlayedCardVisualJobs.IsValidIndex(Index)) return;
	const auto Job = PlayedCardVisualJobs[Index]; PlayedCardVisualJobs.RemoveAt(Index);
	UE_LOG(LogBattleCardVisual, Verbose, TEXT("Retire Runtime=%d Visual=%lld Phase=%d"),
		Job.Token.Lifecycle.RuntimeId, Job.Token.LocalVisualGeneration, static_cast<int32>(Job.Phase));
	if (Job.Widget) Job.Widget->RemoveFromParent();
	if (bRestoreSource && Job.Phase == ECardVisualPhase::EnteringPlayArea && Job.SurfaceGeneration == HandSurfaceGeneration)
		if (auto* Source = Job.SourceHandWidget.Get(); Source && Source->GetParent() == HB_Hand.Get()
			&& Source->GetRuntimeId() == Job.Token.Lifecycle.RuntimeId && Source->GetCardId() == Job.Token.Lifecycle.CardId
			&& Source->GetVisibility() == ESlateVisibility::Hidden) Source->SetVisibility(Job.SourceVisibility);
}

void UBattleHUDWidget::RetireCollidingPlayedCardVisualJobs()
{
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
	{
		const auto& Job = PlayedCardVisualJobs[I];
		bool bRetire = Job.ViewModel.Get() != ViewModel.Get() || Job.SurfaceGeneration != HandSurfaceGeneration
			|| (ViewModel && (ViewModel->Outcome != EBattleHUDOutcome::None
				|| ViewModel->InteractionState == EBattleHUDInteractionState::PresentationUnavailable));
		if (HB_Hand)
			for (auto* Child : HB_Hand->GetAllChildren())
				if (auto* Card = Cast<UBattleCardWidget>(Child); Card && Card->IsVisible()
					&& Card->GetRuntimeId() == Job.Token.Lifecycle.RuntimeId && ViewModel
					&& ViewModel->GetCardPresentationOwner(Card->GetRuntimeId()) == ECardPresentationOwner::Hand)
					if (Job.Phase != ECardVisualPhase::Prepared || Card != Job.SourceHandWidget.Get()) bRetire = true;
		if (bRetire) RetirePlayedCardVisualJob(I);
	}
}

UBattleCardWidget* UBattleHUDWidget::FindPreferredPlayedCardVisual() const
{
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
		if (!PlayedCardVisualJobs[I].bDetachedDestination && PlayedCardVisualJobs[I].Phase != ECardVisualPhase::Prepared)
			return PlayedCardVisualJobs[I].Widget;
	if (auto* Fallback = NativeBlockingFallbackPlayedCardWidget.Get()) return Fallback;
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
		if (PlayedCardVisualJobs[I].bDetachedDestination) return PlayedCardVisualJobs[I].Widget;
	return nullptr;
}

void UBattleHUDWidget::SetPlayedCardVisualsSelectionHidden(bool bHidden)
{
	bPlayedCardVisualsSelectionHidden = bHidden;
	const auto CosmeticVisibility = bHidden ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible;
	for (auto& Job : PlayedCardVisualJobs)
		if (Job.Widget && !Job.bDetachedDestination && !Job.bDetachedArrival && Job.Phase != ECardVisualPhase::Prepared) Job.Widget->SetVisibility(CosmeticVisibility);
	if (auto* Card = NativeBlockingFallbackPlayedCardWidget.Get()) Card->SetVisibility(CosmeticVisibility);
}

bool UBattleHUDWidget::CompletePlayedCardVisualJob(const FDetachedCardVisualToken& VisualToken, const FPresentationPlaybackToken& BlockingToken)
{
	for (int32 I = 0; I < PlayedCardVisualJobs.Num(); ++I)
	{
		auto& Job = PlayedCardVisualJobs[I];
		if (Job.bDetachedDestination || Job.bDetachedArrival || !(Job.Token == VisualToken) || Job.BlockingToken != BlockingToken) continue;
		if (Job.Phase == ECardVisualPhase::EnteringPlayArea)
		{
			Job.Phase = ECardVisualPhase::AtPlayArea; Job.Elapsed = 0; UpdatePlayedCardVisualJobs(0); return true;
		}
		if (Job.Phase == ECardVisualPhase::DestinationTail) { RetirePlayedCardVisualJob(I); return true; }
		return false;
	}
	return false;
}

void UBattleHUDWidget::NativeCancelPlayedCardVisualsForSession(const FPresentationSessionToken& Session)
{
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
		if (PlayedCardVisualJobs[I].Token.Lifecycle.SessionToken == Session) RetirePlayedCardVisualJob(I,true);
	if (BlockingFallbackPlayedLifecycle.SessionToken == Session)
	{
		if (auto* Card = NativeBlockingFallbackPlayedCardWidget.Get()) Card->RemoveFromParent();
		NativeBlockingFallbackPlayedCardWidget.Reset(); BlockingFallbackPlayedLifecycle = {};
	}
}

void UBattleHUDWidget::NativeCancelAllPlayedCardVisuals()
{
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I) RetirePlayedCardVisualJob(I,true);
	if (auto* Card = NativeBlockingFallbackPlayedCardWidget.Get()) Card->RemoveFromParent();
	NativeBlockingFallbackPlayedCardWidget.Reset(); BlockingFallbackPlayedLifecycle = {};
}
