#include "Phase6UIA1TestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Phase6UIA1Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionUsesLegalTargetsAndCancelTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.SelectionUsesLegalTargetsAndCancelIsPresentationOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FSelectionUsesLegalTargetsAndCancelTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	const int32 RuntimeId = Fixture.FirstRuntimeId();
	TestTrue(TEXT("Enemy-target card can be selected"), Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId));
	TestEqual(TEXT("Selection enters ChoosingTarget"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::ChoosingTarget);
	TestEqual(TEXT("Legal target set comes from gameplay"), Fixture.ViewModel->LegalTargets.Num(), 1);
	TestFalse(TEXT("Enemy target is not mislabeled as player"), Fixture.ViewModel->LegalTargets[0].bPlayer);
	TestEqual(TEXT("Legal target maps to the enemy presentation identity"), Fixture.ViewModel->LegalTargets[0].PresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("Legal target uses the combatant display name"), Fixture.ViewModel->LegalTargets[0].DisplayName.ToString(), FString(TEXT("Cultist")));
	FBattleHUDTargetView EnemyTargetByPresentation;
	TestTrue(
		TEXT("Enemy legal target can be resolved by presentation identity"),
		Fixture.ViewModel->TryGetLegalTargetByPresentationId(Fixture.ViewModel->Enemy.PresentationId, EnemyTargetByPresentation)
	);
	TestEqual(TEXT("Presentation lookup preserves the Enemy target id"), EnemyTargetByPresentation.TargetId, Fixture.ViewModel->LegalTargets[0].TargetId);

	Fixture.ViewModel->CancelSelection();
	TestEqual(TEXT("Cancel returns to Idle"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Idle);
	TestEqual(TEXT("Cancel clears selected runtime identity"), Fixture.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestEqual(TEXT("Cancel does not mutate authoritative Hand"), Fixture.ViewModel->HandCards.Num(), 1);
	TestEqual(TEXT("Cancel does not discard the card"), Fixture.ViewModel->DiscardCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNoTargetConfirmationLocksUntilReadyTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.NoTargetRequiresConfirmAndLocksUntilReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNoTargetConfirmationLocksUntilReadyTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::None);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	TestTrue(TEXT("No-target card can be selected"), Fixture.ViewModel->SelectCardByRuntimeId(Fixture.FirstRuntimeId()));
	TestEqual(TEXT("No-target card waits for explicit confirmation"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::ReadyToConfirm);
	TestTrue(TEXT("Confirm submits formal RequestPlayCard"), Fixture.ViewModel->ConfirmSelectedCard());
	TestEqual(TEXT("Accepted request enters Resolving before Ready"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Resolving);
	TestTrue(TEXT("Accepted request locks input before Ready"), Fixture.ViewModel->bInputLocked);
	TestEqual(TEXT("ViewModel has not refreshed stale Hand before Ready"), Fixture.ViewModel->HandCards.Num(), 1);

	Fixture.FlushReady();
	TestEqual(TEXT("Stable Ready refresh returns ViewModel to Idle"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Idle);
	TestFalse(TEXT("Stable Ready releases input"), Fixture.ViewModel->bInputLocked);
	TestEqual(TEXT("Played card leaves Hand after stable refresh"), Fixture.ViewModel->HandCards.Num(), 0);
	TestEqual(TEXT("Played card appears in Discard count"), Fixture.ViewModel->DiscardCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelfTargetUsesLegalPlayerSelectionTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.SelfTargetUsesLegalPlayerSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FSelfTargetUsesLegalPlayerSelectionTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Self, 1, 5, 5);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	const int32 RuntimeId = Fixture.FirstRuntimeId();
	TestTrue(TEXT("Self-target card can be selected"), Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId));
	TestTrue(TEXT("Self-target selection keeps runtime identity"), Fixture.ViewModel->SelectedCardRuntimeId != INDEX_NONE);
	TestEqual(TEXT("Self-target card enters target selection"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::ChoosingTarget);
	TestEqual(TEXT("Self-target card exposes one gameplay-provided target"), Fixture.ViewModel->LegalTargets.Num(), 1);
	if (Fixture.ViewModel->LegalTargets.Num() != 1)
	{
		return false;
	}
	TestTrue(TEXT("Self-target legal target is identified as Player"), Fixture.ViewModel->LegalTargets[0].bPlayer);
	TestEqual(TEXT("Self-target maps to the Player presentation"), Fixture.ViewModel->LegalTargets[0].PresentationId, Fixture.ViewModel->Player.PresentationId);
	FBattleHUDTargetView PlayerTargetByPresentation;
	TestTrue(
		TEXT("Player legal target can be resolved by presentation identity"),
		Fixture.ViewModel->TryGetLegalTargetByPresentationId(Fixture.ViewModel->Player.PresentationId, PlayerTargetByPresentation)
	);
	TestEqual(TEXT("Presentation lookup preserves the Player target id"), PlayerTargetByPresentation.TargetId, Fixture.ViewModel->LegalTargets[0].TargetId);

	TestTrue(TEXT("Selecting the gameplay-provided Player target submits the card"), Fixture.ViewModel->SelectTargetById(PlayerTargetByPresentation.TargetId));
	TestEqual(TEXT("Accepted self-target request enters Resolving"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Resolving);
	TestTrue(TEXT("Accepted self-target request locks input before Ready"), Fixture.ViewModel->bInputLocked);

	Fixture.FlushReady();
	TestEqual(TEXT("Ready refresh returns self-target flow to Idle"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Idle);
	TestEqual(TEXT("Self-target Defend path grants Block"), Fixture.ViewModel->Player.Block, 5);
	TestEqual(TEXT("Self-target Defend path spends Energy"), Fixture.ViewModel->Energy, 2);
	TestEqual(TEXT("Self-target card leaves Hand"), Fixture.ViewModel->HandCards.Num(), 0);
	TestEqual(TEXT("Self-target card reaches Discard"), Fixture.ViewModel->DiscardCount, 1);
	TestEqual(TEXT("Ready refresh clears selected runtime identity"), Fixture.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestEqual(TEXT("Ready refresh clears public legal targets"), Fixture.ViewModel->LegalTargets.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTargetRequestLocksUntilReadyTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.TargetRequestLocksUntilReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FTargetRequestLocksUntilReadyTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	Fixture.ViewModel->SelectCardByRuntimeId(Fixture.FirstRuntimeId());
	const int32 TargetId = Fixture.ViewModel->LegalTargets[0].TargetId;
	TestTrue(TEXT("Selecting gameplay-provided target submits the request"), Fixture.ViewModel->SelectTargetById(TargetId));
	TestEqual(TEXT("Targeted accepted request enters Resolving"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Resolving);
	TestTrue(TEXT("Targeted accepted request locks input"), Fixture.ViewModel->bInputLocked);

	Fixture.FlushReady();
	TestEqual(TEXT("Ready refresh returns targeted flow to Idle"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Idle);
	TestEqual(TEXT("Targeted card resolves out of Hand"), Fixture.ViewModel->HandCards.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEndTurnLocksUntilReadyTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.EndTurnLocksUntilReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FEndTurnLocksUntilReadyTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy, 0, 5);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	TestTrue(TEXT("End Turn is enabled from stable PlayerTurn"), Fixture.ViewModel->bCanEndTurn);
	TestTrue(TEXT("End Turn submits formal request"), Fixture.ViewModel->RequestEndTurn());
	TestEqual(TEXT("Accepted end-turn request locks ViewModel"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Resolving);
	TestTrue(TEXT("End-turn lock remains until Ready"), Fixture.ViewModel->bInputLocked);

	Fixture.FlushReady();
	TestEqual(TEXT("Full player-enemy-player macro flow publishes final Idle"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Idle);
	TestEqual(TEXT("Enemy committed attack is reflected after Ready"), Fixture.ViewModel->Player.HP, 95);
	TestEqual(TEXT("Remaining Hand cleanup is reflected after Ready"), Fixture.ViewModel->HandCards.Num(), 0);
	return true;
}

#endif
