#include "Phase6UIA1TestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Phase6UIA1Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLateSubscriberPullBuildsHUDTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.SubscribeThenPullBuildsHUD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FLateSubscriberPullBuildsHUDTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture;
	Fixture.DrainInitialReady();
	TestTrue(TEXT("ViewModel initializes after the initial Ready edge already fired"), Fixture.InitializeViewModel());
	if (!RequireFixture(*this, Fixture)) return false;

	TestEqual(TEXT("Late subscriber immediately pulls the opening Hand"), Fixture.ViewModel->HandCards.Num(), 1);
	TestEqual(TEXT("HUD shows authoritative player HP"), Fixture.ViewModel->Player.HP, 100);
	TestEqual(TEXT("HUD shows authoritative Energy"), Fixture.ViewModel->Energy, 3);
	TestFalse(TEXT("Input is released in stable PlayerTurn"), Fixture.ViewModel->bInputLocked);
	TestEqual(TEXT("Interaction starts Idle"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnplayableEnergyFeedbackTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.UnplayableCardSurfacesGameplayReason",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FUnplayableEnergyFeedbackTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy, 4, 5);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	TestFalse(TEXT("Card view reflects gameplay-owned insufficient-energy validation"), Fixture.ViewModel->HandCards[0].bGameplayPlayable);
	TestFalse(TEXT("Selecting an unplayable card is rejected"), Fixture.ViewModel->SelectCardByRuntimeId(Fixture.FirstRuntimeId()));
	TestFalse(TEXT("Rejected selection exposes player-facing feedback"), Fixture.ViewModel->LastFeedback.IsEmpty());
	TestEqual(TEXT("Rejected selection does not enter Resolving"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FIntentCurrentValueIsCopiedTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.IntentUsesGameplayDerivedCurrentValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FIntentCurrentValueIsCopiedTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy, 0, 5);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	TestEqual(TEXT("HUD Intent is Attack"), Fixture.ViewModel->EnemyIntent.Type, EBattleHUDIntentType::Attack);
	TestEqual(TEXT("Committed BaseAmount remains available"), Fixture.ViewModel->EnemyIntent.BaseAmount, 5);
	TestTrue(TEXT("Gameplay-derived current damage value is available"), Fixture.ViewModel->EnemyIntent.bHasCurrentResolvedDamageAmount);
	TestEqual(TEXT("HUD copies gameplay-derived current value without recomputing rules"), Fixture.ViewModel->EnemyIntent.CurrentResolvedDamageAmount, 5);
	return true;
}

#endif
