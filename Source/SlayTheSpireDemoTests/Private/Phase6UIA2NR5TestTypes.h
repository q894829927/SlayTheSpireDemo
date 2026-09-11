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
	void SetAcceptSyntheticBlockPlayback(bool bInAccept) { bAcceptSyntheticBlockPlayback = bInAccept; }
	void SetAcceptDetachedDamage(bool bInAccept) { bAcceptDetachedDamage = bInAccept; }
	void SetForceTimerFailure(bool bInForceFailure) { bForceTimerFailure = bInForceFailure; }

	virtual UWorld* GetWorld() const override;

	bool IsLocalPresentationActive() const { return HasActiveNativePresentation(); }
	bool IsLocalFinishTimerSet() const { return HasNativePresentationFinishTimer(); }
	EBattlePresentationRecordType ActiveLocalType() const { return GetActiveNativePresentationType(); }
	FPresentationPlaybackToken ActiveLocalToken() const { return GetActiveNativePresentationToken(); }

	void InvokeFinishForTesting(const FPresentationPlaybackToken& Token);
	void InvokeCancelForTesting(const FPresentationPlaybackToken& Token);
	void InvokeNativeDestructForTesting();

	virtual bool PrepareDetachedDamageVisual(
		const FPresentationSessionToken& SessionToken,
		const FPresentationRecord& Record,
		int64 SourceFinalStateRevision,
		float VisualDuration,
		FDetachedDamageToken& OutToken) override;
	virtual bool ActivatePreparedDetachedDamageVisual(
		const FDetachedDamageToken& Token) override;
	virtual bool CancelDetachedDamageVisual(
		const FDetachedDamageToken& Token) override;
	virtual int32 CancelDetachedDamageVisualsForSession(
		const FPresentationSessionToken& SessionToken) override;
	virtual void CancelAllDetachedDamageVisuals() override;
	virtual void PlayCommittedDamageCombatantCues(
		const FPresentationRecord& Record,
		const FPresentationSessionToken& SessionToken) override;

	int32 CancelDispatchCount = 0;
	FPresentationPlaybackToken LastCancelDispatchToken;
	int32 DetachedPrepareCount = 0;
	int32 DetachedActivateCount = 0;
	int32 DetachedCancelCount = 0;
	int32 DetachedCueCount = 0;
	FDetachedDamageToken LastPreparedDetachedToken;
	FDetachedDamageToken LastActivatedDetachedToken;

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
	bool bAcceptSyntheticBlockPlayback = false;
	bool bAcceptDetachedDamage = false;
	bool bForceTimerFailure = false;
	bool bHasPreparedDetachedDamage = false;
	bool bHasActiveDetachedDamage = false;
	int64 NextSyntheticDetachedGeneration = 1;
};
