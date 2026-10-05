#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SelectionPresentationG0TestTypes.h"
#include "Components/HorizontalBox.h"
#include "Engine/World.h"
#include "Presentation/PresentationTypes.h"
#include "UI/BattleHUDViewModel.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/GarbageCollection.h"

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
	// UUserWidget defaults to SelfHitTestInvisible: the card's button children
	// remain hit-testable. Both normal formal states satisfy this contract.
	TestTrue(TEXT("Formal slot starts visible with hit-testable children."),
		Card61->GetVisibility() == ESlateVisibility::Visible
		|| Card61->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG0DrawAdoptionTest,
	"SlayTheSpireDemo.SelectionPresentation.G0.DrawAdoption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSelectionPresentationG0DrawAdoptionTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG0HUDTest;
	(void)Parameters;
	FProbeFixture Fixture;
	if (!TestTrue(TEXT("G0 HUD fixture is valid."), Fixture.IsValidFixture())) return false;
	Fixture.Initialize(MakeSnapshot(701, 1, { 71 }));
	UBattleCardWidget* Survivor = Fixture.HUD->FindFormalHandCardForTesting(71);

	// Use the production presentation-card factory and the same Hand attachment
	// as DrawToHand. Completion leaves this visual for the reducer to adopt.
	FPresentationCardSnapshot Draw;
	Draw.RuntimeId = 72;
	Draw.CardId = TEXT("HUD_72");
	Draw.DisplayName = FText::FromString(TEXT("Card72"));
	Draw.Cost = 1;
	Draw.CardType = ECardType::Skill;
	Draw.CardColor = ECardColor::Red;
	FPresentationPlaybackToken Token;
	Token.BattleId = 701; Token.ResolutionId = 1; Token.PresentationSequence = 1; Token.LocalPlaybackGeneration = 1;
	UBattleCardWidget* Drawn = Fixture.HUD->PrepareDrawForTesting(Draw, Token, 1);
	if (!TestNotNull(TEXT("Draw visual is created."), Drawn)) return false;
	TestEqual(TEXT("In-flight draw is not hit-testable."), Drawn->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestFalse(TEXT("In-flight draw has no Gameplay request binding."), Drawn->OnBattleCardRequested.IsBound());
	FPresentationPlaybackToken Stale = Token;
	++Stale.LocalPlaybackGeneration;
	TestFalse(TEXT("Stale completion cannot authorize adoption."), Fixture.HUD->CompleteDrawForTesting(Stale));
	TestTrue(TEXT("Exact completion authorizes the pending attachment."), Fixture.HUD->CompleteDrawForTesting(Token));

	Fixture.ViewModel->ApplyPresentationSnapshot(MakeSnapshot(701, 2, { 71, 72 }), true);
	TestTrue(TEXT("Reducer adopts the exact draw visual."), Fixture.HUD->FindFormalHandCardForTesting(72) == Drawn);
	TestTrue(TEXT("Existing formal card identity survives draw."), Fixture.HUD->FindFormalHandCardForTesting(71) == Survivor);
	TestEqual(TEXT("Adopted draw receives mouse hit tests."), Drawn->GetVisibility(), ESlateVisibility::Visible);
	TestTrue(TEXT("Adopted draw is enabled."), Drawn->GetIsEnabled());
	TestTrue(TEXT("Adopted draw has a formal request binding."), Drawn->OnBattleCardRequested.IsBound());
	TestEqual(TEXT("Adoption leaves no duplicate slot."), Fixture.Hand->GetChildrenCount(), 2);
	TestFalse(TEXT("Old cancel cannot remove the adopted formal card."), Fixture.HUD->CancelDrawForTesting(Token));

	const int64 Generation = Fixture.ViewModel->BeginCardPresentationSelectionLifecycle(2);
	TestTrue(TEXT("Adopted card can enter Selection ownership."), Fixture.ViewModel->SetPendingCardPresentationSelection(Generation, 72, true));
	Fixture.HUD->RefreshFormalHandForTesting();
	TestEqual(TEXT("Reconcile preserves explicitly owned Hidden slot."), Drawn->GetVisibility(), ESlateVisibility::Hidden);
	TestFalse(TEXT("Explicitly owned adopted card stays input-disabled."), Drawn->GetIsEnabled());
	Fixture.ViewModel->CancelCardPresentationSelectionLifecycle(Generation);
	TestEqual(TEXT("Ownership release restores adopted card hit testing."), Drawn->GetVisibility(), ESlateVisibility::Visible);
	TestTrue(TEXT("Ownership release restores adopted card input."), Drawn->GetIsEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeHandStructureTransactionTest,
	"SlayTheSpireDemo.HandStructure.Transaction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeHandStructureTransactionTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG0HUDTest;
	FProbeFixture Fixture;
	if (!TestTrue(TEXT("Fixture exists"), Fixture.IsValidFixture())) return false;
	Fixture.Initialize(MakeSnapshot(801, 1, {81, 82, 83}));
	UBattleCardWidget* Card81 = Fixture.HUD->FindFormalHandCardForTesting(81);
	UPanelSlot* Slot81 = Card81->Slot;
	bool bPublishNested = true;
	Fixture.HUD->OnAfterRefresh = [&]()
	{
		if (!bPublishNested) return;
		bPublishNested = false;
		Fixture.ViewModel->ApplyPresentationSnapshot(MakeSnapshot(801, 3, {83, 81, 84}), true);
	};
	Fixture.ViewModel->ApplyPresentationSnapshot(MakeSnapshot(801, 2, {82, 81, 83}), true);
	TestEqual(TEXT("Nested notifications drain after the outer refresh"), Fixture.HUD->MaxRefreshDepth, 1);
	TestTrue(TEXT("Final frozen order is committed before publication returns"), Fixture.Hand->GetChildAt(1) == Card81);
	TestTrue(TEXT("Reordering preserves the survivor slot"), Card81->Slot == Slot81);
	TestEqual(TEXT("Final member count"), Fixture.Hand->GetChildrenCount(), 3);
	FPresentationStateSnapshot Invalid = MakeSnapshot(801, 4, {81, 81, 85});
	Fixture.ViewModel->ApplyPresentationSnapshot(Invalid, true);
	TestTrue(TEXT("Invalid preparation preserves the last complete order"), Fixture.Hand->GetChildAt(1) == Card81);
	TestNull(TEXT("Preparation failure never attaches a partial new member"), Fixture.HUD->FindFormalHandCardForTesting(85));
	TestEqual(TEXT("Invalid frozen Hand disables Presentation without a Gameplay fault"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::PresentationUnavailable);
	Fixture.ViewModel->ApplyPresentationSnapshot(MakeSnapshot(801, 5, {83, 81, 86}), true);
	TestTrue(TEXT("Explicit recovery can commit a valid frozen Hand"), Fixture.HUD->FindFormalHandCardForTesting(86) != nullptr);
	Fixture.HUD->CardWidgetClass = nullptr;
	Fixture.ViewModel->ApplyPresentationSnapshot(MakeSnapshot(801, 6, {81, 87}), true);
	TestEqual(TEXT("Missing Widget class preserves the complete member set"), Fixture.Hand->GetChildrenCount(), 3);
	TestEqual(TEXT("Missing class also exposes unavailable state"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::PresentationUnavailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeIncomingHandLifetimeTest,
	"SlayTheSpireDemo.HandStructure.IncomingLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNativeIncomingHandLifetimeTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG0HUDTest;
	FProbeFixture Fixture;
	if (!TestTrue(TEXT("Fixture exists"), Fixture.IsValidFixture())) return false;
	Fixture.Initialize(MakeSnapshot(901, 1, {91}));
	TStrongObjectPtr<USelectionPresentationG0HUDProbe> KeepHUD(Fixture.HUD);
	TStrongObjectPtr<UBattleHUDViewModel> KeepViewModel(Fixture.ViewModel);
	FPresentationCardSnapshot Draw;
	Draw.RuntimeId = 92; Draw.CardId = TEXT("HUD_92"); Draw.DisplayName = FText::FromString(TEXT("Card92"));
	Draw.Cost = 1; Draw.CardType = ECardType::Skill; Draw.CardColor = ECardColor::Red;
	FPresentationPlaybackToken Token;
	Token.BattleId = 901; Token.ResolutionId = 1; Token.PresentationSequence = 1; Token.LocalPlaybackGeneration = 1;
	TestNull(TEXT("Wrong target index declines with no attachment"), Fixture.HUD->PrepareDrawForTesting(Draw, Token, 0));
	TestEqual(TEXT("Decline preserves formal membership"), Fixture.Hand->GetChildrenCount(), 1);
	UBattleCardWidget* Drawn = Fixture.HUD->PrepareDrawForTesting(Draw, Token, 1);
	if (!TestNotNull(TEXT("One temporary draw attaches"), Drawn)) return false;
	TWeakObjectPtr<UBattleCardWidget> WeakDraw(Drawn);
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("HUD roots incoming Widget through GC"), WeakDraw.IsValid());
	Fixture.HUD->RefreshFormalHandForTesting();
	TestTrue(TEXT("Unrelated refresh preserves in-flight attachment"), Fixture.Hand->GetChildAt(1) == Drawn);
	TestNull(TEXT("A second temporary draw is rejected"), Fixture.HUD->PrepareDrawForTesting(Draw, Token, 1));
	TestTrue(TEXT("Exact cancel retires only temporary draw"), Fixture.HUD->CancelDrawForTesting(Token));
	TestEqual(TEXT("Cancel leaves one formal slot"), Fixture.Hand->GetChildrenCount(), 1);
	FPresentationPlaybackToken NewToken = Token; ++NewToken.LocalPlaybackGeneration;
	UBattleCardWidget* NewDraw = Fixture.HUD->PrepareDrawForTesting(Draw, NewToken, 1);
	TestNotNull(TEXT("New generation can attach the same runtime identity"), NewDraw);
	TestFalse(TEXT("Old cancel cannot retire new generation"), Fixture.HUD->CancelDrawForTesting(Token));
	TestFalse(TEXT("Old completion cannot authorize new generation"), Fixture.HUD->CompleteDrawForTesting(Token));
	Fixture.ViewModel->ApplyPresentationSnapshot(MakeSnapshot(902, 1, {91, 92}), true);
	TestTrue(TEXT("Battle replacement creates a new formal owner"), Fixture.HUD->FindFormalHandCardForTesting(92) != NewDraw);
	TestFalse(TEXT("Retired token cannot cancel replacement owner"), Fixture.HUD->CancelDrawForTesting(NewToken));
	TestEqual(TEXT("Replacement has no ghost temporary slot"), Fixture.Hand->GetChildrenCount(), 2);
	Draw.RuntimeId = 93; Draw.CardId = TEXT("HUD_93"); Draw.DisplayName = FText::FromString(TEXT("Card93"));
	NewToken.BattleId = 902;
	TestNotNull(TEXT("Replacement surface can prepare its own draw"), Fixture.HUD->PrepareDrawForTesting(Draw, NewToken, 2));
	Fixture.HUD->SetViewModel(nullptr);
	TestEqual(TEXT("ViewModel loss cancels the sole temporary attachment"), Fixture.Hand->GetChildrenCount(), 2);
	TestFalse(TEXT("Detached surface rejects the old completion"), Fixture.HUD->CompleteDrawForTesting(NewToken));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
