#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/BattleHUDCombatantPresentationWidgetBase.h"
#include "UI/BattleHUDWidget.h"
#include "Phase6UIA2NR3TestTypes.generated.h"

class UButton;
class UOverlay;
class UProgressBar;
class UTextBlock;
class UWidget;

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR3StatusTooltipProbe : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void RebuildTooltip(const TArray<FBattleHUDStatusView>& Statuses);

	int32 RebuildCallCount = 0;
	TArray<FBattleHUDStatusView> LastStatuses;
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR3CombatantProbe
	: public UBattleHUDCombatantPresentationWidgetBase
{
	GENERATED_BODY()
};

UCLASS(Transient)
class SLAYTHESPIREDEMOTESTS_API UPhase6UIA2NR3HUDProbe : public UBattleHUDWidget
{
	GENERATED_BODY()

public:
	void ConfigureCombatantSurfaces(
		UBattleHUDCombatantPresentationWidgetBase* InPlayerPresentation,
		UProgressBar* InPlayerHPProgress,
		UTextBlock* InPlayerHPText,
		UTextBlock* InPlayerBlockText,
		UBattleHUDCombatantPresentationWidgetBase* InEnemyPresentation,
		UProgressBar* InEnemyHPProgress,
		UTextBlock* InEnemyHPText,
		UTextBlock* InEnemyBlockText);

	void ConfigureTerminalSurfaces(UOverlay* InTerminalOverlay, UTextBlock* InOutcomeText);
	void ConfigureInputSurfaces(
		UButton* InEndTurn,
		UButton* InConfirm,
		UButton* InCancel,
		UTextBlock* InFeedback);
	void ConfigureEnemyInspectSurfaces(
		UBattleHUDCombatantPresentationWidgetBase* InEnemyPresentation,
		UTextBlock* InEnemyName,
		UWidget* InEnemyStatusTooltip);

	void RefreshCombatantsForTesting();
	void RefreshTerminalForTesting();
	void RefreshInputForTesting();
	void RefreshFeedbackForTesting();
	void InvokeCombatantInspectForTesting(UBattleHUDCombatantPresentationWidgetBase* Presentation);
	void InvokeCombatantInspectClearForTesting(UBattleHUDCombatantPresentationWidgetBase* Presentation);
};
