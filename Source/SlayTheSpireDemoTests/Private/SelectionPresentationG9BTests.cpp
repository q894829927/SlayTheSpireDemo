#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA0TestTypes.h"
#include "Phase6UIA2ATestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Combat/Combatant.h"
#include "Containers/Ticker.h"
#include "Deck/DeckRuntime.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "UI/BattleBufferedPlayerIntent.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG9BTest
{
	struct FBaseFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

		explicit FBaseFixture(bool bRecording)
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
			Player->PresentationId = TEXT("G9BPlayer");
			Enemy->PresentationId = TEXT("G9BEnemy");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->bEnableCommittedPresentationRecording = bRecording;
		}

		virtual ~FBaseFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		UCardData* MakeCard(const TCHAR* Id, ECardTargetType TargetType = ECardTargetType::None)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(Id);
			Card->DisplayName = FText::FromString(Id);
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = TargetType == ECardTargetType::Enemy ? ECardType::Attack : ECardType::Skill;
			Card->TargetType = TargetType;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
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
	};

	struct FCardLagFixture : FBaseFixture
	{
		UBattleHUDViewModel* ViewModel = nullptr;
		UPhase6UIA2APlaybackWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;

		FCardLagFixture()
			: FBaseFixture(true)
		{
			if (!IsValid(Battle)) return;
			Battle->OpeningHandDrawCount = 2;
			Battle->DebugStartingDeck.Add(MakeCard(TEXT("G9B.CardA")));
			Battle->DebugStartingDeck.Add(MakeCard(TEXT("G9B.CardB")));
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();

			ViewModel = NewObject<UBattleHUDViewModel>(World);
			Widget = NewObject<UPhase6UIA2APlaybackWidget>(World);
			Controller = NewObject<UBattlePresentationController>(World);
			if (!IsValid(ViewModel) || !IsValid(Widget) || !IsValid(Controller)) return;
			ViewModel->Initialize(Battle, true);
			Controller->Initialize(Battle, ViewModel, Widget);
		}

		~FCardLagFixture() override
		{
			if (IsValid(ViewModel)) ViewModel->ClearBufferedPlayerIntentG9();
			if (IsValid(Controller)) Controller->Shutdown();
		}

		bool IsReady() const
		{
			return IsValid(Battle) && IsValid(ViewModel) && IsValid(Controller)
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}

		bool BeginLag(int32& OutSurvivorRuntimeId)
		{
			OutSurvivorRuntimeId = INDEX_NONE;
			UCardInstance* CardA = FindHand(TEXT("G9B.CardA"));
			UCardInstance* CardB = FindHand(TEXT("G9B.CardB"));
			if (!IsValid(CardA) || !IsValid(CardB)) return false;
			OutSurvivorRuntimeId = CardB->GetRuntimeId();
			const FGameplayRequestResult Result = Battle->RequestPlayCard(CardA, nullptr);
			if (!Result.IsAcceptedForResolution()) return false;
			Battle->FlushScheduledReadStateReadyForTesting();
			return Controller->IsWaitingForCompletionForTesting();
		}
	};

	struct FDirectFixture : FBaseFixture
	{
		UBattleHUDViewModel* ViewModel = nullptr;

		FDirectFixture(ECardTargetType OptionalCardTarget = ECardTargetType::None, bool bAddCard = false)
			: FBaseFixture(false)
		{
			if (!IsValid(Battle)) return;
			if (bAddCard)
			{
				Battle->OpeningHandDrawCount = 1;
				Battle->DebugStartingDeck.Add(MakeCard(TEXT("G9B.DirectCard"), OptionalCardTarget));
			}
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			ViewModel = NewObject<UBattleHUDViewModel>(World);
			if (IsValid(ViewModel)) ViewModel->Initialize(Battle, false);
		}

		~FDirectFixture() override
		{
			if (IsValid(ViewModel)) ViewModel->ClearBufferedPlayerIntentG9();
		}

		bool IsReady() const
		{
			return IsValid(Battle) && IsValid(ViewModel)
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9BBufferedCardReplayTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.Card.BufferedReplayExactOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9BBufferedCardReplayTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9BTest;
	FCardLagFixture Fixture;
	if (!TestTrue(TEXT("G9-B card-lag fixture is ready"), Fixture.IsReady())) return false;

	int32 SurvivorRuntimeId = INDEX_NONE;
	if (!TestTrue(TEXT("Played-card blocking lag begins"), Fixture.BeginLag(SurvivorRuntimeId))) return false;

	FBufferedCardIntent Intent;
	if (!TestTrue(TEXT("Exact sealed target captures"),
		Fixture.Controller->TryCaptureBufferedCardTarget(SurvivorRuntimeId, Intent))) return false;
	if (!TestTrue(TEXT("Production owner stores one buffered card selection"),
		Fixture.ViewModel->StoreBufferedCardSelectionG9(Intent, Fixture.Controller))) return false;

	Fixture.ViewModel->RefreshBufferedPlayerIntentG9();
	TestTrue(TEXT("Card remains buffered while Presentation is behind"),
		Fixture.ViewModel->HasBufferedCardSelectionG9());
	TestEqual(TEXT("Old click has not selected early"),
		Fixture.ViewModel->SelectedCardRuntimeId, INDEX_NONE);

	Fixture.Controller->SkipPresentation();
	Fixture.ViewModel->RefreshBufferedPlayerIntentG9();
	TestFalse(TEXT("Exact ready replay consumes buffered card first"),
		Fixture.ViewModel->HasBufferedCardSelectionG9());
	TestEqual(TEXT("Buffered click replays normal SelectCard exactly once"),
		Fixture.ViewModel->SelectedCardRuntimeId, SurvivorRuntimeId);
	TestEqual(TEXT("Untargeted buffered card stops at ReadyToConfirm"),
		Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::ReadyToConfirm);

	Fixture.ViewModel->RefreshBufferedPlayerIntentG9();
	TestEqual(TEXT("No second replay toggles the selection off"),
		Fixture.ViewModel->SelectedCardRuntimeId, SurvivorRuntimeId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9BEndTurnOverridesCardTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.Arbitration.EndTurnOverridesCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9BEndTurnOverridesCardTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9BTest;
	FCardLagFixture Fixture;
	if (!TestTrue(TEXT("G9-B arbitration fixture is ready"), Fixture.IsReady())) return false;

	int32 SurvivorRuntimeId = INDEX_NONE;
	if (!TestTrue(TEXT("Played-card blocking lag begins"), Fixture.BeginLag(SurvivorRuntimeId))) return false;
	FBufferedCardIntent Intent;
	if (!TestTrue(TEXT("Card target captures"),
		Fixture.Controller->TryCaptureBufferedCardTarget(SurvivorRuntimeId, Intent))) return false;
	if (!TestTrue(TEXT("Card future intent stores"),
		Fixture.ViewModel->StoreBufferedCardSelectionG9(Intent, Fixture.Controller))) return false;

	TestTrue(TEXT("EndTurn can be accepted during the same player turn Presentation lag"),
		Fixture.ViewModel->TryAcceptEndTurnIntentG9());
	TestFalse(TEXT("Accepted EndTurn atomically retires buffered card"),
		Fixture.ViewModel->HasBufferedCardSelectionG9());
	TestTrue(TEXT("EndTurn becomes the single pending G9 intent"),
		Fixture.ViewModel->HasBufferedEndTurnG9());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9BResolutionBusyEndTurnTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.EndTurn.DirectBaselineResolutionBusy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9BResolutionBusyEndTurnTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9BTest;
	FDirectFixture Fixture;
	if (!TestTrue(TEXT("DirectBaseline fixture is ready"), Fixture.IsReady())) return false;

	FPlayerTurnAuthorityToken FirstTurn;
	if (!TestTrue(TEXT("First player-turn token exists"),
		Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(FirstTurn))) return false;

	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	if (!TestNotNull(TEXT("ActionQueue exists"), Queue)) return false;
	UPhase6UIA0ManualFinishAction* Hold = NewObject<UPhase6UIA0ManualFinishAction>(Queue);
	if (!TestNotNull(TEXT("Manual hold action exists"), Hold)) return false;
	if (!TestTrue(TEXT("Manual hold action enqueues"), Queue->AddToBack(Hold))) return false;
	if (!TestTrue(TEXT("Manual hold action starts"), Queue->StartProcessing())) return false;
	TestTrue(TEXT("Queue is ResolutionBusy"), Queue->IsBusy() && Hold->HasExecuted());

	TestTrue(TEXT("EndTurn intent remains acceptable while same turn is ResolutionBusy"),
		Fixture.ViewModel->CanAcceptEndTurnIntentG9());
	TestTrue(TEXT("Physical EndTurn intent captures exact turn authority"),
		Fixture.ViewModel->TryAcceptEndTurnIntentG9());
	TestTrue(TEXT("Finalize stores the waiting EndTurn instead of executing early"),
		Fixture.ViewModel->FinalizeAcceptedEndTurnIntentG9());
	TestTrue(TEXT("Buffered EndTurn remains pending while busy"),
		Fixture.ViewModel->HasBufferedEndTurnG9());

	Hold->CompleteManually();
	TestFalse(TEXT("Resolution settles before deferred replay"), Queue->IsBusy());
	TestTrue(TEXT("ResolutionIdle only schedules replay; it does not re-enter immediately"),
		Fixture.ViewModel->HasBufferedEndTurnG9());

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestFalse(TEXT("Next-tick exact replay consumes EndTurn"),
		Fixture.ViewModel->HasBufferedEndTurnG9());

	FPlayerTurnAuthorityToken SecondTurn;
	if (!TestTrue(TEXT("Battle reaches the next player turn"),
		Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(SecondTurn))) return false;
	TestTrue(TEXT("Old buffered EndTurn cannot ABA-match the new turn"), SecondTurn != FirstTurn);
	TestEqual(TEXT("EndTurn executed exactly once"), SecondTurn.PlayerTurnSerial, uint64(2));

	Fixture.ViewModel->RefreshBufferedPlayerIntentG9();
	FPlayerTurnAuthorityToken AfterExtraRefresh;
	Fixture.Battle->TryGetCurrentPlayerTurnAuthorityToken(AfterExtraRefresh);
	TestTrue(TEXT("No duplicate replay advances another turn"), AfterExtraRefresh == SecondTurn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9BReadyToConfirmEndTurnTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.EndTurn.ReadyToConfirmSupersede",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9BReadyToConfirmEndTurnTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9BTest;
	FDirectFixture Fixture(ECardTargetType::None, true);
	if (!TestTrue(TEXT("ReadyToConfirm fixture is ready"), Fixture.IsReady())) return false;
	UCardInstance* Card = Fixture.FindHand(TEXT("G9B.DirectCard"));
	if (!TestNotNull(TEXT("Direct card exists"), Card)) return false;

	if (!TestTrue(TEXT("Normal card selection enters ReadyToConfirm"),
		Fixture.ViewModel->SelectCardByRuntimeId(Card->GetRuntimeId()))) return false;
	TestEqual(TEXT("ReadyToConfirm established"), Fixture.ViewModel->InteractionState,
		EBattleHUDInteractionState::ReadyToConfirm);

	TestTrue(TEXT("EndTurn is accepted before destructive selection retirement"),
		Fixture.ViewModel->TryAcceptEndTurnIntentG9());
	TestEqual(TEXT("Acceptance alone preserves the old transient selection"),
		Fixture.ViewModel->SelectedCardRuntimeId, Card->GetRuntimeId());

	TestTrue(TEXT("Finalization applies the G9 scoped EndTurn supersede"),
		Fixture.ViewModel->FinalizeAcceptedEndTurnIntentG9());
	TestEqual(TEXT("Transient card selection is retired after acceptance"),
		Fixture.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestFalse(TEXT("Immediate legal EndTurn leaves no buffered intent"),
		Fixture.ViewModel->HasBufferedEndTurnG9());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9BChoosingTargetEndTurnTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.EndTurn.ChoosingTargetSupersede",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9BChoosingTargetEndTurnTest::RunTest(const FString& Parameters)
{
	using namespace SelectionPresentationG9BTest;
	FDirectFixture Fixture(ECardTargetType::Enemy, true);
	if (!TestTrue(TEXT("ChoosingTarget fixture is ready"), Fixture.IsReady())) return false;
	UCardInstance* Card = Fixture.FindHand(TEXT("G9B.DirectCard"));
	if (!TestNotNull(TEXT("Enemy-target card exists"), Card)) return false;

	if (!TestTrue(TEXT("Normal target card selection succeeds"),
		Fixture.ViewModel->SelectCardByRuntimeId(Card->GetRuntimeId()))) return false;
	TestEqual(TEXT("ChoosingTarget established"), Fixture.ViewModel->InteractionState,
		EBattleHUDInteractionState::ChoosingTarget);

	TestTrue(TEXT("G9 accepts EndTurn on the exact player-turn authority"),
		Fixture.ViewModel->TryAcceptEndTurnIntentG9());
	TestEqual(TEXT("Selection is still intact at the acceptance boundary"),
		Fixture.ViewModel->SelectedCardRuntimeId, Card->GetRuntimeId());
	TestTrue(TEXT("G9 finalization cancels target choice then ends turn"),
		Fixture.ViewModel->FinalizeAcceptedEndTurnIntentG9());
	TestEqual(TEXT("ChoosingTarget selection is cleared after accepted EndTurn"),
		Fixture.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	return true;
}

#endif
