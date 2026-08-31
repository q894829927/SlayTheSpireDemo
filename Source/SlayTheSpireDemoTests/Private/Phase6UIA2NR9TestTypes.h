#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDWidget.h"
#include "UI/BattleStatusWidget.h"
#include "Phase6UIA2NR9TestTypes.generated.h"

class UBattleHUDViewModel;
class UImage;
class UTextBlock;
class UWrapBox;
class UWorld;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR9StatusProbe : public UBattleStatusWidget
{
	GENERATED_BODY()

public:
	UTextBlock* AmountTextForTesting() const { return Txt_StatusAmount; }
	UImage* IconForTesting() const { return Img_StatusIcon; }

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UImage> TestIcon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TestAmount = nullptr;
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR9HUDProbe : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
	void SetTestWorld(UWorld* InWorld);
	void SetViewModelForTesting(UBattleHUDViewModel* InViewModel) { ViewModel = InViewModel; }
	void ConfigureStatusSurfaces(UWrapBox* InPlayerStatuses, UWrapBox* InEnemyStatuses);
	virtual UWorld* GetWorld() const override;

	void RefreshStatusRowsForTesting() { RefreshStatusRows(); }
	bool InvokeBeginDirectForTesting(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token);
	void InvokeFinishForTesting(const FPresentationPlaybackToken& Token);
	void InvokeCancelForTesting(const FPresentationPlaybackToken& Token);
	void InvokeNativeDestructForTesting();

	bool IsLocalPresentationActive() const { return HasActiveNativePresentation(); }
	bool IsLocalFinishTimerSet() const { return HasNativePresentationFinishTimer(); }
	UBattleStatusWidget* ActiveStatusForTesting() const
	{
		return GetActiveNativeStatusPresentationWidget();
	}
	bool IsActiveStatusCreateForTesting() const
	{
		return IsActiveNativeStatusCreatedTransient();
	}

private:
	UPROPERTY(Transient)
	TObjectPtr<UWorld> TestWorld = nullptr;
};
