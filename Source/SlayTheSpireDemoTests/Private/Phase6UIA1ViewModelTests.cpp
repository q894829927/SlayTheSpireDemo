#include "Phase6UIA1TestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Status/StatusContainer.h"
#include "Status/StatusData.h"

using namespace Phase6UIA1Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLateSubscriberPullBuildsHUDTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.SubscribeThenPullBuildsHUD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FLateSubscriberPullBuildsHUDTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture;

	UStatusData* StatusDefinition = IsValid(Fixture.World)
		? NewObject<UStatusData>(Fixture.World)
		: nullptr;
	UStatusContainer* StatusContainer = IsValid(Fixture.Player)
		? Fixture.Player->GetStatusContainer()
		: nullptr;
	if (!IsValid(StatusDefinition) || !IsValid(StatusContainer))
	{
		AddError(TEXT("Expected a valid status definition and player StatusContainer."));
		return false;
	}

	StatusDefinition->StatusId = TEXT("AtlasStatus");
	StatusDefinition->DisplayName = FText::FromString(TEXT("Atlas Status"));
	StatusDefinition->IconRegion.bUseAtlasIcon = true;
	StatusDefinition->IconRegion.UVOffset = FVector2D(0.25, 0.65);
	StatusDefinition->IconRegion.UVScale = FVector2D(0.02, 0.03);
	StatusDefinition->IconRegion.TrimOffset = FVector2D(0.06, 0.27);
	StatusDefinition->IconRegion.TrimScale = FVector2D(0.88, 0.54);

	bool bCreated = false;
	TestNotNull(
		TEXT("Test status is applied before the ViewModel initial pull"),
		StatusContainer->ApplyStatus(StatusDefinition, 2, 777, bCreated)
	);
	TestTrue(TEXT("Test status creates a new runtime instance"), bCreated);

	Fixture.DrainInitialReady();
	TestTrue(TEXT("ViewModel initializes after the initial Ready edge already fired"), Fixture.InitializeViewModel());
	if (!RequireFixture(*this, Fixture)) return false;

	TestEqual(TEXT("Late subscriber immediately pulls the opening Hand"), Fixture.ViewModel->HandCards.Num(), 1);
	TestEqual(TEXT("HUD shows authoritative player HP"), Fixture.ViewModel->Player.HP, 100);
	TestEqual(TEXT("HUD shows authoritative Energy"), Fixture.ViewModel->Energy, 3);
	TestFalse(TEXT("Input is released in stable PlayerTurn"), Fixture.ViewModel->bInputLocked);
	TestEqual(TEXT("Interaction starts Idle"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Idle);

	TestEqual(TEXT("HUD receives the current player status"), Fixture.ViewModel->Player.Statuses.Num(), 1);
	if (Fixture.ViewModel->Player.Statuses.Num() == 1)
	{
		const FBattleHUDStatusView& StatusView = Fixture.ViewModel->Player.Statuses[0];
		TestEqual(TEXT("HUD status id is preserved"), StatusView.StatusId, FName(TEXT("AtlasStatus")));
		TestEqual(TEXT("HUD status display name comes from StatusData"), StatusView.DisplayName.ToString(), FString(TEXT("Atlas Status")));
		TestEqual(TEXT("HUD status amount is preserved"), StatusView.Amount, 2);
		TestTrue(TEXT("HUD atlas icon flag comes from StatusData"), StatusView.bUseAtlasIcon);
		TestTrue(TEXT("HUD UVOffset comes from StatusData"), StatusView.UVOffset.Equals(FVector2D(0.25, 0.65)));
		TestTrue(TEXT("HUD UVScale comes from StatusData"), StatusView.UVScale.Equals(FVector2D(0.02, 0.03)));
		TestTrue(TEXT("HUD TrimOffset comes from StatusData"), StatusView.TrimOffset.Equals(FVector2D(0.06, 0.27)));
		TestTrue(TEXT("HUD TrimScale comes from StatusData"), StatusView.TrimScale.Equals(FVector2D(0.88, 0.54)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCardPresentationFieldsComeFromDefinitionTest,
	"SlayTheSpireDemo.Phase6UIA1.ViewModel.CardPresentationFieldsComeFromDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCardPresentationFieldsComeFromDefinitionTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture;
	UCardInstance* Card = Fixture.FirstAuthoritativeHandCard();
	UCardData* Definition = IsValid(Card) ? const_cast<UCardData*>(Card->GetDefinition()) : nullptr;
	if (!IsValid(Definition))
	{
		AddError(TEXT("Expected authoritative opening-hand card definition."));
		return false;
	}

	UTexture2D* TestCardArt = NewObject<UTexture2D>(GetTransientPackage());
	Definition->CardType = ECardType::Skill;
	Definition->Description = FText::FromString(TEXT("Draw 1 card."));
	Definition->CardArt = TestCardArt;

	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	TestEqual(TEXT("HUD card type is copied from the card definition"), Fixture.ViewModel->HandCards[0].CardType, ECardType::Skill);
	TestEqual(TEXT("HUD card description is copied from the card definition"), Fixture.ViewModel->HandCards[0].Description.ToString(), FString(TEXT("Draw 1 card.")));
	TestTrue(TEXT("HUD card art is copied from the card definition"), Fixture.ViewModel->HandCards[0].CardArt == TestCardArt);
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
