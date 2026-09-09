#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CardSelectionPresentationTestTypes.h"
#include "Phase6UIA2NR8TestTypes.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

namespace CardSelectionPresentationTest
{
	constexpr int64 TestBattleId = 9201;
	constexpr int64 TestResolutionId = 9202;

	FPresentationCardSnapshot MakeSnapshot(int32 RuntimeId, const TCHAR* CardId)
	{
		FPresentationCardSnapshot Snapshot;
		Snapshot.RuntimeId = RuntimeId;
		Snapshot.CardId = FName(CardId);
		Snapshot.DisplayName = FText::FromString(CardId);
		Snapshot.Cost = 1;
		Snapshot.CardType = ECardType::Skill;
		Snapshot.Rarity = ECardRarity::Common;
		Snapshot.CardColor = ECardColor::Red;
		Snapshot.TargetType = ECardTargetType::None;
		Snapshot.Description = FText::FromString(TEXT("Selection presentation probe."));
		return Snapshot;
	}

	FBattleHUDCardView MakeFormalView(const FPresentationCardSnapshot& Snapshot)
	{
		FBattleHUDCardView View;
		View.RuntimeId = Snapshot.RuntimeId;
		View.CardId = Snapshot.CardId;
		View.DisplayName = Snapshot.DisplayName;
		View.bUpgraded = Snapshot.bUpgraded;
		View.Cost = Snapshot.Cost;
		View.CardType = Snapshot.CardType;
		View.Rarity = Snapshot.Rarity;
		View.CardColor = Snapshot.CardColor;
		View.TargetType = Snapshot.TargetType;
		View.Description = Snapshot.Description;
		View.CardArt = Snapshot.CardArt;
		return View;
	}

	FPresentationRecord MakeHandOutgoingRecord(
		const FPresentationCardSnapshot& Snapshot,
		int64 Sequence,
		ECardZone Destination,
		int32 DestinationIndex)
	{
		FPresentationRecord Record;
		Record.BattleId = TestBattleId;
		Record.ResolutionId = TestResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::CardZoneChanged;
		Record.CardZoneChanged.Card = Snapshot;
		Record.CardZoneChanged.FromZone = ECardZone::Hand;
		Record.CardZoneChanged.ToZone = Destination;
		Record.CardZoneChanged.FromIndex = 0;
		Record.CardZoneChanged.ToIndex = DestinationIndex;
		return Record;
	}

	FPresentationPlaybackToken MakeToken(int64 Sequence, int64 Generation)
	{
		FPresentationPlaybackToken Token;
		Token.BattleId = TestBattleId;
		Token.ResolutionId = TestResolutionId;
		Token.PresentationSequence = Sequence;
		Token.LocalPlaybackGeneration = Generation;
		return Token;
	}

	struct FFixture
	{
		UWorld* World = nullptr;
		UCardSelectionPresentationHUDProbe* HUD = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		UHorizontalBox* Hand = nullptr;
		UOverlay* PlayArea = nullptr;
		UOverlay* SelectionArea = nullptr;
		UTextBlock* DrawCount = nullptr;
		UTextBlock* DiscardCount = nullptr;
		UTextBlock* ExhaustCount = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;
			HUD = NewObject<UCardSelectionPresentationHUDProbe>(World);
			ViewModel = NewObject<UBattleHUDViewModel>(HUD);
			Hand = NewObject<UHorizontalBox>(HUD);
			PlayArea = NewObject<UOverlay>(HUD);
			SelectionArea = NewObject<UOverlay>(HUD);
			DrawCount = NewObject<UTextBlock>(HUD);
			DiscardCount = NewObject<UTextBlock>(HUD);
			ExhaustCount = NewObject<UTextBlock>(HUD);
			if (!IsValid(HUD) || !IsValid(ViewModel) || !IsValid(Hand)
				|| !IsValid(PlayArea) || !IsValid(SelectionArea)
				|| !IsValid(DrawCount) || !IsValid(DiscardCount)
				|| !IsValid(ExhaustCount)) return;
			HUD->SetTestWorld(World);
			HUD->ConfigureSelectionSurfaces(
				ViewModel,
				Hand,
				PlayArea,
				DrawCount,
				DiscardCount,
				ExhaustCount);
			HUD->SetSelectionAreaHostForTesting(SelectionArea);
			ViewModel->BattleId = TestBattleId;
			ViewModel->StateRevision = 100;
			ViewModel->DrawCount = 4;
			ViewModel->DiscardCount = 3;
			ViewModel->ExhaustCount = 2;
			SyncCounts();
		}

		~FFixture()
		{
			if (IsValid(HUD)) HUD->SkipPresentation();
			FTSTicker::GetCoreTicker().Tick(0.0f);
			if (IsValid(World)) World->DestroyWorld(false);
		}

		void SyncCounts()
		{
			if (!IsValid(ViewModel)) return;
			if (IsValid(DrawCount)) DrawCount->SetText(FText::AsNumber(ViewModel->DrawCount));
			if (IsValid(DiscardCount)) DiscardCount->SetText(FText::AsNumber(ViewModel->DiscardCount));
			if (IsValid(ExhaustCount)) ExhaustCount->SetText(FText::AsNumber(ViewModel->ExhaustCount));
		}

		UBattleCardWidget* AddFormalCard(
			const FPresentationCardSnapshot& Snapshot,
			ESlateVisibility Visibility = ESlateVisibility::Visible)
		{
			if (!IsValid(HUD) || !IsValid(Hand) || !IsValid(ViewModel)) return nullptr;
			UPhase6UIA2NR8CardProbe* Card = NewObject<UPhase6UIA2NR8CardProbe>(HUD);
			if (!IsValid(Card)) return nullptr;
			const FBattleHUDCardView View = MakeFormalView(Snapshot);
			Card->SetCardView(View);
			Card->SetVisibility(Visibility);
			ViewModel->HandCards.Reset();
			ViewModel->HandCards.Add(View);
			Hand->ClearChildren();
			return Hand->AddChild(Card) != nullptr ? Card : nullptr;
		}

		UBattleCardWidget* AddSelectionAreaCard(const FPresentationCardSnapshot& Snapshot)
		{
			if (!IsValid(HUD) || !IsValid(SelectionArea)) return nullptr;
			UPhase6UIA2NR8CardProbe* Card = NewObject<UPhase6UIA2NR8CardProbe>(HUD);
			if (!IsValid(Card)) return nullptr;
			Card->SetCardView(MakeFormalView(Snapshot));
			Card->SetVisibility(ESlateVisibility::HitTestInvisible);
			return SelectionArea->AddChild(Card) != nullptr ? Card : nullptr;
		}
	};
}

using namespace CardSelectionPresentationTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSharedSelectionHandToDrawPresentationTest,
	"SlayTheSpireDemo.CardSelection.Presentation.HandToDraw.GenericTransferFinishAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSharedSelectionHandToDrawPresentationTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!TestNotNull(TEXT("World exists"), Fixture.World)
		|| !TestNotNull(TEXT("HUD exists"), Fixture.HUD)
		|| !TestNotNull(TEXT("ViewModel exists"), Fixture.ViewModel))
	{
		return false;
	}

	// Retain a real played-card transient across the decision, as production does.
	const FPresentationCardSnapshot PlayedSnapshot = MakeSnapshot(709, TEXT("GenericPlayedCard"));
	Fixture.ViewModel->Player.PresentationId = TEXT("Player");
	Fixture.ViewModel->Energy = 3;
	Fixture.ViewModel->MaxEnergy = 3;
	Fixture.AddFormalCard(PlayedSnapshot);
	FPresentationRecord PlayedRecord;
	PlayedRecord.BattleId = TestBattleId;
	PlayedRecord.ResolutionId = TestResolutionId;
	PlayedRecord.PresentationSequence = 1;
	PlayedRecord.Type = EBattlePresentationRecordType::CardPlayed;
	PlayedRecord.CardPlayed.Card = PlayedSnapshot;
	PlayedRecord.CardPlayed.SourcePresentationId = TEXT("Player");
	PlayedRecord.CardPlayed.HandIndexBefore = 0;
	PlayedRecord.CardPlayed.PlayAreaIndexAfter = 0;
	PlayedRecord.CardPlayed.EnergyBefore = 3;
	PlayedRecord.CardPlayed.EnergyAfter = 2;
	PlayedRecord.CardPlayed.CostPaid = 1;
	if (!TestTrue(TEXT("Played-card visual starts"),
		Fixture.HUD->PlayPresentationRecord(PlayedRecord, MakeToken(1, 1)))) return false;
	Fixture.HUD->FinishNativeForTesting(MakeToken(1, 1));
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Played-card visual is retained before selection transfer"),
		Fixture.PlayArea->GetChildrenCount(), 1);

	const FPresentationCardSnapshot Snapshot = MakeSnapshot(710, TEXT("GenericSelectedCard"));
	UBattleCardWidget* Historical = Fixture.AddFormalCard(Snapshot);
	if (!TestNotNull(TEXT("Formal Hand card is prepared"), Historical)) return false;
	const ESlateVisibility HistoricalVisibilityBefore = Historical->GetVisibility();
	const FPresentationRecord Record = MakeHandOutgoingRecord(
		Snapshot, 2, ECardZone::DrawPile, Fixture.ViewModel->DrawCount);
	const FPresentationPlaybackToken Token = MakeToken(2, 2);

	FPresentationRecord BadIdentity = Record;
	BadIdentity.CardZoneChanged.Card.CardId = TEXT("WrongIdentity");
	TestFalse(TEXT("Mismatched immutable card identity is rejected"),
		Fixture.HUD->PlayPresentationRecord(BadIdentity, Token));
	TestEqual(TEXT("Rejected identity leaves the formal card visible"),
		Historical->GetVisibility(), HistoricalVisibilityBefore);

	TestTrue(TEXT("Generic Hand->DrawPile Record starts transition engine"),
		Fixture.HUD->PlayPresentationRecord(Record, Token));
	TestEqual(TEXT("SingleRecord creates exactly one active transition child"),
		Fixture.HUD->GetActiveNativeCardTransitionCountForTesting(), 1);
	TestEqual(TEXT("Historical Hand card is hidden while transition owns its visual"),
		Historical->GetVisibility(), ESlateVisibility::Hidden);
	TestEqual(TEXT("Transition coexists with retained played-card visual"),
		Fixture.PlayArea->GetChildrenCount(), 2);
	UBattleCardWidget* Moving = Fixture.HUD->GetActiveNativeCardTransitionVisualForTesting();
	if (!TestNotNull(TEXT("Moving visual exists"), Moving)) return false;
	const FVector2D StartTranslation = Moving->GetRenderTransform().Translation;
	Fixture.HUD->InvokeNativeTickForTesting(0.25f);
	const FVector2D MidTranslation = Moving->GetRenderTransform().Translation;
	TestTrue(TEXT("Selected card visibly moves toward DrawPile"),
		!MidTranslation.Equals(StartTranslation));
	TestEqual(TEXT("Presentation animation does not mutate frozen DrawCount"),
		Fixture.ViewModel->DrawCount, 4);

	Fixture.HUD->SkipPresentation();
	TestEqual(TEXT("Cancel restores exact historical Hand card visibility"),
		Historical->GetVisibility(), HistoricalVisibilityBefore);
	TestEqual(TEXT("Skip removes moving and retained played-card visuals"),
		Fixture.PlayArea->GetChildrenCount(), 0);

	Historical = Fixture.AddFormalCard(Snapshot);
	if (!TestNotNull(TEXT("Formal Hand card can be prepared again"), Historical)) return false;
	const FPresentationPlaybackToken FinishToken = MakeToken(2, 3);
	const FPresentationPlaybackToken StaleFinishToken = MakeToken(2, 99);
	TestTrue(TEXT("Second generic transition starts"),
		Fixture.HUD->PlayPresentationRecord(Record, FinishToken));
	Fixture.HUD->FinishNativeCardTransitionForTesting(StaleFinishToken);
	TestEqual(TEXT("Stale Finish keeps historical Hand card hidden"),
		Historical->GetVisibility(), ESlateVisibility::Hidden);
	TestEqual(TEXT("Stale Finish keeps one transient moving card"),
		Fixture.PlayArea->GetChildrenCount(), 1);

	Fixture.HUD->FinishNativeCardTransitionForTesting(FinishToken);
	TestEqual(TEXT("Exact Finish collapses historical Hand visual until reducer refresh"),
		Historical->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Exact Finish removes transient moving card"),
		Fixture.PlayArea->GetChildrenCount(), 0);
	TestEqual(TEXT("Finish still does not mutate frozen DrawCount directly"),
		Fixture.ViewModel->DrawCount, 4);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSharedSelectionHandToExhaustPresentationTest,
	"SlayTheSpireDemo.CardSelection.Presentation.HandToExhaust.GenericFadeAtSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSharedSelectionHandToExhaustPresentationTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	const FPresentationCardSnapshot Snapshot = MakeSnapshot(711, TEXT("GenericExhaustedCard"));
	UBattleCardWidget* Historical = Fixture.AddFormalCard(Snapshot);
	if (!TestNotNull(TEXT("Historical Hand card exists"), Historical)) return false;
	const FVector2D ConfirmedTranslation(240.0f, -180.0f);
	Historical->SetRenderTranslation(ConfirmedTranslation);
	const FPresentationRecord Record = MakeHandOutgoingRecord(
		Snapshot, 3, ECardZone::ExhaustPile, Fixture.ViewModel->ExhaustCount);
	const FPresentationPlaybackToken Token = MakeToken(3, 1);

	TestTrue(TEXT("Hand->Exhaust starts generic transition"),
		Fixture.HUD->PlayPresentationRecord(Record, Token));
	TestEqual(TEXT("Formal structural Hand card is hidden, not reparented"),
		Historical->GetVisibility(), ESlateVisibility::Hidden);
	TestTrue(TEXT("Formal structural Hand card remains in HB_Hand"),
		Historical->GetParent() == Fixture.Hand);
	UBattleCardWidget* Moving = Fixture.HUD->GetActiveNativeCardTransitionVisualForTesting();
	if (!TestNotNull(TEXT("Exhaust moving visual exists"), Moving)) return false;
	TestTrue(TEXT("Exhaust child starts at confirmed compatibility translation"),
		Moving->GetRenderTransform().Translation.Equals(ConfirmedTranslation));

	Fixture.HUD->InvokeNativeTickForTesting(0.25f);
	TestTrue(TEXT("Generic Exhaust fade keeps child at its source position"),
		Moving->GetRenderTransform().Translation.Equals(ConfirmedTranslation));
	TestTrue(TEXT("Generic Exhaust changes opacity without adding movement"),
		Moving->GetRenderOpacity() > 0.0f && Moving->GetRenderOpacity() < 1.0f);
	TestEqual(TEXT("ExhaustCount remains historical during animation"),
		Fixture.ViewModel->ExhaustCount, 2);

	Fixture.HUD->FinishNativeCardTransitionForTesting(Token);
	TestEqual(TEXT("Finished Exhaust formal card is collapsed until reducer refresh"),
		Historical->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Finished Exhaust removes transition child"),
		Fixture.PlayArea->GetChildrenCount(), 0);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSharedSelectionHandToDiscardPresentationTest,
	"SlayTheSpireDemo.CardSelection.Presentation.HandToDiscard.GenericMovementAndDestinationValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSharedSelectionHandToDiscardPresentationTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	const FPresentationCardSnapshot Snapshot = MakeSnapshot(712, TEXT("GenericDiscardProbe"));
	UBattleCardWidget* Historical = Fixture.AddFormalCard(Snapshot);
	if (!TestNotNull(TEXT("Historical Hand card exists"), Historical)) return false;
	FPresentationRecord Record = MakeHandOutgoingRecord(
		Snapshot, 4, ECardZone::DiscardPile, Fixture.ViewModel->DiscardCount);
	const FPresentationPlaybackToken Token = MakeToken(4, 1);

	FPresentationRecord BadDestination = Record;
	BadDestination.CardZoneChanged.ToIndex = Fixture.ViewModel->DiscardCount + 1;
	TestFalse(TEXT("Wrong committed Discard destination index is rejected"),
		Fixture.HUD->PlayPresentationRecord(BadDestination, Token));
	TestEqual(TEXT("Rejected destination creates zero transition children"),
		Fixture.HUD->GetActiveNativeCardTransitionCountForTesting(), 0);
	TestEqual(TEXT("Rejected destination leaves formal card visible"),
		Historical->GetVisibility(), ESlateVisibility::Visible);

	TestTrue(TEXT("Exact Hand->Discard starts generic transition"),
		Fixture.HUD->PlayPresentationRecord(Record, Token));
	UBattleCardWidget* Moving = Fixture.HUD->GetActiveNativeCardTransitionVisualForTesting();
	if (!TestNotNull(TEXT("Discard moving visual exists"), Moving)) return false;
	const FVector2D StartTranslation = Moving->GetRenderTransform().Translation;
	Fixture.HUD->InvokeNativeTickForTesting(0.25f);
	TestTrue(TEXT("Discard child moves toward committed discard surface"),
		!Moving->GetRenderTransform().Translation.Equals(StartTranslation));
	TestTrue(TEXT("Discard child fades while moving"),
		Moving->GetRenderOpacity() > 0.15f && Moving->GetRenderOpacity() < 1.0f);
	TestEqual(TEXT("DiscardCount remains historical during animation"),
		Fixture.ViewModel->DiscardCount, 3);

	Fixture.HUD->FinishNativeCardTransitionForTesting(Token);
	TestEqual(TEXT("Discard Finish collapses formal card until reducer refresh"),
		Historical->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Discard Finish removes transition child"),
		Fixture.PlayArea->GetChildrenCount(), 0);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSharedSelectionAreaSourcePresentationTest,
	"SlayTheSpireDemo.CardSelection.Presentation.G4.SelectionAreaExactVisualTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSharedSelectionAreaSourcePresentationTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	const FPresentationCardSnapshot Snapshot = MakeSnapshot(713, TEXT("SelectionAreaSourceProbe"));
	UBattleCardWidget* Formal = Fixture.AddFormalCard(Snapshot, ESlateVisibility::Hidden);
	UBattleCardWidget* SelectionVisual = Fixture.AddSelectionAreaCard(Snapshot);
	if (!TestNotNull(TEXT("Formal structural Hand card exists"), Formal)
		|| !TestNotNull(TEXT("SelectionArea visual exists"), SelectionVisual))
	{
		return false;
	}

	const int64 SelectionGeneration =
		Fixture.ViewModel->BeginCardPresentationSelectionLifecycle(Fixture.ViewModel->StateRevision);
	TestTrue(TEXT("Selection ownership lifecycle begins"), SelectionGeneration > 0);
	TestTrue(TEXT("Exact RuntimeId enters SelectionArea Pending"),
		Fixture.ViewModel->SetPendingCardPresentationSelection(
			SelectionGeneration, Snapshot.RuntimeId, true));
	TArray<int32> ConfirmedIds;
	ConfirmedIds.Add(Snapshot.RuntimeId);
	TestTrue(TEXT("Exact SelectionArea set confirms"),
		Fixture.ViewModel->ConfirmCardPresentationSelection(
			SelectionGeneration, ConfirmedIds));
	TestEqual(TEXT("Confirmed visual owner is SelectionArea"),
		Fixture.ViewModel->GetCardPresentationOwner(Snapshot.RuntimeId),
		ECardPresentationOwner::SelectionArea);

	const FPresentationRecord Record = MakeHandOutgoingRecord(
		Snapshot, 5, ECardZone::DrawPile, Fixture.ViewModel->DrawCount);
	const FPresentationPlaybackToken Token = MakeToken(5, 1);
	TestTrue(TEXT("SelectionArea source starts through generic SingleRecord engine"),
		Fixture.HUD->PlayPresentationRecord(Record, Token));
	TestEqual(TEXT("SingleRecord still owns exactly one child"),
		Fixture.HUD->GetActiveNativeCardTransitionCountForTesting(), 1);
	TestTrue(TEXT("Generic transition moves the exact SelectionArea object, not a clone"),
		Fixture.HUD->GetActiveNativeCardTransitionVisualForTesting() == SelectionVisual);
	TestTrue(TEXT("Exact SelectionArea object is now parented by transition surface"),
		SelectionVisual->GetParent() == Fixture.PlayArea);
	TestEqual(TEXT("SelectionArea Host no longer owns a duplicate child"),
		Fixture.SelectionArea->GetChildrenCount(), 0);
	TestEqual(TEXT("Accepted visual ownership advances SelectionArea -> Transition"),
		Fixture.ViewModel->GetCardPresentationOwner(Snapshot.RuntimeId),
		ECardPresentationOwner::Transition);
	TestTrue(TEXT("Formal Hand child remains structural while non-Hand owner is active"),
		Formal->GetParent() == Fixture.Hand);
	TestEqual(TEXT("Formal Hand child stays hidden"),
		Formal->GetVisibility(), ESlateVisibility::Hidden);

	const FPresentationPlaybackToken StaleToken = MakeToken(5, 88);
	Fixture.HUD->FinishNativeCardTransitionForTesting(StaleToken);
	TestEqual(TEXT("Stale cross-generation finish cannot retire exact child"),
		Fixture.HUD->GetActiveNativeCardTransitionCountForTesting(), 1);
	Fixture.HUD->FinishNativeCardTransitionForTesting(Token);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Exact finish retires transition child"),
		Fixture.HUD->GetActiveNativeCardTransitionCountForTesting(), 0);
	TestEqual(TEXT("Transition visual is removed after exact finish"),
		Fixture.PlayArea->GetChildrenCount(), 0);

	TestTrue(TEXT("Reducer-pending ownership can advance Transition -> Consumed"),
		Fixture.ViewModel->TryTransferCardPresentationOwnership(
			SelectionGeneration,
			Snapshot.RuntimeId,
			ECardPresentationOwner::Transition,
			ECardPresentationOwner::ConsumedPendingReducer));
	TestFalse(TEXT("ConsumedPendingReducer child is never replayed"),
		Fixture.HUD->PlayPresentationRecord(Record, MakeToken(5, 2)));
	TestEqual(TEXT("No-replay decline creates no visual"),
		Fixture.PlayArea->GetChildrenCount(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
