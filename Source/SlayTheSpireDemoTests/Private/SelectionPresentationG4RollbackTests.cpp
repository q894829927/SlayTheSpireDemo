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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG4SelectionAreaRollbackTest,
	"SlayTheSpireDemo.CardSelection.Presentation.G4.SelectionAreaPrepareRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG4SelectionAreaRollbackTest::RunTest(const FString& Parameters)
{
	constexpr int64 BattleId = 9301;
	constexpr int64 ResolutionId = 9302;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("World exists"), World)) return false;

	UCardSelectionPresentationHUDProbe* HUD = NewObject<UCardSelectionPresentationHUDProbe>(World);
	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(HUD);
	UHorizontalBox* Hand = NewObject<UHorizontalBox>(HUD);
	UOverlay* PlayArea = NewObject<UOverlay>(HUD);
	UOverlay* SelectionArea = NewObject<UOverlay>(HUD);
	UTextBlock* DrawCount = NewObject<UTextBlock>(HUD);
	UTextBlock* DiscardCount = NewObject<UTextBlock>(HUD);
	UTextBlock* ExhaustCount = NewObject<UTextBlock>(HUD);
	if (!TestNotNull(TEXT("HUD exists"), HUD)
		|| !TestNotNull(TEXT("ViewModel exists"), ViewModel)
		|| !TestNotNull(TEXT("Hand exists"), Hand)
		|| !TestNotNull(TEXT("PlayArea exists"), PlayArea)
		|| !TestNotNull(TEXT("SelectionArea exists"), SelectionArea))
	{
		World->DestroyWorld(false);
		return false;
	}

	HUD->SetTestWorld(World);
	HUD->ConfigureSelectionSurfaces(
		ViewModel,
		Hand,
		PlayArea,
		DrawCount,
		DiscardCount,
		ExhaustCount);
	HUD->SetSelectionAreaHostForTesting(SelectionArea);
	ViewModel->BattleId = BattleId;
	ViewModel->StateRevision = 100;
	ViewModel->DrawCount = 1;
	ViewModel->DiscardCount = 0;
	ViewModel->ExhaustCount = 0;

	FPresentationCardSnapshot Snapshot;
	Snapshot.RuntimeId = 731;
	Snapshot.CardId = TEXT("G4RollbackProbe");
	Snapshot.DisplayName = FText::FromString(TEXT("G4 Rollback Probe"));
	Snapshot.Cost = 1;
	Snapshot.CardType = ECardType::Skill;
	Snapshot.Rarity = ECardRarity::Uncommon;
	Snapshot.CardColor = ECardColor::Red;
	Snapshot.TargetType = ECardTargetType::None;
	Snapshot.Description = FText::FromString(TEXT("Rollback probe."));

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
	ViewModel->HandCards.Add(View);

	UPhase6UIA2NR8CardProbe* Formal = NewObject<UPhase6UIA2NR8CardProbe>(HUD);
	Formal->SetCardView(View);
	Formal->SetVisibility(ESlateVisibility::Hidden);
	Hand->AddChild(Formal);

	UPhase6UIA2NR8CardProbe* SelectionVisual = NewObject<UPhase6UIA2NR8CardProbe>(HUD);
	SelectionVisual->SetCardView(View);
	SelectionVisual->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SelectionVisual->SetRenderTranslation(FVector2D(77.0f, -55.0f));
	SelectionVisual->SetRenderScale(FVector2D(1.08f, 1.08f));
	SelectionVisual->SetRenderOpacity(0.63f);
	SelectionVisual->SetIsEnabled(false);
	SelectionArea->AddChild(SelectionVisual);

	const FWidgetTransform SourceTransform = SelectionVisual->GetRenderTransform();
	const float SourceOpacity = SelectionVisual->GetRenderOpacity();
	const ESlateVisibility SourceVisibility = SelectionVisual->GetVisibility();
	const bool bSourceEnabled = SelectionVisual->GetIsEnabled();

	const int64 SelectionGeneration =
		ViewModel->BeginCardPresentationSelectionLifecycle(ViewModel->StateRevision);
	TestTrue(TEXT("Selection lifecycle starts"), SelectionGeneration > 0);
	TestTrue(TEXT("Exact card enters SelectionArea"),
		ViewModel->SetPendingCardPresentationSelection(
			SelectionGeneration,
			Snapshot.RuntimeId,
			true));
	TArray<int32> ConfirmedIds;
	ConfirmedIds.Add(Snapshot.RuntimeId);
	TestTrue(TEXT("Exact card confirms"),
		ViewModel->ConfirmCardPresentationSelection(SelectionGeneration, ConfirmedIds));

	FPresentationRecord Record;
	Record.BattleId = BattleId;
	Record.ResolutionId = ResolutionId;
	Record.PresentationSequence = 1;
	Record.Type = EBattlePresentationRecordType::CardZoneChanged;
	Record.CardZoneChanged.Card = Snapshot;
	Record.CardZoneChanged.FromZone = ECardZone::Hand;
	Record.CardZoneChanged.ToZone = ECardZone::DrawPile;
	Record.CardZoneChanged.FromIndex = 0;
	Record.CardZoneChanged.ToIndex = ViewModel->DrawCount;

	FPresentationPlaybackToken Token;
	Token.BattleId = BattleId;
	Token.ResolutionId = ResolutionId;
	Token.PresentationSequence = 1;
	Token.LocalPlaybackGeneration = 1;

	// Force failure after the exact SelectionArea object has been prepared and
	// reparented: without a World the local finish timer cannot be armed.
	HUD->SetTestWorld(nullptr);
	TestFalse(TEXT("Post-prepare timer failure declines the Record"),
		HUD->PlayPresentationRecord(Record, Token));
	TestTrue(TEXT("Rollback restores the exact object to SelectionArea"),
		SelectionVisual->GetParent() == SelectionArea);
	TestEqual(TEXT("Rollback leaves no PlayArea child"), PlayArea->GetChildrenCount(), 0);
	TestEqual(TEXT("Rollback leaves no active transition child"),
		HUD->GetActiveNativeCardTransitionCountForTesting(), 0);
	TestTrue(TEXT("Rollback restores source translation"),
		SelectionVisual->GetRenderTransform().Translation.Equals(SourceTransform.Translation));
	TestTrue(TEXT("Rollback restores source scale"),
		SelectionVisual->GetRenderTransform().Scale.Equals(SourceTransform.Scale));
	TestTrue(TEXT("Rollback restores source opacity"),
		FMath::IsNearlyEqual(SelectionVisual->GetRenderOpacity(), SourceOpacity));
	TestEqual(TEXT("Rollback restores source visibility"),
		SelectionVisual->GetVisibility(), SourceVisibility);
	TestEqual(TEXT("Rollback restores source enabled state"),
		SelectionVisual->GetIsEnabled(), bSourceEnabled);
	TestEqual(TEXT("Declined preparation does not advance ownership"),
		ViewModel->GetCardPresentationOwner(Snapshot.RuntimeId),
		ECardPresentationOwner::SelectionArea);

	HUD->SetTestWorld(World);
	FPresentationPlaybackToken RetryToken = Token;
	RetryToken.LocalPlaybackGeneration = 2;
	TestTrue(TEXT("Same exact SelectionArea object can retry after rollback"),
		HUD->PlayPresentationRecord(Record, RetryToken));
	TestTrue(TEXT("Retry moves the exact object to transition surface"),
		HUD->GetActiveNativeCardTransitionVisualForTesting() == SelectionVisual
			&& SelectionVisual->GetParent() == PlayArea);
	TestEqual(TEXT("Accepted retry advances ownership to Transition"),
		ViewModel->GetCardPresentationOwner(Snapshot.RuntimeId),
		ECardPresentationOwner::Transition);
	HUD->FinishNativeCardTransitionForTesting(RetryToken);
	FTSTicker::GetCoreTicker().Tick(0.0f);

	HUD->SkipPresentation();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
