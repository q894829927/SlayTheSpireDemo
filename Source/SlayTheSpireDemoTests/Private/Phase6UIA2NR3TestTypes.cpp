#include "Phase6UIA2NR3TestTypes.h"

#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UPhase6UIA2NR3StatusTooltipProbe::RebuildTooltip(
	const TArray<FBattleHUDStatusView>& Statuses)
{
	++RebuildCallCount;
	LastStatuses = Statuses;
}

void UPhase6UIA2NR3HUDProbe::ConfigureCombatantSurfaces(
	UBattleHUDCombatantPresentationWidgetBase* InPlayerPresentation,
	UProgressBar* InPlayerHPProgress,
	UTextBlock* InPlayerHPText,
	UTextBlock* InPlayerBlockText,
	UBattleHUDCombatantPresentationWidgetBase* InEnemyPresentation,
	UProgressBar* InEnemyHPProgress,
	UTextBlock* InEnemyHPText,
	UTextBlock* InEnemyBlockText)
{
	Combatant_PlayerPresentation = InPlayerPresentation;
	PB_PlayerHP = InPlayerHPProgress;
	Txt_PlayerHP = InPlayerHPText;
	Txt_PlayerBlock = InPlayerBlockText;
	Combatant_EnemyPresentation = InEnemyPresentation;
	PB_EnemyHP = InEnemyHPProgress;
	Txt_EnemyHP = InEnemyHPText;
	Txt_EnemyBlock = InEnemyBlockText;
}

void UPhase6UIA2NR3HUDProbe::ConfigureTerminalSurfaces(
	UOverlay* InTerminalOverlay,
	UTextBlock* InOutcomeText)
{
	Overlay_Terminal = InTerminalOverlay;
	Txt_Outcome = InOutcomeText;
}

void UPhase6UIA2NR3HUDProbe::ConfigureInputSurfaces(
	UButton* InEndTurn,
	UButton* InConfirm,
	UButton* InCancel,
	UTextBlock* InFeedback)
{
	Btn_EndTurn = InEndTurn;
	Btn_Confirm = InConfirm;
	Btn_Cancel = InCancel;
	Txt_Feedback = InFeedback;
}

void UPhase6UIA2NR3HUDProbe::ConfigureEnemyInspectSurfaces(
	UBattleHUDCombatantPresentationWidgetBase* InEnemyPresentation,
	UTextBlock* InEnemyName,
	UWidget* InEnemyStatusTooltip)
{
	Combatant_EnemyPresentation = InEnemyPresentation;
	Txt_EnemyName = InEnemyName;
	StatusTooltip_Enemy = InEnemyStatusTooltip;
}

void UPhase6UIA2NR3HUDProbe::RefreshCombatantsForTesting()
{
	RefreshCombatants();
}

void UPhase6UIA2NR3HUDProbe::RefreshTerminalForTesting()
{
	RefreshTerminalFromViewModel();
}

void UPhase6UIA2NR3HUDProbe::RefreshInputForTesting()
{
	RefreshInputState();
}

void UPhase6UIA2NR3HUDProbe::RefreshFeedbackForTesting()
{
	RefreshFeedback();
}

void UPhase6UIA2NR3HUDProbe::InvokeCombatantInspectForTesting(
	UBattleHUDCombatantPresentationWidgetBase* Presentation)
{
	HandleCombatantInspectRequested(Presentation);
}

void UPhase6UIA2NR3HUDProbe::InvokeCombatantInspectClearForTesting(
	UBattleHUDCombatantPresentationWidgetBase* Presentation)
{
	HandleCombatantInspectCleared(Presentation);
}
