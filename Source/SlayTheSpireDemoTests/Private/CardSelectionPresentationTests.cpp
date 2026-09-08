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

	FPresentationRecord MakeHandToDrawRecord(
		const FPresentationCardSnapshot& Snapshot,
		int64 Sequence,
		int32 DrawCountBefore)
	{
		FPresentationRecord Record;
		Record.BattleId = 9201;
		Record.ResolutionId = 9202;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::CardZoneChanged;
		Record.CardZoneChanged.Card = Snapshot;
		Record.CardZoneChanged.FromZone = ECardZone::Hand;
		Record.CardZoneChanged.ToZone = ECardZone::DrawPile;
		Record.CardZoneChanged.FromIndex = 0;
		Record.CardZoneChanged.ToIndex = DrawCountBefore;
		return Record;
	}

	FPresentationPlaybackToken MakeToken(int64 Sequence, int64 Generation)
	{
		FPresentationPlaybackToken Token;
		Token.BattleId = 9201;
		Token.ResolutionId = 9202;
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
		UTextBlock* DrawCount = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;
			HUD = NewObject<UCardSelectionPresentationHUDProbe>(World);
			ViewModel = NewObject<UBattleHUDViewModel>(HUD);
			Hand = NewObject<UHorizontalBox>(HUD);
			PlayArea = NewObject<UOverlay>(HUD);
			DrawCount = NewObject<UTextBlock>(HUD);
			if (!IsValid(HUD) || !IsValid(ViewModel) || !IsValid(Hand)
				|| !IsValid(PlayArea) || !IsValid(DrawCount)) return;
			HUD->SetTestWorld(World);
			HUD->ConfigureSelectionSurfaces(ViewModel, Hand, PlayArea, DrawCount);
			ViewModel->DrawCount = 4;
			DrawCount->SetText(FText::AsNumber(ViewModel->DrawCount));
		}

		~FFixture()
		{
			if (IsValid(HUD)) HUD->SkipPresentation();
			FTSTicker::GetCoreTicker().Tick(0.0f);
			if (IsValid(World)) World->DestroyWorld(false);
		}

		bool AddFormalCard(const FPresentationCardSnapshot& Snapshot)
		{
			if (!IsValid(HUD) || !IsValid(Hand) || !IsValid(ViewModel)) return false;
			UPhase6UIA2NR8CardProbe* Card = NewObject<UPhase6UIA2NR8CardProbe>(HUD);
			if (!IsValid(Card)) return false;
			const FBattleHUDCardView View = MakeFormalView(Snapshot);
			Card->SetCardView(View);
			ViewModel->HandCards.Reset();
			ViewModel->HandCards.Add(View);
			Hand->ClearChildren();
			return Hand->AddChild(Card) != nullptr;
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
	Fixture.AddFormalCard(PlayedSnapshot);
	FPresentationRecord PlayedRecord;
	PlayedRecord.BattleId = 9201;
	PlayedRecord.ResolutionId = 9202;
	PlayedRecord.PresentationSequence = 1;
	PlayedRecord.Type = EBattlePresentationRecordType::CardPlayed;
	PlayedRecord.CardPlayed.Card = PlayedSnapshot;
	PlayedRecord.CardPlayed.SourcePresentationId = TEXT("Player");
	PlayedRecord.CardPlayed.HandIndexBefore = 0;
	PlayedRecord.CardPlayed.PlayAreaIndexAfter = 0;
	PlayedRecord.CardPlayed.EnergyBefore = 3;
	PlayedRecord.CardPlayed.EnergyAfter = 2;
	PlayedRecord.CardPlayed.CostPaid = 1;
	if (!TestTrue(TEXT("Played-card visual starts"), Fixture.HUD->PlayPresentationRecord(PlayedRecord, MakeToken(1, 1)))) return false;
	Fixture.HUD->FinishNativeForTesting(MakeToken(1, 1));
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Played-card visual is retained before selection transfer"), Fixture.PlayArea->GetChildrenCount(), 1);
	const FPresentationCardSnapshot Snapshot = MakeSnapshot(710, TEXT("GenericSelectedCard"));
	if (!TestTrue(TEXT("Formal Hand card is prepared"), Fixture.AddFormalCard(Snapshot))) return false;
	UWidget* Historical = Fixture.Hand->GetChildAt(0);
	const ESlateVisibility HistoricalVisibilityBefore = Historical->GetVisibility();
	const FPresentationRecord Record = MakeHandToDrawRecord(Snapshot, 2, Fixture.ViewModel->DrawCount);
	const FPresentationPlaybackToken Token = MakeToken(2, 2);

	TestTrue(TEXT("Generic Hand->DrawPile Record starts Native transfer"), Fixture.HUD->PlayPresentationRecord(Record, Token));
	TestEqual(TEXT("Historical selected card is hidden while transfer owns its visual"), Historical->GetVisibility(), ESlateVisibility::Hidden);
	TestEqual(TEXT("Transfer coexists with the retained played-card visual"), Fixture.PlayArea->GetChildrenCount(), 2);
	UBattleCardWidget* Moving = Cast<UBattleCardWidget>(Fixture.PlayArea->GetChildAt(1));
	if (!TestNotNull(TEXT("Moving visual is a card"), Moving)) return false;
	const FVector2D StartTranslation = Moving->GetRenderTransform().Translation;
	Fixture.HUD->InvokeNativeTickForTesting(0.25f);
	const FVector2D MidTranslation = Moving->GetRenderTransform().Translation;
	TestTrue(TEXT("Selected card visibly moves toward DrawPile"), !MidTranslation.Equals(StartTranslation));
	TestEqual(TEXT("Presentation animation does not mutate frozen DrawCount"), Fixture.ViewModel->DrawCount, 4);

	Fixture.HUD->SkipPresentation();
	TestEqual(TEXT("Cancel restores exact historical Hand card visibility"), Historical->GetVisibility(), HistoricalVisibilityBefore);
	TestEqual(TEXT("Skip removes both moving and retained played-card visuals"), Fixture.PlayArea->GetChildrenCount(), 0);

	if (!TestTrue(TEXT("Formal Hand card can be prepared again"), Fixture.AddFormalCard(Snapshot))) return false;
	Historical = Fixture.Hand->GetChildAt(0);
	const FPresentationPlaybackToken FinishToken = MakeToken(2, 3);
	const FPresentationPlaybackToken StaleFinishToken = MakeToken(2, 99);
	TestTrue(TEXT("Second generic transfer starts"), Fixture.HUD->PlayPresentationRecord(Record, FinishToken));
	Fixture.HUD->FinishSharedHandToDrawPilePresentationForTesting(StaleFinishToken);
	TestEqual(TEXT("Stale Finish keeps historical Hand card hidden"), Historical->GetVisibility(), ESlateVisibility::Hidden);
	TestEqual(TEXT("Stale Finish keeps transient moving card"), Fixture.PlayArea->GetChildrenCount(), 1);

	Fixture.HUD->FinishSharedHandToDrawPilePresentationForTesting(FinishToken);
	TestEqual(TEXT("Exact Finish collapses historical Hand visual until reducer refresh"), Historical->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Exact Finish removes transient moving card"), Fixture.PlayArea->GetChildrenCount(), 0);
	TestEqual(TEXT("Finish still does not mutate frozen DrawCount directly"), Fixture.ViewModel->DrawCount, 4);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
