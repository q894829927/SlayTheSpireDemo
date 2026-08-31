#include "Phase6UIA2NR7TestTypes.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UPhase6UIA2NR7HUDProbe::SetTestWorld(UWorld* InWorld)
{
	TestWorld = InWorld;
}

void UPhase6UIA2NR7HUDProbe::ConfigureDamageSurfaces(
	UBattleHUDCombatantPresentationWidgetBase* InPlayerPresentation,
	UProgressBar* InPlayerHPProgress,
	UTextBlock* InPlayerHPText,
	UTextBlock* InPlayerBlockText,
	UBattleHUDCombatantPresentationWidgetBase* InEnemyPresentation,
	UProgressBar* InEnemyHPProgress,
	UTextBlock* InEnemyHPText,
	UTextBlock* InEnemyBlockText,
	UTextBlock* InDamageText)
{
	Combatant_PlayerPresentation = InPlayerPresentation;
	PB_PlayerHP = InPlayerHPProgress;
	Txt_PlayerHP = InPlayerHPText;
	Txt_PlayerBlock = InPlayerBlockText;
	Combatant_EnemyPresentation = InEnemyPresentation;
	PB_EnemyHP = InEnemyHPProgress;
	Txt_EnemyHP = InEnemyHPText;
	Txt_EnemyBlock = InEnemyBlockText;
	Txt_DamagePresentation = InDamageText;
}

UWorld* UPhase6UIA2NR7HUDProbe::GetWorld() const
{
	return TestWorld.Get();
}

void UPhase6UIA2NR7HUDProbe::InvokeFinishForTesting(
	const FPresentationPlaybackToken& Token)
{
	FinishNativePresentation(Token);
}

void UPhase6UIA2NR7HUDProbe::InvokeCancelForTesting(
	const FPresentationPlaybackToken& Token)
{
	CancelPresentationRecordPlayback(Token);
}

void UPhase6UIA2NR7HUDProbe::InvokeNativeDestructForTesting()
{
	NativeDestruct();
}

void UPhase6UIA2NR7HUDProbe::CancelPresentationRecordPlayback_Implementation(
	const FPresentationPlaybackToken& Token)
{
	++CancelDispatchCount;
	LastCancelDispatchToken = Token;
	Super::CancelPresentationRecordPlayback_Implementation(Token);
}
