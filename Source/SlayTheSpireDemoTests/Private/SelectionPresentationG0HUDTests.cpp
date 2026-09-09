#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SelectionPresentationG0TestTypes.h"
#include "Components/HorizontalBox.h"
#include "Engine/World.h"
#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG0HUDTest
{
	FPresentationStateSnapshot MakeSnapshot(
		int64 BattleId,
		int64 StateRevision,
		std::initializer_list<int32> HandRuntimeIds)
	{
		FPresentationStateSnapshot Snapshot;
		Snapshot.BattleId = BattleId;
		Snapshot.StateRevision = StateRevision;
		Snapshot.BattleState = EBattleState::PlayerTurn;
		Snapshot.Outcome = EBattleHUDOutcome::None;
		Snapshot.Energy = 3;
		Snapshot.MaxEnergy = 3;
		Snapshot.bCanEndTurn = true;
		Snapshot.Player.PresentationId = TEXT("Player");
		Snapshot.Player.bPlayer = true;
		Snapshot.Player.DisplayName = FText::FromString(TEXT("Player"));
		Snapshot.Player.HP = 80;
		Snapshot.Player.MaxHP = 80;
		Snapshot.Enemy.PresentationId = TEXT("Enemy");
		Snapshot.Enemy.DisplayName = FText::FromString(TEXT("Enemy"));
		Snapshot.Enemy.HP = 40;
		Snapshot.Enemy.MaxHP = 40;

		for (const int32 RuntimeId : HandRuntimeIds)
		{
			FBattleHUDCardView Card;
			Card.RuntimeId = RuntimeId;
			Card.CardId = FName(*FString::Printf(TEXT("HUD_%d"), RuntimeId));
			Card.DisplayName = FText::FromString(FString::Printf(TEXT("Card%d"), RuntimeId));
			Card.Cost = 1;
			Card.CardType = ECardType::Skill;
			Card.Rarity = ECardRarity::Common;
			Card.CardColor = ECardColor::Red;
			Card.TargetType = ECardTargetType::None;
			Snapshot.HandCards.Add(Card);
		}
		return Snapshot;
	}

	struct FProbeFixture
	{
		UWorld* World = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		USelectionPresentationG0HUDProbe* HUD = nullptr;
		UHorizontalBox* Hand = nullptr;

		FProbeFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;
			ViewModel = NewObject<UBattleHUDViewModel>(World);
			HUD = NewObject<USelectionPresentationG0HUDProbe>(World);
			Hand = NewObject<UHorizontalBox>(HUD);
			if (!IsValid(ViewModel) || !IsValid(HUD) || !IsValid(Hand)) return;
			HUD->SetTestWorld(World);
		}

		~FProbeFixture()
		{
			if (IsValid(HUD)) HUD->SetViewModel(nullptr);
			if (IsValid(World)) World->DestroyWorld(false);
		}

		bool IsValidFixture() const
		{
			return IsValid(World) && IsValid(ViewModel) && IsValid(HUD) && IsValid(Hand);
		}

		void Initialize(const FPresentationStateSnapshot& Snapshot)
		{
			ViewModel->ApplyPresentationSnapshot(Snapshot, true);
			HUD->ConfigureFormalHandForTesting(ViewModel, Hand);
			HUD->RefreshFormalHandForTesting();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG0HandIdentityTest,
	"SlayTheSpireDemo.SelectionPresentation.G0.HandIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSelectionPresentationG0HandIdentityTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG0HUDTest;
	(void)Parameters;

	FProbeFixture Fixture;
	if (!TestTrue(TEXT("G0 HUD fixture is valid."), Fixture.IsValidFixture())) return false;
	Fixture.Initialize(MakeSnapshot(501, 1, { 51, 52, 53 }));

	UBattleCardWidget* Card51 = Fixture.HUD->FindFormalHandCardForTesting(51);
	UBattleCardWidget* Card52 = Fixture.HUD->FindFormalHandCardForTesting(52);
	UBattleCardWidget* Card53 = Fixture.HUD->FindFormalHandCardForTesting(53);
	TestNotNull(TEXT("Initial card 51 exists."), Card51);
	TestNotNull(TEXT("Initial card 52 exists."), Card52);
	TestNotNull(TEXT("Initial card 53 exists."), Card53);
	TestEqual(TEXT("Initial formal child count matches frozen Hand."), Fixture.HUD->GetFormalHandChildCountForTesting(), 3);

	FPresentationStateSnapshot EnergyOnly = MakeSnapshot(501, 2, { 51, 52, 53 });
	EnergyOnly.Energy = 2;
	Fixture.ViewModel->ApplyPresentationSnapshot(EnergyOnly, true);
	TestTrue(TEXT("Energy-only publication preserves card 51 Widget identity."), Fixture.HUD->FindFormalHandCardForTesting(51) == Card51);
	TestTrue(TEXT("Energy-only publication preserves card 52 Widget identity."), Fixture.HUD->FindFormalHandCardForTesting(52) == Card52);
	TestTrue(TEXT("Energy-only publication preserves card 53 Widget identity."), Fixture.HUD->FindFormalHandCardForTesting(53) == Card53);

	Fixture.ViewModel->ApplyPresentationSnapshot(MakeSnapshot(501, 3, { 52, 53, 54 }), true);
	TestNull(TEXT("Removed RuntimeId leaves formal Hand."), Fixture.HUD->FindFormalHandCardForTesting(51));
	TestTrue(TEXT("Surviving card 52 keeps exact Widget object."), Fixture.HUD->FindFormalHandCardForTesting(52) == Card52);
	TestTrue(TEXT("Surviving card 53 keeps exact Widget object."), Fixture.HUD->FindFormalHandCardForTesting(53) == Card53);
	TestNotNull(TEXT("Added RuntimeId gets a Widget."), Fixture.HUD->FindFormalHandCardForTesting(54));
	TestEqual(TEXT("Formal child count still matches frozen Hand."), Fixture.HUD->GetFormalHandChildCountForTesting(), 3);

	UBattleCardWidget* BeforeBattleReplacement = Fixture.HUD->FindFormalHandCardForTesting(52);
	Fixture.ViewModel->ApplyPresentationSnapshot(MakeSnapshot(502, 1, { 52, 60 }), true);
	Fixture.HUD->RefreshFormalHandForTesting();
	UBattleCardWidget* AfterBattleReplacement = Fixture.HUD->FindFormalHandCardForTesting(52);
	TestNotNull(TEXT("Same RuntimeId exists in replacement Battle."), AfterBattleReplacement);
	TestTrue(TEXT("RuntimeId identity does not cross Battle lifecycle."), AfterBattleReplacement != BeforeBattleReplacement);
	TestEqual(TEXT("Replacement Battle formal count is exact."), Fixture.HUD->GetFormalHandChildCountForTesting(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG0FormalSlotOwnershipTest,
	"SlayTheSpireDemo.SelectionPresentation.G0.FormalSlotOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSelectionPresentationG0FormalSlotOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG0HUDTest;
	(void)Parameters;

	FProbeFixture Fixture;
	if (!TestTrue(TEXT("G0 HUD fixture is valid."), Fixture.IsValidFixture())) return false;
	Fixture.Initialize(MakeSnapshot(601, 10, { 61, 62 }));

	UBattleCardWidget* Card61 = Fixture.HUD->FindFormalHandCardForTesting(61);
	if (!TestNotNull(TEXT("Formal card 61 exists."), Card61)) return false;
	TestEqual(TEXT("Formal slot starts visible."), Card61->GetVisibility(), ESlateVisibility::Visible);
	TestTrue(TEXT("Formal slot starts enabled."), Card61->GetIsEnabled());

	const int64 Generation = Fixture.ViewModel->BeginCardPresentationSelectionLifecycle(10);
	TestTrue(TEXT("Ownership lifecycle begins."), Generation > 0);
	TestTrue(TEXT("Pending selection establishes explicit SelectionArea owner."),
		Fixture.ViewModel->SetPendingCardPresentationSelection(Generation, 61, true));
	TestEqual(TEXT("Non-Hand owner preserves structural child count."), Fixture.HUD->GetFormalHandChildCountForTesting(), 2);
	TestTrue(TEXT("Ownership event does not replace exact formal Widget."), Fixture.HUD->FindFormalHandCardForTesting(61) == Card61);
	TestEqual(TEXT("SelectionArea-owned formal slot is Hidden, not Collapsed."), Card61->GetVisibility(), ESlateVisibility::Hidden);
	TestFalse(TEXT("SelectionArea-owned formal slot input is disabled."), Card61->GetIsEnabled());

	TestTrue(TEXT("Deselect removes explicit ownership."),
		Fixture.ViewModel->SetPendingCardPresentationSelection(Generation, 61, false));
	TestTrue(TEXT("Deselect still preserves exact Widget."), Fixture.HUD->FindFormalHandCardForTesting(61) == Card61);
	TestEqual(TEXT("Hand owner restores formal visibility."), Card61->GetVisibility(), ESlateVisibility::Visible);
	TestTrue(TEXT("Hand owner restores formal input."), Card61->GetIsEnabled());

	TestTrue(TEXT("Card can be selected again."),
		Fixture.ViewModel->SetPendingCardPresentationSelection(Generation, 61, true));
	TestTrue(TEXT("Confirmed selection freezes ownership."),
		Fixture.ViewModel->ConfirmCardPresentationSelection(Generation, { 61 }));
	TestEqual(TEXT("Confirmed SelectionArea slot remains Hidden."), Card61->GetVisibility(), ESlateVisibility::Hidden);
	TestFalse(TEXT("Confirmed SelectionArea slot remains input-disabled."), Card61->GetIsEnabled());
	TestTrue(TEXT("Direct completion can be armed only beyond boundary."),
		Fixture.ViewModel->ArmDirectCardPresentationCompletion(Generation, 11));
	Fixture.ViewModel->ApplyPresentationSnapshot(MakeSnapshot(601, 11, { 61, 62 }), true);
	TestEqual(TEXT("Completion fail-safe restores formal visibility without recreating Widget."), Card61->GetVisibility(), ESlateVisibility::Visible);
	TestTrue(TEXT("Completion fail-safe restores formal input."), Card61->GetIsEnabled());
	TestTrue(TEXT("Completion recovery preserves exact formal Widget."), Fixture.HUD->FindFormalHandCardForTesting(61) == Card61);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
