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
	FSelfTargetRequiresConfirmAndSubmitsPlayerTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.SelfTargetRequiresConfirmAndSubmitsPlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FSelfTargetRequiresConfirmAndSubmitsPlayerTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Self, 1, 5, 5);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	const int32 RuntimeId = Fixture.FirstRuntimeId();
	TestTrue(TEXT("Self-target card can be selected"), Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId));
	TestTrue(TEXT("Self-target selection keeps runtime identity"), Fixture.ViewModel->SelectedCardRuntimeId != INDEX_NONE);
	TestEqual(TEXT("Self-target card waits for explicit confirmation"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::ReadyToConfirm);
	TestEqual(TEXT("Self-target confirmation candidate is not exposed as a public legal-target button"), Fixture.ViewModel->LegalTargets.Num(), 0);
	TestEqual(
		TEXT("Self-target confirmation exposes only the Player presentation highlight identity"),
		Fixture.ViewModel->PendingConfirmationTargetPresentationId,
		Fixture.ViewModel->Player.PresentationId
	);

	TestTrue(TEXT("Self-target confirm submits the gameplay-derived Player candidate"), Fixture.ViewModel->ConfirmSelectedCard());
	TestEqual(TEXT("Accepted self-target request enters Resolving"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Resolving);
	TestTrue(TEXT("Accepted self-target request locks input before Ready"), Fixture.ViewModel->bInputLocked);

	Fixture.FlushReady();
	TestEqual(TEXT("Ready refresh returns self-target flow to Idle"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Idle);
	TestEqual(TEXT("Self-target Defend path grants Block"), Fixture.ViewModel->Player.Block, 5);
	TestEqual(TEXT("Self-target Defend path spends Energy"), Fixture.ViewModel->Energy, 2);
	TestEqual(TEXT("Self-target card leaves Hand"), Fixture.ViewModel->HandCards.Num(), 0);
	TestEqual(TEXT("Self-target card reaches Discard"), Fixture.ViewModel->DiscardCount, 1);
	TestEqual(TEXT("Ready refresh clears selected runtime identity"), Fixture.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestEqual(TEXT("Ready refresh keeps public legal targets empty"), Fixture.ViewModel->LegalTargets.Num(), 0);
	TestTrue(TEXT("Ready refresh clears the confirmation highlight identity"), Fixture.ViewModel->PendingConfirmationTargetPresentationId.IsNone());
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
