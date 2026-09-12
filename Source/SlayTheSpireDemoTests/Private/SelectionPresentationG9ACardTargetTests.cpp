#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleBufferedPlayerIntent.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG9ACardTargetTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		UPhase6UIA2APlaybackWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

			Player->MaxHP = 100;
			Enemy->MaxHP = 100;
			Player->PresentationId = TEXT("G9ACardPlayer");
			Enemy->PresentationId = TEXT("G9ACardEnemy");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 2;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->bEnableCommittedPresentationRecording = true;

			Battle->DebugStartingDeck.Reset();
			Battle->DebugStartingDeck.Add(MakePlain(TEXT("G9A.CardA")));
			Battle->DebugStartingDeck.Add(MakePlain(TEXT("G9A.CardB")));
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();

			ViewModel = NewObject<UBattleHUDViewModel>(World);
			Widget = NewObject<UPhase6UIA2APlaybackWidget>(World);
			Controller = NewObject<UBattlePresentationController>(World);
			if (!IsValid(ViewModel) || !IsValid(Widget) || !IsValid(Controller)) return;
			ViewModel->Initialize(Battle, true);
			Controller->Initialize(Battle, ViewModel, Widget);
		}

		~FFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		UCardData* MakePlain(const TCHAR* Id)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(Id);
			Card->DisplayName = FText::FromString(Id);
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		bool IsReady() const
		{
			return IsValid(Battle)
				&& IsValid(ViewModel)
				&& IsValid(Widget)
				&& IsValid(Controller)
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}

		UCardInstance* FindHand(FName CardId) const
		{
			UDeckRuntime* Deck = IsValid(Battle) ? Battle->GetDeckRuntimeForTesting() : nullptr;
			if (!IsValid(Deck)) return nullptr;
			for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
			{
				if (IsValid(Card.Get()) && Card->GetCardId() == CardId)
				{
					return Card.Get();
				}
			}
			return nullptr;
		}

		bool BeginPlayedCardLag(int32& OutSurvivorRuntimeId)
		{
			OutSurvivorRuntimeId = INDEX_NONE;
			UCardInstance* CardA = FindHand(TEXT("G9A.CardA"));
			UCardInstance* CardB = FindHand(TEXT("G9A.CardB"));
			if (!IsValid(CardA) || !IsValid(CardB)) return false;
			OutSurvivorRuntimeId = CardB->GetRuntimeId();

			const FGameplayRequestResult Result = Battle->RequestPlayCard(CardA, nullptr);
			if (!Result.IsAcceptedForResolution()) return false;

			Battle->FlushScheduledReadStateReadyForTesting();
			return Controller->IsWaitingForCompletionForTesting();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9ACardTargetWaitReadyTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.CardTarget.ExactWaitReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9ACardTargetWaitReadyTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ACardTargetTest;
	FFixture Fixture;
	if (!TestTrue(TEXT("G9-A card fixture is ready"), Fixture.IsReady())) return false;

	int32 SurvivorRuntimeId = INDEX_NONE;
	if (!TestTrue(TEXT("Played-card Presentation lag begins"),
		Fixture.BeginPlayedCardLag(SurvivorRuntimeId))) return false;

	const FPresentationPlaybackToken Playback = Fixture.Controller->GetActivePlaybackTokenForTesting();
	if (!TestTrue(TEXT("Approved card playback token is valid"), Playback.IsValid())) return false;

	FBufferedCardIntent Intent;
	if (!TestTrue(TEXT("Controller captures exact already-sealed card target"),
		Fixture.Controller->TryCaptureBufferedCardTarget(SurvivorRuntimeId, Intent))) return false;
	TestTrue(TEXT("Captured card intent is valid"), Intent.IsValid());
	TestEqual(TEXT("Capture window uses exact source Resolution"),
		Intent.CaptureWindow.SourceResolutionId, Playback.ResolutionId);
	TestEqual(TEXT("Capture window uses exact source PresentationSequence"),
		Intent.CaptureWindow.SourcePresentationSequence, Playback.PresentationSequence);
	TestEqual(TEXT("Capture window uses exact local playback generation"),
		Intent.CaptureWindow.LocalWindowGeneration,
		static_cast<uint64>(Playback.LocalPlaybackGeneration));
	TestEqual(TEXT("Displayed lag evaluates Waiting"),
		Fixture.Controller->EvaluateBufferedCardTarget(Intent),
		EBufferedIntentShadowEvaluation::Waiting);

	Fixture.Controller->SkipPresentation();
	TestFalse(TEXT("Skip catches Presentation up"), Fixture.Controller->HasSkippablePresentationDelay());
	TestEqual(TEXT("Exact caught-up target evaluates Ready"),
		Fixture.Controller->EvaluateBufferedCardTarget(Intent),
		EBufferedIntentShadowEvaluation::Ready);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9ACardTargetWindowGateTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.CardTarget.ApprovedWindowOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9ACardTargetWindowGateTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ACardTargetTest;
	FFixture Fixture;
	if (!TestTrue(TEXT("G9-A card fixture is ready"), Fixture.IsReady())) return false;

	UCardInstance* CardB = Fixture.FindHand(TEXT("G9A.CardB"));
	if (!TestNotNull(TEXT("Surviving card exists"), CardB)) return false;

	Fixture.Battle->TestGainBlock();
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	if (!TestTrue(TEXT("BlockChanged Presentation is asynchronously active"),
		Fixture.Controller->IsWaitingForCompletionForTesting())) return false;

	FBufferedCardIntent Rejected;
	TestFalse(TEXT("Non-card Blocking playback cannot mint a card buffer credential"),
		Fixture.Controller->TryCaptureBufferedCardTarget(CardB->GetRuntimeId(), Rejected));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9ACardTargetStaleRevisionTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.CardTarget.SealedTargetChangeStales",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9ACardTargetStaleRevisionTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ACardTargetTest;
	FFixture Fixture;
	if (!TestTrue(TEXT("G9-A card fixture is ready"), Fixture.IsReady())) return false;

	int32 SurvivorRuntimeId = INDEX_NONE;
	if (!TestTrue(TEXT("Played-card Presentation lag begins"),
		Fixture.BeginPlayedCardLag(SurvivorRuntimeId))) return false;

	FBufferedCardIntent Intent;
	if (!TestTrue(TEXT("Initial exact card target captures"),
		Fixture.Controller->TryCaptureBufferedCardTarget(SurvivorRuntimeId, Intent))) return false;

	// Test-only authoritative mutation while Presentation remains behind: a newer
	// sealed target must invalidate the older exact revision rather than treating
	// revision ordering as a permission range.
	Fixture.Battle->TestGainBlock();
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	TestEqual(TEXT("A different latest sealed target makes old credential stale"),
		Fixture.Controller->EvaluateBufferedCardTarget(Intent),
		EBufferedIntentShadowEvaluation::Stale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9ACardTargetSessionReplacementTest,
	"SlayTheSpireDemo.SelectionPresentation.G9A.CardTarget.SessionReplacementStales",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9ACardTargetSessionReplacementTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9ACardTargetTest;
	FFixture Fixture;
	if (!TestTrue(TEXT("G9-A card fixture is ready"), Fixture.IsReady())) return false;

	int32 SurvivorRuntimeId = INDEX_NONE;
	if (!TestTrue(TEXT("Played-card Presentation lag begins"),
		Fixture.BeginPlayedCardLag(SurvivorRuntimeId))) return false;

	FBufferedCardIntent Intent;
	if (!TestTrue(TEXT("Exact card target captures before replacement"),
		Fixture.Controller->TryCaptureBufferedCardTarget(SurvivorRuntimeId, Intent))) return false;

	UPhase6UIA2APlaybackWidget* ReplacementWidget =
		NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	if (!TestNotNull(TEXT("Replacement Widget exists"), ReplacementWidget)) return false;
	Fixture.Controller->SetWidget(ReplacementWidget);

	TestFalse(TEXT("Captured Presentation session is no longer current"),
		Fixture.Controller->IsCurrentPresentationSession(Intent.SessionToken));
	TestEqual(TEXT("Binding replacement makes buffered card credential stale"),
		Fixture.Controller->EvaluateBufferedCardTarget(Intent),
		EBufferedIntentShadowEvaluation::Stale);
	return true;
}

#endif
