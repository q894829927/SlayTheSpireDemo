#include "BattleHUDWidgetBase.h"
#include "BattleHUDViewModel.h"
#include "../Presentation/BattlePresentationController.h"

bool UBattleHUDWidgetBase::PrepareDetachedCardArrival(const FPresentationRecord& Record, const FPresentationRecord& Destination,
	const FPlayedCardPresentationLifecycleToken& Lifecycle, FDetachedCardArrivalToken& OutToken)
{
	OutToken = {};
	auto* const Controller = PresentationController.Get(); auto* const Model = ViewModel.Get();
	FPlayedCardPresentationLifecycleToken Current; FPresentationRecord Future;
	if (!IsValid(Controller) || !IsValid(Model) || HasTrackedPresentationPlayback()
		|| !Controller->IsDetachedCardArrivalD2Enabled() || !Controller->IsDetachedCardDestinationD1Enabled()
		|| !Controller->TryGetDetachedCardArrivalPlan(Record, Current, Future) || Current != Lifecycle
		|| Future.ResolutionId != Destination.ResolutionId || Future.PresentationSequence != Destination.PresentationSequence) return false;
	const bool Prepared = NativePrepareDetachedCardArrival(Record, Destination, Lifecycle, OutToken);
	if (!Prepared || !OutToken.IsValid() || OutToken.Visual.Lifecycle != Lifecycle
		|| OutToken.DestinationResolutionId != Destination.ResolutionId || OutToken.DestinationPresentationSequence != Destination.PresentationSequence
		|| !NativeIsPreparedDetachedCardArrivalCurrent(OutToken) || PresentationController.Get() != Controller || ViewModel.Get() != Model
		|| !Controller->TryGetDetachedCardArrivalPlan(Record, Current, Future) || Current != Lifecycle
		|| Future.ResolutionId != OutToken.DestinationResolutionId || Future.PresentationSequence != OutToken.DestinationPresentationSequence)
	{ NativeRetireDetachedCardArrival(OutToken); OutToken = {}; return false; }
	return true;
}

bool UBattleHUDWidgetBase::ActivatePreparedDetachedCardArrival(const FDetachedCardArrivalToken& Token)
{
	if (!Token.IsValid() || !IsValid(PresentationController) || HasTrackedPresentationPlayback()
		|| !PresentationController->IsDetachedCardArrivalD2Enabled() || !PresentationController->IsDetachedCardDestinationD1Enabled()
		|| !PresentationController->IsCommittedCardArrivalCurrent(Token, false)) return false;
	return NativeActivatePreparedDetachedCardArrival(Token);
}

bool UBattleHUDWidgetBase::CommitDetachedCardArrivalDestination(const FDetachedCardArrivalToken& Token)
{
	if (!Token.IsValid() || !IsValid(PresentationController) || HasTrackedPresentationPlayback()
		|| !PresentationController->IsCommittedCardArrivalCurrent(Token, true)) return false;
	return NativeCommitDetachedCardArrivalDestination(Token);
}
void UBattleHUDWidgetBase::RetireDetachedCardArrival(const FDetachedCardArrivalToken& Token)
{ if (Token.IsValid()) NativeRetireDetachedCardArrival(Token); }
void UBattleHUDWidgetBase::CancelDetachedCardArrivalVisuals() { NativeCancelDetachedCardArrivalVisuals(); }
