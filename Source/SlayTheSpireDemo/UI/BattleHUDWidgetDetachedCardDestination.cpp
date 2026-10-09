#include "BattleHUDWidget.h"
#include "BattleCardWidget.h"
#include "BattleHUDViewModel.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "../Presentation/BattlePresentationController.h"

DEFINE_LOG_CATEGORY_STATIC(LogBattleCardTail, Log, All);

void UBattleHUDWidget::SetDetachedCardDestinationD1Enabled(bool bEnabled)
{
	bEnableDetachedCardDestinationD1 = bEnabled;
	if (PresentationController) PresentationController->SetDetachedCardDestinationD1Enabled(bEnabled);
	else if (!bEnabled) NativeCancelDetachedCardDestinationVisuals();
}
void UBattleHUDWidget::NativeOnPresentationControllerChanged()
{
	if (PresentationController) PresentationController->SetDetachedCardDestinationD1Enabled(bEnableDetachedCardDestinationD1 && bNativeBindingsValid);
	if (PresentationController) PresentationController->SetDetachedCardArrivalD2Enabled(bEnableDetachedCardArrivalD2 && bNativeBindingsValid);
}

bool UBattleHUDWidget::BuildPlayedCardDestinationVisual(const FNativePlayedCardVisualJob& Job,
	const FPresentationRecord& Record, FPreparedCardDestinationVisual& OutSpec, bool bArrivalPreflight) const
{
	OutSpec = {};
	if ((Job.Phase != ECardVisualPhase::AtPlayArea && !(bArrivalPreflight && Job.Phase == ECardVisualPhase::Prepared))
		|| Job.bDetachedDestination || !IsValid(Job.Widget)
		|| Job.Widget->GetParent() != DetachedCardVFXHost || Job.ViewModel.Get() != ViewModel.Get()
		|| Job.SurfaceGeneration != HandSurfaceGeneration || !OV_PlayArea
		|| !DoesNativeCardViewMatchSnapshot(Job.Widget->GetCardView(), Record.CardZoneChanged.Card)) return false;
	const auto Zone = Record.CardZoneChanged.ToZone;
	if (Zone != ECardZone::DiscardPile && Zone != ECardZone::ExhaustPile && Zone != ECardZone::RemovedPile) return false;
	const auto& Root = GetCardInputRootGeometry(); const auto& Play = OV_PlayArea->GetCachedGeometry();
	const FVector2D Size = Root.GetLocalSize();
	if (!FMath::IsFinite(Size.X) || !FMath::IsFinite(Size.Y) || Size.X <= 0 || Size.Y <= 0
		|| Play.GetLocalSize().IsNearlyZero()) return false;
	const FVector2D Start = Root.AbsoluteToLocal(Play.LocalToAbsolute(Play.GetLocalSize()*0.5f));
	FVector2D End = Start;
	if (Zone == ECardZone::DiscardPile)
	{
		if (Txt_DiscardCount && !Txt_DiscardCount->GetCachedGeometry().GetLocalSize().IsNearlyZero())
		{
			const auto& Pile = Txt_DiscardCount->GetCachedGeometry();
			End = Root.AbsoluteToLocal(Pile.LocalToAbsolute(Pile.GetLocalSize()*0.5f));
		}
		else End += FVector2D(420,90);
	}
	if (!FMath::IsFinite(Start.X) || !FMath::IsFinite(Start.Y) || !FMath::IsFinite(End.X) || !FMath::IsFinite(End.Y)) return false;
	OutSpec.StartCenter = Start/Size; OutSpec.EndCenter = End/Size;
	OutSpec.EndOpacity = Zone == ECardZone::DiscardPile ? 0.15f : 0;
	return FMath::IsFinite(OutSpec.Duration) && OutSpec.Duration > 0;
}

bool UBattleHUDWidget::NativePrepareDetachedCardDestination(const FPresentationRecord& Record,
	const FPlayedCardPresentationLifecycleToken& Lifecycle, FDetachedCardDestinationToken& OutToken)
{
	OutToken = {};
	if (!Lifecycle.IsValid() || PlayedCardVisualJobs.Num() > 32 || NextCardDestinationPreparationGeneration <= 0
		|| NextCardDestinationPreparationGeneration == MAX_int64 || HasActiveNativePresentation()) return false;
	for (auto& Job : PlayedCardVisualJobs)
	{
		if (Job.Token.Lifecycle != Lifecycle) continue;
		FPreparedCardDestinationVisual Spec;
		if (Job.PreparedDestination.Token.IsValid() || !BuildPlayedCardDestinationVisual(Job, Record, Spec)) return false;
		Spec.Token.Visual = Job.Token; Spec.Token.ResolutionId = Record.ResolutionId;
		Spec.Token.PresentationSequence = Record.PresentationSequence;
		Spec.Token.PreparationGeneration = NextCardDestinationPreparationGeneration++;
		Job.PreparedDestination = Spec; Job.LastDestinationPreparationToken = Spec.Token;
		OutToken = Spec.Token; return true;
	}
	return false;
}

bool UBattleHUDWidget::NativeIsPreparedDetachedCardDestinationCurrent(const FDetachedCardDestinationToken& Token) const
{
	const FVector2D Size = GetCardInputRootGeometry().GetLocalSize();
	if (!FMath::IsFinite(Size.X) || !FMath::IsFinite(Size.Y) || Size.X <= 0 || Size.Y <= 0) return false;
	for (const auto& Job : PlayedCardVisualJobs)
		if (Job.Token == Token.Visual && Job.PreparedDestination.Token == Token)
			return Job.Phase == ECardVisualPhase::AtPlayArea && !Job.bDetachedDestination && IsValid(Job.Widget)
				&& Job.Widget->GetParent() == DetachedCardVFXHost && Job.ViewModel.Get() == ViewModel.Get()
				&& Job.SurfaceGeneration == HandSurfaceGeneration;
	return false;
}

bool UBattleHUDWidget::NativeActivatePreparedDetachedCardDestination(const FDetachedCardDestinationToken& Token)
{
	for (auto& Job : PlayedCardVisualJobs)
	{
		if (!(Job.Token == Token.Visual) || !(Job.PreparedDestination.Token == Token)) continue;
		if (Job.Phase != ECardVisualPhase::AtPlayArea || !IsValid(Job.Widget) || Job.Widget->GetParent() != DetachedCardVFXHost
			|| Job.ViewModel.Get() != ViewModel.Get() || Job.SurfaceGeneration != HandSurfaceGeneration) return false;
		const auto Spec = Job.PreparedDestination;
		Job.PreparedDestination = {}; Job.Phase = ECardVisualPhase::DestinationTail;
		Job.bDetachedDestination = true; Job.BlockingToken = {}; Job.Elapsed = 0;
		Job.DetachedStartCenter = Spec.StartCenter; Job.DestinationCenter = Spec.EndCenter;
		Job.Duration = Spec.Duration; Job.EndScale = Spec.EndScale; Job.EndOpacity = Spec.EndOpacity;
		Job.Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		UE_LOG(LogBattleCardTail, Verbose, TEXT("Detached activate Runtime=%d Visual=%lld Destination=%lld/%lld Jobs=%d"),
			Token.Visual.Lifecycle.RuntimeId, Token.Visual.LocalVisualGeneration, Token.ResolutionId, Token.PresentationSequence, PlayedCardVisualJobs.Num());
		UpdatePlayedCardVisualJobs(0);
		return PlayedCardVisualJobs.ContainsByPredicate([&](const auto& Current) { return Current.Token == Token.Visual; });
	}
	return false;
}

void UBattleHUDWidget::NativeCancelPreparedDetachedCardDestination(const FDetachedCardDestinationToken& Token)
{
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
	{
		auto& Job = PlayedCardVisualJobs[I];
		if (Job.Token == Token.Visual && Job.PreparedDestination.Token == Token)
		{
			Job.PreparedDestination = {};
			if (!IsValid(Job.Widget) || Job.Widget->GetParent() != DetachedCardVFXHost) RetirePlayedCardVisualJob(I);
		}
	}
}
void UBattleHUDWidget::NativeRetireDetachedCardDestination(const FDetachedCardDestinationToken& Token)
{
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
		if (PlayedCardVisualJobs[I].Token == Token.Visual && PlayedCardVisualJobs[I].LastDestinationPreparationToken == Token)
			RetirePlayedCardVisualJob(I);
}
void UBattleHUDWidget::NativeCancelDetachedCardDestinationVisuals()
{
	for (int32 I = PlayedCardVisualJobs.Num()-1; I >= 0; --I)
		if (PlayedCardVisualJobs[I].bDetachedDestination || PlayedCardVisualJobs[I].bDetachedArrival) RetirePlayedCardVisualJob(I);
		else PlayedCardVisualJobs[I].PreparedDestination = {};
}
