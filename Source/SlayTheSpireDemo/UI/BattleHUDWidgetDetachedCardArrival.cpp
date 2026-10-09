#include "BattleHUDWidget.h"
#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "Components/CanvasPanel.h"
#include "Components/Overlay.h"
#include "../Presentation/BattlePresentationController.h"

DEFINE_LOG_CATEGORY_STATIC(LogBattleCardArrival, Log, All);

void UBattleHUDWidget::SetDetachedCardArrivalD2Enabled(bool bEnabled)
{
	bEnableDetachedCardArrivalD2 = bEnabled;
	if (PresentationController) PresentationController->SetDetachedCardArrivalD2Enabled(bEnabled);
	else if (!bEnabled) NativeCancelDetachedCardArrivalVisuals();
}

TOptional<FCardPlayVisualOrigin> UBattleHUDWidget::ResolveHostedCardPlayOrigin(const FPresentationRecord& Record,
	const FPlayedCardPresentationLifecycleToken& Lifecycle) const
{
	TOptional<FCardPlayVisualOrigin> Origin;
	if (SubmittedCardVisual.IsSet() && SubmittedCardViewModel.Get() == ViewModel.Get()
		&& SubmittedCardController.Get() == PresentationController.Get()
		&& SubmittedCardVisual->Turn.BattleId == static_cast<uint64>(Record.BattleId)
		&& SubmittedCardVisual->RuntimeId == Lifecycle.RuntimeId && SubmittedCardVisual->CardId == Lifecycle.CardId
		&& (SubmittedCardVisual->InputSequence == 0 || SubmittedCardVisual->TargetPresentationId == Record.CardPlayed.TargetPresentationId)
		&& (!SubmittedCardVisual->PresentationFence.IsSet() || PresentationController->IsCurrentPresentationSession(SubmittedCardVisual->PresentationFence.GetValue())))
		Origin = SubmittedCardVisual->VisualOrigin;
	if (!Origin.IsSet()) Origin = CaptureCardPlayVisualOrigin(Lifecycle.RuntimeId);
	return Origin;
}

bool UBattleHUDWidget::NativePrepareDetachedCardArrival(const FPresentationRecord& Record, const FPresentationRecord& Destination,
	const FPlayedCardPresentationLifecycleToken& Lifecycle, FDetachedCardArrivalToken& OutToken)
{
	OutToken = {};
	UBattleCardWidget* Source = nullptr;
	if (!Lifecycle.IsValid() || !ViewModel || !OV_PlayArea || HasActiveNativePresentation()
		|| NextCardArrivalPreparationGeneration <= 0 || NextCardArrivalPreparationGeneration == MAX_int64
		|| !FindExactHistoricalHandCard(Record.CardPlayed.Card, Record.CardPlayed.HandIndexBefore, Source)) return false;
	const auto PlaySize = OV_PlayArea->GetCachedGeometry().GetLocalSize();
	if (!FMath::IsFinite(PlaySize.X) || !FMath::IsFinite(PlaySize.Y) || PlaySize.X <= 0 || PlaySize.Y <= 0) return false;
	const auto Origin = ResolveHostedCardPlayOrigin(Record, Lifecycle);
	if (!Origin.IsSet()) return false;
	FDetachedCardVisualToken Visual;
	if (!PreparePlayedCardVisualJob(Record, {}, Lifecycle, Origin.GetValue(), Source, Visual, true)) return false;
	const int32 Index = PlayedCardVisualJobs.IndexOfByPredicate([&](const auto& Job) { return Job.Token == Visual; });
	if (Index == INDEX_NONE) return false;
	FPreparedCardDestinationVisual Spec;
	if (!BuildPlayedCardDestinationVisual(PlayedCardVisualJobs[Index], Destination, Spec, true))
	{ RetirePlayedCardVisualJob(Index); return false; }
	FDetachedCardArrivalToken Receipt;
	Receipt.Visual = Visual; Receipt.DestinationResolutionId = Destination.ResolutionId;
	Receipt.DestinationPresentationSequence = Destination.PresentationSequence;
	Receipt.PreparationGeneration = NextCardArrivalPreparationGeneration++;
	PlayedCardVisualJobs[Index].ArrivalToken = Receipt;
	PlayedCardVisualJobs[Index].ArrivalDestination = Spec;
	PlayedCardVisualJobs[Index].SourcePresentationId = Record.CardPlayed.SourcePresentationId;
	OutToken = Receipt; return true;
}

bool UBattleHUDWidget::NativeIsPreparedDetachedCardArrivalCurrent(const FDetachedCardArrivalToken& Token) const
{
	const auto Size = GetCardInputRootGeometry().GetLocalSize();
	if (!FMath::IsFinite(Size.X) || !FMath::IsFinite(Size.Y) || Size.X <= 0 || Size.Y <= 0) return false;
	for (const auto& Job : PlayedCardVisualJobs)
		if (Job.ArrivalToken == Token && Job.Token == Token.Visual)
		{
			auto* Source = Job.SourceHandWidget.Get();
			return Job.bDetachedArrival && Job.Phase == ECardVisualPhase::Prepared && IsValid(Job.Widget)
				&& Job.Widget->GetParent() == DetachedCardVFXHost && Job.ViewModel.Get() == ViewModel.Get()
				&& Job.SurfaceGeneration == HandSurfaceGeneration && IsValid(Source) && Source->GetParent() == HB_Hand.Get()
				&& Source->GetRuntimeId() == Token.Visual.Lifecycle.RuntimeId && Source->GetCardId() == Token.Visual.Lifecycle.CardId;
		}
	return false;
}

bool UBattleHUDWidget::NativeActivatePreparedDetachedCardArrival(const FDetachedCardArrivalToken& Token)
{
	for (auto& Job : PlayedCardVisualJobs)
	{
		if (!(Job.ArrivalToken == Token) || !(Job.Token == Token.Visual)) continue;
		if (!Job.bDetachedArrival || Job.Phase != ECardVisualPhase::Prepared || !IsValid(Job.Widget)
			|| Job.Widget->GetParent() != DetachedCardVFXHost || Job.ViewModel.Get() != ViewModel.Get()
			|| Job.SurfaceGeneration != HandSurfaceGeneration) return false;
		Job.Phase = ECardVisualPhase::EnteringPlayArea; Job.Elapsed = 0;
		Job.Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		const auto Source = Job.SourcePresentationId;
		const bool Attack = Job.Widget->GetCardView().CardType == ECardType::Attack;
		SubmittedCardVisual.Reset();
		UpdatePlayedCardVisualJobs(0);
		if (Attack) PlayNativeCombatantAnimation(Source, EBattleHUDCombatantAnimation::Attack);
		UE_LOG(LogBattleCardArrival, Verbose, TEXT("Detached arrival Runtime=%d Visual=%lld Destination=%lld/%lld"),
			Token.Visual.Lifecycle.RuntimeId, Token.Visual.LocalVisualGeneration, Token.DestinationResolutionId, Token.DestinationPresentationSequence);
		return PlayedCardVisualJobs.ContainsByPredicate([&](const auto& Current) { return Current.ArrivalToken == Token; });
	}
	return false;
}

void UBattleHUDWidget::BeginDetachedArrivalDestinationTail(FNativePlayedCardVisualJob& Job, float Elapsed)
{
	const auto& Spec = Job.ArrivalDestination;
	Job.Phase = ECardVisualPhase::DestinationTail; Job.Elapsed = Elapsed;
	Job.bDetachedDestination = true; Job.DetachedStartCenter = Spec.StartCenter;
	Job.DestinationCenter = Spec.EndCenter; Job.Duration = Spec.Duration;
	Job.EndScale = Spec.EndScale; Job.EndOpacity = Spec.EndOpacity;
}

bool UBattleHUDWidget::NativeCommitDetachedCardArrivalDestination(const FDetachedCardArrivalToken& Token)
{
	for (auto& Job : PlayedCardVisualJobs)
	{
		if (!(Job.ArrivalToken == Token) || !(Job.Token == Token.Visual)) continue;
		if (!Job.bDetachedArrival || Job.bArrivalDestinationCommitted || !IsValid(Job.Widget)
			|| Job.Widget->GetParent() != DetachedCardVFXHost || Job.ViewModel.Get() != ViewModel.Get()
			|| Job.SurfaceGeneration != HandSurfaceGeneration
			|| (Job.Phase != ECardVisualPhase::EnteringPlayArea && Job.Phase != ECardVisualPhase::AtPlayArea)) return false;
		Job.bArrivalDestinationCommitted = true;
		if (Job.Phase == ECardVisualPhase::AtPlayArea) BeginDetachedArrivalDestinationTail(Job);
		UpdatePlayedCardVisualJobs(0);
		return PlayedCardVisualJobs.ContainsByPredicate([&](const auto& Current) { return Current.ArrivalToken == Token; });
	}
	return false;
}

void UBattleHUDWidget::NativeRetireDetachedCardArrival(const FDetachedCardArrivalToken& Token)
{
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
		if (PlayedCardVisualJobs[I].ArrivalToken == Token && PlayedCardVisualJobs[I].Token == Token.Visual) RetirePlayedCardVisualJob(I);
}
void UBattleHUDWidget::NativeCancelDetachedCardArrivalVisuals()
{
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
		if (PlayedCardVisualJobs[I].bDetachedArrival) RetirePlayedCardVisualJob(I);
}
