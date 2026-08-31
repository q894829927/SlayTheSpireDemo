#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDWidget.h"
#include "Phase6UIA2NR5TestTypes.generated.h"

class UWorld;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR5HUDProbe : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
	void SetTestWorld(UWorld* InWorld);
	void SetAcceptSyntheticPlayback(bool bInAccept) { bAcceptSyntheticPlayback = bInAccept; }
	void SetForceTimerFailure(bool bInForceFailure) { bForceTimerFailure = bInForceFailure; }

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
	virtual bool BeginPresentationRecordPlayback_Implementation(
		const FPresentationRecord& Record,
		const FPresentationPlaybackToken& Token) override;
	virtual void CancelPresentationRecordPlayback_Implementation(
		const FPresentationPlaybackToken& Token) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWorld> TestWorld = nullptr;

	bool bAcceptSyntheticPlayback = false;
	bool bForceTimerFailure = false;
};
