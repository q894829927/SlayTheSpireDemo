#include "BattleHUDWidget.h"

#include "BattleCardWidget.h"
#include "BattleStatusWidget.h"
#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"

void UBattleHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	bNativeBindingsValid =
		CardWidgetClass != nullptr
		&& StatusWidgetClass != nullptr
		&& IsValid(Combatant_PlayerPresentation)
		&& IsValid(Combatant_EnemyPresentation)
		&& IsValid(HB_Hand)
		&& IsValid(Btn_EndTurn)
		&& IsValid(Btn_Confirm)
		&& IsValid(Btn_Cancel)
		&& IsValid(Txt_Feedback)
		&& IsValid(OV_PlayArea)
		&& IsValid(Txt_DamagePresentation)
		&& IsValid(Overlay_Terminal)
		&& IsValid(PB_PlayerHP)
		&& IsValid(Txt_PlayerHP)
		&& IsValid(Txt_PlayerBlock)
		&& IsValid(WB_PlayerStatuses)
		&& IsValid(PB_EnemyHP)
		&& IsValid(Txt_EnemyHP)
		&& IsValid(Txt_EnemyBlock)
		&& IsValid(WB_EnemyStatuses)
		&& IsValid(Txt_DrawCount)
		&& IsValid(Txt_DiscardCount)
		&& IsValid(Txt_ExhaustCount)
		&& IsValid(Txt_Energy)
		&& IsValid(Txt_Outcome);

	if (!ensureMsgf(
		bNativeBindingsValid,
		TEXT("Native Battle HUD '%s' has a missing dynamic Widget class or required BindWidget control."),
		*GetName()))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[BattleHUD][Native] Invalid required bindings on '%s'; Native input and playback are disabled."),
			*GetPathName());
	}
}

void UBattleHUDWidget::NativeOnBattleHUDViewModelChanged()
{
	// R2 is only the Native shell. Do not call the base implementation: doing so
	// would execute the Legacy BP_OnViewModelChanged graph on the Native stack.
}

bool UBattleHUDWidget::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& /*Record*/,
	const FPresentationPlaybackToken& /*Token*/
)
{
	// R2 intentionally has no migrated presentation behavior. Returning false
	// keeps the existing controller immediate-fallback contract. A broken shell
	// also cannot accidentally claim an asynchronous playback path.
	return false;
}
