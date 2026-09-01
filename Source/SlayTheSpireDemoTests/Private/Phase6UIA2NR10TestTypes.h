#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDWidget.h"
#include "Phase6UIA2NR10TestTypes.generated.h"

class UBattleHUDViewModel;
class UButton;
class UOverlay;
class UTextBlock;
class UWorld;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR10HUDProbe : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
	void SetTestWorld(UWorld* InWorld);
	void SetViewModelForTesting(UBattleHUDViewModel* InViewModel) { ViewModel = InViewModel; }
	void ConfigureTerminalSurfaces(
		UOverlay* InTerminalOverlay,
		UTextBlock* InOutcomeText,
		UTextBlock* InFeedbackText,
		UButton* InEndTurnButton);

	virtual UWorld* GetWorld() const override;

	bool InvokeBeginDirectForTesting(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	void InvokeFinishForTesting(const FPresentationPlaybackToken& Token);
	void InvokeCancelForTesting(const FPresentationPlaybackToken& Token);
	void InvokeNativeDestructForTesting();
	void RefreshTerminalForTesting();
	void RefreshAvailabilityForTesting();
	void RefreshInputForTesting();

	bool IsLocalPresentationActive() const { return HasActiveNativePresentation(); }
	bool IsLocalFinishTimerSet() const { return HasNativePresentationFinishTimer(); }
	EBattlePresentationRecordType ActiveLocalType() const
	{
		return GetActiveNativePresentationType();
	}
	FPresentationPlaybackToken ActiveLocalToken() const
	{
		return GetActiveNativePresentationToken();
	}

private:
	UPROPERTY(Transient)
	TObjectPtr<UWorld> TestWorld = nullptr;
};
