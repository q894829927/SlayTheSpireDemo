#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR8TestTypes.h"
#include "Containers/Ticker.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeR8SkipClearsRetainedPlayedCardTest,
	"SlayTheSpireDemo.Phase6UIA2N.R8.Zone.SkipClearsRetainedPlayedCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeR8SkipClearsRetainedPlayedCardTest::RunTest(const FString& Parameters)
{
	constexpr int64 BattleId = 861;
	constexpr int64 ResolutionId = 862;
	const FName PlayerPresentationId(TEXT("PlayerPresentation"));
	const FName EnemyPresentationId(TEXT("EnemyPresentation"));

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	UPhase6UIA2NR8HUDProbe* Probe = IsValid(World)
		? NewObject<UPhase6UIA2NR8HUDProbe>(World)
		: nullptr;
	UBattleHUDViewModel* ViewModel = IsValid(Probe)
		? NewObject<UBattleHUDViewModel>(Probe)
		: nullptr;
	UHorizontalBox* Hand = IsValid(Probe) ? NewObject<UHorizontalBox>(Probe) : nullptr;
	UOverlay* PlayArea = IsValid(Probe) ? NewObject<UOverlay>(Probe) : nullptr;
	UTextBlock* DrawCount = IsValid(Probe) ? NewObject<UTextBlock>(Probe) : nullptr;
	UTextBlock* DiscardCount = IsValid(Probe) ? NewObject<UTextBlock>(Probe) : nullptr;
	UTextBlock* Energy = IsValid(Probe) ? NewObject<UTextBlock>(Probe) : nullptr;

	if (!IsValid(World) || !IsValid(Probe) || !IsValid(ViewModel)
		|| !IsValid(Hand) || !IsValid(PlayArea) || !IsValid(DrawCount)
		|| !IsValid(DiscardCount) || !IsValid(Energy))
	{
		AddError(TEXT("Failed to create the R8 Skip-cleanup fixture."));
		if (IsValid(World))
		{
			World->DestroyWorld(false);
		}
		return false;
	}

	Probe->SetTestWorld(World);
	Probe->SetViewModelForTesting(ViewModel);
	Probe->ConfigureCardSurfaces(Hand, PlayArea, DrawCount, DiscardCount, Energy);
	ViewModel->Player.PresentationId = PlayerPresentationId;
	ViewModel->Enemy.PresentationId = EnemyPresentationId;
	ViewModel->Energy = 3;
	ViewModel->MaxEnergy = 3;
	ViewModel->DrawCount = 1;
	ViewModel->DiscardCount = 0;
	DrawCount->SetText(FText::AsNumber(1));
	DiscardCount->SetText(FText::AsNumber(0));
	Energy->SetText(FText::FromString(TEXT("3/3")));

	FPresentationCardSnapshot PlayedSnapshot;
	PlayedSnapshot.RuntimeId = 8601;
	PlayedSnapshot.CardId = TEXT("SkipPlayed");
	PlayedSnapshot.DisplayName = FText::FromString(TEXT("SkipPlayed"));
	PlayedSnapshot.Cost = 1;
	PlayedSnapshot.CardType = ECardType::Attack;
	PlayedSnapshot.TargetType = ECardTargetType::Enemy;
	PlayedSnapshot.Description = FText::FromString(TEXT("Frozen played card"));

	FBattleHUDCardView FormalView;
	FormalView.RuntimeId = PlayedSnapshot.RuntimeId;
	FormalView.CardId = PlayedSnapshot.CardId;
	FormalView.DisplayName = PlayedSnapshot.DisplayName;
	FormalView.Cost = PlayedSnapshot.Cost;
	FormalView.CardType = PlayedSnapshot.CardType;
	FormalView.TargetType = PlayedSnapshot.TargetType;
	FormalView.Description = PlayedSnapshot.Description;
	FormalView.bGameplayPlayable = true;
	ViewModel->HandCards.Add(FormalView);

	UPhase6UIA2NR8CardProbe* FormalCard = NewObject<UPhase6UIA2NR8CardProbe>(Probe);
	FormalCard->SetCardView(FormalView);
	Hand->AddChild(FormalCard);

	FPresentationRecord PlayedRecord;
	PlayedRecord.BattleId = BattleId;
	PlayedRecord.ResolutionId = ResolutionId;
	PlayedRecord.PresentationSequence = 1;
	PlayedRecord.Type = EBattlePresentationRecordType::CardPlayed;
	PlayedRecord.CardPlayed.Card = PlayedSnapshot;
	PlayedRecord.CardPlayed.SourcePresentationId = PlayerPresentationId;
	PlayedRecord.CardPlayed.TargetPresentationId = EnemyPresentationId;
	PlayedRecord.CardPlayed.HandIndexBefore = 0;
	PlayedRecord.CardPlayed.PlayAreaIndexAfter = 0;
	PlayedRecord.CardPlayed.EnergyBefore = 3;
	PlayedRecord.CardPlayed.EnergyAfter = 2;
	PlayedRecord.CardPlayed.CostPaid = 1;

	FPresentationPlaybackToken PlayedToken;
	PlayedToken.BattleId = BattleId;
	PlayedToken.ResolutionId = ResolutionId;
	PlayedToken.PresentationSequence = 1;
	PlayedToken.LocalPlaybackGeneration = 1;

	TestTrue(TEXT("CardPlayed begins before Skip-cleanup scenario"),
		Probe->PlayPresentationRecord(PlayedRecord, PlayedToken));
	Probe->InvokeFinishForTesting(PlayedToken);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestNotNull(TEXT("Exact CardPlayed Finish intentionally retains PlayedCard"),
		Probe->PlayedCardForTesting());
	TestEqual(TEXT("Retained PlayedCard remains in PlayArea before later Record"),
		PlayArea->GetChildrenCount(), 1);

	// Simulate the Controller applying only the completed CardPlayed reducer state.
	ViewModel->HandCards.Reset();
	ViewModel->Energy = 2;
	Hand->ClearChildren();
	Energy->SetText(FText::FromString(TEXT("2/3")));

	FPresentationCardSnapshot DrawSnapshot;
	DrawSnapshot.RuntimeId = 8602;
	DrawSnapshot.CardId = TEXT("SkipDraw");
	DrawSnapshot.DisplayName = FText::FromString(TEXT("SkipDraw"));
	DrawSnapshot.Cost = 0;
	DrawSnapshot.CardType = ECardType::Skill;
	DrawSnapshot.TargetType = ECardTargetType::Self;
	DrawSnapshot.Description = FText::FromString(TEXT("Frozen drawn card"));

	FPresentationRecord DrawRecord;
	DrawRecord.BattleId = BattleId;
	DrawRecord.ResolutionId = ResolutionId;
	DrawRecord.PresentationSequence = 2;
	DrawRecord.Type = EBattlePresentationRecordType::CardZoneChanged;
	DrawRecord.CardZoneChanged.Card = DrawSnapshot;
	DrawRecord.CardZoneChanged.FromZone = ECardZone::DrawPile;
	DrawRecord.CardZoneChanged.ToZone = ECardZone::Hand;
	DrawRecord.CardZoneChanged.FromIndex = 0;
	DrawRecord.CardZoneChanged.ToIndex = 0;

	FPresentationPlaybackToken DrawToken;
	DrawToken.BattleId = BattleId;
	DrawToken.ResolutionId = ResolutionId;
	DrawToken.PresentationSequence = 2;
	DrawToken.LocalPlaybackGeneration = 2;

	TestTrue(TEXT("Later Draw Record begins while prior PlayedCard is retained"),
		Probe->PlayPresentationRecord(DrawRecord, DrawToken));
	TestEqual(TEXT("Draw owns one Hand transient before Skip"), Hand->GetChildrenCount(), 1);
	TestEqual(TEXT("Prior PlayedCard is still present before Skip"), PlayArea->GetChildrenCount(), 1);

	Probe->SkipPresentation();

	TestFalse(TEXT("Skip clears current local presentation ownership"),
		Probe->IsLocalPresentationActive());
	TestFalse(TEXT("Skip clears current local finish timer"),
		Probe->IsLocalFinishTimerSet());
	TestNull(TEXT("Skip removes current Draw transient"),
		Probe->DrawnCardForTesting());
	TestEqual(TEXT("Skip leaves no Draw transient in Hand"), Hand->GetChildrenCount(), 0);
	TestNull(TEXT("Skip also clears the retained cross-Record PlayedCard"),
		Probe->PlayedCardForTesting());
	TestEqual(TEXT("Skip leaves no stale PlayedCard in PlayArea"),
		PlayArea->GetChildrenCount(), 0);

	World->DestroyWorld(false);
	return true;
}

#endif
