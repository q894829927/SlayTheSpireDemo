#include "Phase6UIA3NativePreviewTestTypes.h"

#include "Components/Overlay.h"
#include "UI/BattleHUDViewModel.h"

void UPhase6UIA3PreviewHUDProbe::ConfigurePreviewSurface(
	UBattleHUDViewModel* InViewModel,
	UOverlay* InOverlay
)
{
	OV_PlayArea = InOverlay;
	SetViewModel(InViewModel);
	EnsureImmediatePreviewSurface();
}

void UPhase6UIA3PreviewHUDProbe::ReleasePreviewSurfaceForTesting()
{
	ReleaseImmediatePreviewSurface();
}

void UPhase6UIA3PreviewHUDProbe::ClearPreviewAsCombatantWouldForTesting()
{
	HandleCombatantPreviewCleared();
}

void UPhase6UIA3PreviewEventSink::ObserveViewModel(UBattleHUDViewModel* InViewModel)
{
	if (UBattleHUDViewModel* Previous = ObservedViewModel.Get())
	{
		Previous->OnChanged.RemoveDynamic(
			this,
			&UPhase6UIA3PreviewEventSink::HandleViewModelChanged);
	}

	ObservedViewModel = InViewModel;
	if (IsValid(InViewModel))
	{
		InViewModel->OnChanged.AddUniqueDynamic(
			this,
			&UPhase6UIA3PreviewEventSink::HandleViewModelChanged);
	}
}

void UPhase6UIA3PreviewEventSink::HandleInspectRequested(
	UBattleHUDCombatantPresentationWidgetBase* /*Presentation*/
)
{
	++InspectRequestedCount;
}

void UPhase6UIA3PreviewEventSink::HandleInspectCleared(
	UBattleHUDCombatantPresentationWidgetBase* /*Presentation*/
)
{
	++InspectClearedCount;
}

void UPhase6UIA3PreviewEventSink::HandlePreviewRequested(int32 TargetId)
{
	++PreviewRequestedCount;
	LastPreviewTargetId = TargetId;
}

void UPhase6UIA3PreviewEventSink::HandlePreviewCleared()
{
	++PreviewClearedCount;
	LastPreviewTargetId = INDEX_NONE;
}

void UPhase6UIA3PreviewEventSink::HandleViewModelChanged()
{
	const UBattleHUDViewModel* ViewModel = ObservedViewModel.Get();
	if (IsValid(ViewModel)
		&& ViewModel->SelectedCardRuntimeId != INDEX_NONE
		&& ViewModel->InteractionState == EBattleHUDInteractionState::ChoosingTarget
		&& !ViewModel->bHasImmediatePreview
		&& ViewModel->PreviewTargetId == INDEX_NONE)
	{
		bObservedPreRequestPreviewClear = true;
	}
}

void UPhase6UIA3PreviewEventSink::BeginDestroy()
{
	ObserveViewModel(nullptr);
	Super::BeginDestroy();
}