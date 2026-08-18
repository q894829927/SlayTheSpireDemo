#include "Phase6UIA1TestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Phase6UIA1Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExternalResolutionClearsSelectionTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.ReadyRefreshClearsStaleSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FExternalResolutionClearsSelectionTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	const int32 RuntimeId = Fixture.FirstRuntimeId();
	Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId);
	UCardInstance* Card = Fixture.FirstAuthoritativeHandCard();
	TestNotNull(TEXT("Authoritative card remains available for external formal request"), Card);
	if (!Card) return false;
	TestTrue(TEXT("External formal request is accepted"), Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy).IsAcceptedForResolution());
	TestEqual(TEXT("Presentation selection remains until stable Ready"), Fixture.ViewModel->SelectedCardRuntimeId, RuntimeId);

	Fixture.FlushReady();
	TestEqual(TEXT("Stable Ready clears stale selected identity"), Fixture.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestEqual(TEXT("Stable Ready clears obsolete legal targets"), Fixture.ViewModel->LegalTargets.Num(), 0);
	TestEqual(TEXT("Stable Ready reflects authoritative Hand change"), Fixture.ViewModel->HandCards.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FResolutionFaultVisibleTerminalTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.ResolutionFaultIsVisibleTerminalState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FResolutionFaultVisibleTerminalTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture;
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution fault requested:"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("[Battle] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);

	TestTrue(TEXT("Fault request is accepted by Queue"), Fixture.Battle->GetActionQueueForTesting()->RequestResolutionFault(TEXT("UI-A1 visible fault regression.")));
	TestNotEqual(TEXT("ViewModel does not claim terminal until public Ready arrives"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Terminal);
	Fixture.FlushReady();

	TestEqual(TEXT("ResolutionFaulted becomes a visible HUD outcome"), Fixture.ViewModel->Outcome, EBattleHUDOutcome::ResolutionFaulted);
	TestEqual(TEXT("ResolutionFaulted moves interaction to Terminal"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Terminal);
	TestTrue(TEXT("Terminal fault keeps player input locked"), Fixture.ViewModel->bInputLocked);
	TestFalse(TEXT("Terminal fault disables End Turn"), Fixture.ViewModel->bCanEndTurn);
	return true;
}

#endif
