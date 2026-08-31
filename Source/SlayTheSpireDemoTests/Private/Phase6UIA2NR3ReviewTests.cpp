#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR3TestTypes.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2NR3Review
{
	struct FBlockSurface
	{
		USizeBox* Badge = nullptr;
		UOverlay* Overlay = nullptr;
		UTextBlock* Text = nullptr;
	};

	FBlockSurface CreateBlockSurface(UObject* Outer)
	{
		FBlockSurface Surface;
		Surface.Badge = NewObject<USizeBox>(Outer);
		Surface.Overlay = NewObject<UOverlay>(Outer);
		Surface.Text = NewObject<UTextBlock>(Outer);
		if (IsValid(Surface.Badge) && IsValid(Surface.Overlay) && IsValid(Surface.Text))
		{
			Surface.Badge->AddChild(Surface.Overlay);
			Surface.Overlay->AddChild(Surface.Text);
		}
		return Surface;
	}

	bool IsValidBlockSurface(const FBlockSurface& Surface)
	{
		return IsValid(Surface.Badge) && IsValid(Surface.Overlay) && IsValid(Surface.Text);
	}
}

using namespace Phase6UIA2NR3Review;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeBlockBadgeParityTest,
	"SlayTheSpireDemo.Phase6UIA2N.R3.BlockBadge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNativeBlockBadgeParityTest::RunTest(const FString& Parameters)
{
	UPhase6UIA2NR3HUDProbe* HUD = NewObject<UPhase6UIA2NR3HUDProbe>(GetTransientPackage());
	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(HUD);
	UPhase6UIA2NR3CombatantProbe* PlayerPresentation = NewObject<UPhase6UIA2NR3CombatantProbe>(HUD);
	UPhase6UIA2NR3CombatantProbe* EnemyPresentation = NewObject<UPhase6UIA2NR3CombatantProbe>(HUD);
	UProgressBar* PlayerHP = NewObject<UProgressBar>(HUD);
	UProgressBar* EnemyHP = NewObject<UProgressBar>(HUD);
	UTextBlock* PlayerHPText = NewObject<UTextBlock>(HUD);
	UTextBlock* EnemyHPText = NewObject<UTextBlock>(HUD);
	const FBlockSurface PlayerBlock = CreateBlockSurface(HUD);
	const FBlockSurface EnemyBlock = CreateBlockSurface(HUD);

	if (!IsValid(HUD) || !IsValid(ViewModel) || !IsValid(PlayerPresentation)
		|| !IsValid(EnemyPresentation) || !IsValid(PlayerHP) || !IsValid(EnemyHP)
		|| !IsValid(PlayerHPText) || !IsValid(EnemyHPText)
		|| !IsValidBlockSurface(PlayerBlock) || !IsValidBlockSurface(EnemyBlock))
	{
		AddError(TEXT("Failed to build R3 block-badge fixture."));
		return false;
	}

	HUD->ViewModel = ViewModel;
	HUD->ConfigureCombatantSurfaces(
		PlayerPresentation,
		PlayerHP,
		PlayerHPText,
		PlayerBlock.Text,
		EnemyPresentation,
		EnemyHP,
		EnemyHPText,
		EnemyBlock.Text);

	ViewModel->Player.PresentationId = TEXT("Player");
	ViewModel->Player.HP = 80;
	ViewModel->Player.MaxHP = 80;
	ViewModel->Player.Block = 0;
	ViewModel->Enemy.PresentationId = TEXT("Enemy");
	ViewModel->Enemy.HP = 100;
	ViewModel->Enemy.MaxHP = 100;
	ViewModel->Enemy.Block = 0;

	HUD->RefreshCombatantsForTesting();
	TestTrue(TEXT("Player zero Block collapses the whole badge"), PlayerBlock.Badge->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Enemy zero Block collapses the whole badge"), EnemyBlock.Badge->GetVisibility() == ESlateVisibility::Collapsed);

	ViewModel->Player.Block = 7;
	ViewModel->Enemy.Block = 3;
	HUD->RefreshCombatantsForTesting();
	TestTrue(TEXT("Positive Player Block shows the badge"), PlayerBlock.Badge->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	TestTrue(TEXT("Positive Enemy Block shows the badge"), EnemyBlock.Badge->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Player Block text uses the frozen value"), PlayerBlock.Text->GetText().ToString(), FString(TEXT("7")));
	TestEqual(TEXT("Enemy Block text uses the frozen value"), EnemyBlock.Text->GetText().ToString(), FString(TEXT("3")));

	ViewModel->Player.Block = 0;
	ViewModel->Enemy.Block = 0;
	HUD->RefreshCombatantsForTesting();
	TestTrue(TEXT("Player badge collapses again when Block returns to zero"), PlayerBlock.Badge->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Enemy badge collapses again when Block returns to zero"), EnemyBlock.Badge->GetVisibility() == ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeFrozenStatusTooltipParityTest,
	"SlayTheSpireDemo.Phase6UIA2N.R3.StatusTooltip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNativeFrozenStatusTooltipParityTest::RunTest(const FString& Parameters)
{
	UPhase6UIA2NR3HUDProbe* HUD = NewObject<UPhase6UIA2NR3HUDProbe>(GetTransientPackage());
	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(HUD);
	UPhase6UIA2NR3CombatantProbe* EnemyPresentation = NewObject<UPhase6UIA2NR3CombatantProbe>(HUD);
	UTextBlock* EnemyName = NewObject<UTextBlock>(HUD);
	UPhase6UIA2NR3StatusTooltipProbe* Tooltip = NewObject<UPhase6UIA2NR3StatusTooltipProbe>(HUD);

	if (!IsValid(HUD) || !IsValid(ViewModel) || !IsValid(EnemyPresentation)
		|| !IsValid(EnemyName) || !IsValid(Tooltip))
	{
		AddError(TEXT("Failed to build R3 status-tooltip fixture."));
		return false;
	}

	HUD->ViewModel = ViewModel;
	HUD->ConfigureEnemyInspectSurfaces(EnemyPresentation, EnemyName, Tooltip);
	Tooltip->SetVisibility(ESlateVisibility::Collapsed);

	ViewModel->Enemy.DisplayName = FText::FromString(TEXT("Cultist"));
	FBattleHUDStatusView Status;
	Status.StatusId = TEXT("TestStrength");
	Status.RuntimeSequence = 41;
	Status.DisplayName = FText::FromString(TEXT("力量"));
	Status.Description = FText::FromString(TEXT("测试冻结状态"));
	Status.Amount = 2;
	ViewModel->Enemy.Statuses = {Status};

	HUD->InvokeCombatantInspectForTesting(EnemyPresentation);
	TestEqual(TEXT("Inspect rebuilds the optional tooltip exactly once"), Tooltip->RebuildCallCount, 1);
	TestEqual(TEXT("Tooltip receives exactly one frozen status"), Tooltip->LastStatuses.Num(), 1);
	if (Tooltip->LastStatuses.Num() == 1)
	{
		const FBattleHUDStatusView& Received = Tooltip->LastStatuses[0];
		TestEqual(TEXT("StatusId is forwarded from the frozen DTO"), Received.StatusId, FName(TEXT("TestStrength")));
		TestEqual(TEXT("RuntimeSequence is forwarded from the frozen DTO"), Received.RuntimeSequence, static_cast<int64>(41));
		TestEqual(TEXT("Status display name is forwarded"), Received.DisplayName.ToString(), FString(TEXT("力量")));
		TestEqual(TEXT("Status amount is forwarded"), Received.Amount, 2);
	}
	TestTrue(TEXT("Successful frozen rebuild shows the tooltip"), Tooltip->GetVisibility() == ESlateVisibility::Visible);
	TestEqual(TEXT("Inspect name uses the frozen combatant display name"), EnemyName->GetText().ToString(), FString(TEXT("Cultist")));

	HUD->InvokeCombatantInspectClearForTesting(EnemyPresentation);
	TestTrue(TEXT("Inspect clear collapses the tooltip"), Tooltip->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Inspect clear hides the optional name surface"), EnemyName->GetVisibility() == ESlateVisibility::Hidden);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeTerminalHistoricalSurfaceParityTest,
	"SlayTheSpireDemo.Phase6UIA2N.R3.Terminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNativeTerminalHistoricalSurfaceParityTest::RunTest(const FString& Parameters)
{
	UPhase6UIA2NR3HUDProbe* HUD = NewObject<UPhase6UIA2NR3HUDProbe>(GetTransientPackage());
	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(HUD);
	UOverlay* TerminalOverlay = NewObject<UOverlay>(HUD);
	UTextBlock* OutcomeText = NewObject<UTextBlock>(HUD);
	if (!IsValid(HUD) || !IsValid(ViewModel) || !IsValid(TerminalOverlay) || !IsValid(OutcomeText))
	{
		AddError(TEXT("Failed to build R3 terminal fixture."));
		return false;
	}

	HUD->ViewModel = ViewModel;
	HUD->ConfigureTerminalSurfaces(TerminalOverlay, OutcomeText);

	ViewModel->Outcome = EBattleHUDOutcome::None;
	HUD->RefreshTerminalForTesting();
	TestTrue(TEXT("Outcome None keeps terminal collapsed"), TerminalOverlay->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Outcome None clears terminal text"), OutcomeText->GetText().IsEmpty());

	ViewModel->Outcome = EBattleHUDOutcome::Victory;
	HUD->RefreshTerminalForTesting();
	TestTrue(TEXT("Victory shows terminal overlay"), TerminalOverlay->GetVisibility() == ESlateVisibility::Visible);
	TestEqual(TEXT("Victory uses the sealed terminal text"), OutcomeText->GetText().ToString(), FString(TEXT("胜利")));

	ViewModel->Outcome = EBattleHUDOutcome::Defeat;
	HUD->RefreshTerminalForTesting();
	TestTrue(TEXT("Defeat shows terminal overlay"), TerminalOverlay->GetVisibility() == ESlateVisibility::Visible);
	TestEqual(TEXT("Defeat uses the sealed terminal text"), OutcomeText->GetText().ToString(), FString(TEXT("战斗失败")));

	ViewModel->Outcome = EBattleHUDOutcome::ResolutionFaulted;
	HUD->RefreshTerminalForTesting();
	TestTrue(TEXT("ResolutionFaulted shows terminal overlay"), TerminalOverlay->GetVisibility() == ESlateVisibility::Visible);
	TestEqual(TEXT("ResolutionFaulted uses the sealed terminal text"), OutcomeText->GetText().ToString(), FString(TEXT("战斗结算异常")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativePresentationUnavailableParityTest,
	"SlayTheSpireDemo.Phase6UIA2N.R3.PresentationUnavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNativePresentationUnavailableParityTest::RunTest(const FString& Parameters)
{
	UPhase6UIA2NR3HUDProbe* HUD = NewObject<UPhase6UIA2NR3HUDProbe>(GetTransientPackage());
	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(HUD);
	UButton* EndTurn = NewObject<UButton>(HUD);
	UButton* Confirm = NewObject<UButton>(HUD);
	UButton* Cancel = NewObject<UButton>(HUD);
	UTextBlock* Feedback = NewObject<UTextBlock>(HUD);
	UOverlay* TerminalOverlay = NewObject<UOverlay>(HUD);
	UTextBlock* OutcomeText = NewObject<UTextBlock>(HUD);

	if (!IsValid(HUD) || !IsValid(ViewModel) || !IsValid(EndTurn)
		|| !IsValid(Confirm) || !IsValid(Cancel) || !IsValid(Feedback)
		|| !IsValid(TerminalOverlay) || !IsValid(OutcomeText))
	{
		AddError(TEXT("Failed to build R3 PresentationUnavailable fixture."));
		return false;
	}

	HUD->ViewModel = ViewModel;
	HUD->ConfigureInputSurfaces(EndTurn, Confirm, Cancel, Feedback);
	HUD->ConfigureTerminalSurfaces(TerminalOverlay, OutcomeText);

	ViewModel->Outcome = EBattleHUDOutcome::None;
	ViewModel->InteractionState = EBattleHUDInteractionState::Idle;
	ViewModel->bInputLocked = false;
	ViewModel->bCanEndTurn = true;
	ViewModel->EnterPresentationUnavailable(FText::FromString(TEXT("TEST_PRESENTATION_UNAVAILABLE")));

	HUD->RefreshInputForTesting();
	HUD->RefreshFeedbackForTesting();
	HUD->RefreshTerminalForTesting();

	TestEqual(TEXT("PresentationUnavailable uses its dedicated interaction state"), ViewModel->InteractionState, EBattleHUDInteractionState::PresentationUnavailable);
	TestEqual(TEXT("PresentationUnavailable does not become a Gameplay terminal outcome"), ViewModel->Outcome, EBattleHUDOutcome::None);
	TestTrue(TEXT("PresentationUnavailable locks input"), ViewModel->bInputLocked);
	TestFalse(TEXT("PresentationUnavailable disables End Turn eligibility"), ViewModel->bCanEndTurn);
	TestEqual(TEXT("PresentationUnavailable renders the ViewModel failure reason"), Feedback->GetText().ToString(), FString(TEXT("TEST_PRESENTATION_UNAVAILABLE")));
	TestFalse(TEXT("End Turn is disabled"), EndTurn->GetIsEnabled());
	TestTrue(TEXT("Confirm is collapsed"), Confirm->GetVisibility() == ESlateVisibility::Collapsed);
	TestFalse(TEXT("Confirm is disabled"), Confirm->GetIsEnabled());
	TestTrue(TEXT("Cancel is collapsed"), Cancel->GetVisibility() == ESlateVisibility::Collapsed);
	TestFalse(TEXT("Cancel is disabled"), Cancel->GetIsEnabled());
	TestTrue(TEXT("PresentationUnavailable does not show the terminal overlay"), TerminalOverlay->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("PresentationUnavailable leaves terminal outcome text empty"), OutcomeText->GetText().IsEmpty());
	return true;
}

#endif
