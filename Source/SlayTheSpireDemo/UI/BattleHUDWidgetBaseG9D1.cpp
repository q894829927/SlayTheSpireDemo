#include "BattleHUDWidgetBase.h"
#include "BattleHUDViewModel.h"
#include "../Presentation/BattlePresentationController.h"

bool UBattleHUDWidgetBase::PrepareDetachedCardDestination(const FPresentationRecord& Record,
	const FPlayedCardPresentationLifecycleToken& Lifecycle, FDetachedCardDestinationToken& OutToken)
{
	OutToken = {};
	UBattlePresentationController* const Controller = PresentationController.Get();
	UBattleHUDViewModel* const Model = ViewModel.Get();
	FPlayedCardPresentationLifecycleToken Current;
	if (!IsValid(Controller) || !IsValid(Model) || HasTrackedPresentationPlayback()
		|| !Controller->IsDetachedCardDestinationD1Enabled()
		|| Record.Type != EBattlePresentationRecordType::CardZoneChanged
		|| Record.CardZoneChanged.FromZone != ECardZone::PlayArea
		|| !Controller->TryGetPlayedCardLifecycleForRecord(Record, Current) || Current != Lifecycle) return false;
	const bool bPrepared = NativePrepareDetachedCardDestination(Record, Lifecycle, OutToken);
	if (!bPrepared || !OutToken.IsValid() || OutToken.Visual.Lifecycle != Lifecycle
		|| OutToken.ResolutionId != Record.ResolutionId || OutToken.PresentationSequence != Record.PresentationSequence
		|| !NativeIsPreparedDetachedCardDestinationCurrent(OutToken)
		|| PresentationController.Get() != Controller || ViewModel.Get() != Model
		|| !Controller->IsCurrentPresentationSession(Lifecycle.SessionToken)
		|| !Controller->TryGetPlayedCardLifecycleForRecord(Record, Current) || Current != Lifecycle)
	{
		NativeCancelPreparedDetachedCardDestination(OutToken); OutToken = {}; return false;
	}
	return true;
}

bool UBattleHUDWidgetBase::ActivatePreparedDetachedCardDestination(const FDetachedCardDestinationToken& Token)
{
	if (!Token.IsValid() || !IsValid(PresentationController) || HasTrackedPresentationPlayback()
		|| !PresentationController->IsDetachedCardDestinationD1Enabled()
		|| !PresentationController->IsCommittedCardDestinationCurrent(Token)
		|| !PresentationController->IsCurrentPresentationSession(Token.Visual.Lifecycle.SessionToken)) return false;
	return NativeActivatePreparedDetachedCardDestination(Token);
}

void UBattleHUDWidgetBase::CancelPreparedDetachedCardDestination(const FDetachedCardDestinationToken& Token)
{
	if (Token.IsValid()) NativeCancelPreparedDetachedCardDestination(Token);
}
void UBattleHUDWidgetBase::RetireDetachedCardDestination(const FDetachedCardDestinationToken& Token)
{
	if (Token.IsValid()) NativeRetireDetachedCardDestination(Token);
}
void UBattleHUDWidgetBase::CancelDetachedCardDestinationVisuals()
{
	NativeCancelDetachedCardDestinationVisuals();
}
