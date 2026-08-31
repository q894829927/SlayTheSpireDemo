#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDWidget.h"
#include "Phase6UIA2NR6TestTypes.generated.h"

class UTextBlock;
class UWorld;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR6HUDProbe : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
	void SetTestWorld(UWorld* InWorld);
	void ConfigureR6Surfaces(
		UTextBlock* InEnergyText,
		UTextBlock* InPlayerBlockText,
		UTextBlock* InEnemyBlockText,
		UTextBlock* InDrawCountText,
		UTextBlock* InDiscardCountText);

	virtual UWorld* GetWorld() const override;

	bool IsLocalPresentationActive() const { return HasActiveNativePresentation(); }
	bool IsLocalFinishTimerSet() const { return HasNativePresentationFinishTimer(); }
	EBattlePresentationRecordType ActiveLocalType() const { return GetActiveNativePresentationType(); }
	FPresentationPlaybackToken ActiveLocalToken() const { return GetActiveNativePresentationToken(); }

	void InvokeFinishForTesting(const FPresentationPlaybackToken& Token);
	void InvokeCancelForTesting(const FPresentationPlaybackToken& Token);
	void InvokeNativeDestructForTesting();

	int32 CancelDispatchCount = 0;
	FPresentationPlaybackToken LastCancelDispatchToken;

protected:
	virtual void CancelPresentationRecordPlayback_Implementation(
		const FPresentationPlaybackToken& Token) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWorld> TestWorld = nullptr;
};
