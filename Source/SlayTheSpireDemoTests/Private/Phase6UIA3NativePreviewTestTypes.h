#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDCombatantPresentationWidgetBase.h"
#include "UI/BattleHUDWidget.h"
#include "UI/BattleHUDWidgetBase.h"
#include "Phase6UIA3NativePreviewTestTypes.generated.h"

class UBattleHUDViewModel;
class UHorizontalBox;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA3PreviewCombatantProbe
	: public UBattleHUDCombatantPresentationWidgetBase
{
	GENERATED_BODY()
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA3PreviewHUDProbe : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
	void ConfigurePreviewSurface(UBattleHUDViewModel* InViewModel, UHorizontalBox* InHand);
	void RequestPreviewForTesting(int32 TargetId);
	void ApplyPreviewSurfaceForTesting();
	void ReleasePreviewSurfaceForTesting();
	void ClearPreviewAsCombatantWouldForTesting();
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA3BaseHUDProbe : public UBattleHUDWidgetBase
{
	GENERATED_BODY()
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA3PreviewEventSink : public UObject
{
	GENERATED_BODY()

public:
	void ObserveViewModel(UBattleHUDViewModel* InViewModel);

	UFUNCTION()
	void HandleInspectRequested(UBattleHUDCombatantPresentationWidgetBase* Presentation);

	UFUNCTION()
	void HandleInspectCleared(UBattleHUDCombatantPresentationWidgetBase* Presentation);

	UFUNCTION()
	void HandlePreviewRequested(int32 TargetId);

	UFUNCTION()
	void HandlePreviewCleared();

	UFUNCTION()
	void HandleViewModelChanged();

	UFUNCTION()
	void HandlePreviewViewModelChanged();

	int32 InspectRequestedCount = 0;
	int32 InspectClearedCount = 0;
	int32 PreviewRequestedCount = 0;
	int32 PreviewClearedCount = 0;
	int32 StructuralChangedCount = 0;
	int32 PreviewChangedCount = 0;
	int32 LastPreviewTargetId = INDEX_NONE;
	bool bObservedPreRequestPreviewClear = false;

protected:
	virtual void BeginDestroy() override;

private:
	TWeakObjectPtr<UBattleHUDViewModel> ObservedViewModel;
};
