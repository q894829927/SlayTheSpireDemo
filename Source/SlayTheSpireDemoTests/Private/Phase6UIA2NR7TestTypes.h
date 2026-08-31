#pragma once

#include "CoreMinimal.h"
#include "UI/BattleHUDCombatantPresentationWidgetBase.h"
#include "UI/BattleHUDWidget.h"
#include "Phase6UIA2NR7TestTypes.generated.h"

class UProgressBar;
class UTextBlock;
class UWorld;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR7CombatantProbe
	: public UBattleHUDCombatantPresentationWidgetBase
{
	GENERATED_BODY()
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR7HUDProbe : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
	void SetTestWorld(UWorld* InWorld);
	void ConfigureDamageSurfaces(
		UBattleHUDCombatantPresentationWidgetBase* InPlayerPresentation,
		UProgressBar* InPlayerHPProgress,
		UTextBlock* InPlayerHPText,
		UTextBlock* InPlayerBlockText,
		UBattleHUDCombatantPresentationWidgetBase* InEnemyPresentation,
		UProgressBar* InEnemyHPProgress,
		UTextBlock* InEnemyHPText,
		UTextBlock* InEnemyBlockText,
		UTextBlock* InDamageText);

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
