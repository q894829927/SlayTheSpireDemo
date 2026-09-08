#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleSelectionRequest.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/SelectHandCardToDrawPileTopEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Selection/SelectionResolver.h"
#include "UI/BattleHUDViewModel.h"
#include "UI/BattleHUDWidget.h"
#include "Engine/World.h"

namespace CardSelectionInputRoutingTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(),
				FTransform(FVector(100.0, 0.0, 0.0)),
				SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle))
			{
				return;
			}

			Player->MaxHP = 100;
			Enemy->MaxHP = 100;
			Player->PresentationId = TEXT("PlayerHero");
			Enemy->PresentationId = TEXT("EnemyPrimary");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
		}

		~FFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		UCardData* CreateCard(const TCHAR* CardId, ECardTargetType TargetType = ECardTargetType::None)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(CardId);
			Card->DisplayName = FText::FromString(CardId);
			Card->Description = FText::FromString(TEXT("Selection routing test card."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = TargetType == ECardTargetType::Enemy ? ECardType::Attack : ECardType::Skill;
			Card->TargetType = TargetType;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* CreateSelectionCard(const TCHAR* CardId)
		{
			UCardData* Card = CreateCard(CardId);
			USelectHandCardToDrawPileTopEffect* Effect = NewObject<USelectHandCardToDrawPileTopEffect>(Card);
			Effect->BaseSelectionCount = 1;
			Effect->UpgradedSelectionCount = 1;
			Card->Effects.Add(Effect);
			return Card;
		}

		bool Start(const TArray<UCardData*>& Definitions)
		{
			if (!IsValid(Battle) || Definitions.IsEmpty())
			{
				return false;
			}
			Battle->DebugStartingDeck.Reset();
			for (UCardData* Definition : Definitions)
			{
				Battle->DebugStartingDeck.Add(Definition);
			}
			Battle->OpeningHandDrawCount = Definitions.Num();
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			return Battle->BattleState == EBattleState::PlayerTurn
				&& IsValid(Battle->GetDeckRuntimeForTesting());
		}

		UCardInstance* FindHandCard(FName CardId) const
		{
			UDeckRuntime* Deck = IsValid(Battle) ? Battle->GetDeckRuntimeForTesting() : nullptr;
			if (!IsValid(Deck))
			{
				return nullptr;
			}
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
}

using namespace CardSelectionInputRoutingTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPendingBoundaryBlocksOrdinaryCardPlayTest,
	"SlayTheSpireDemo.CardSelection.Presentation.Input.PendingBoundaryBlocksOrdinaryCardPlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPendingBoundaryBlocksOrdinaryCardPlayTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* SelectionCard = Fixture.CreateSelectionCard(TEXT("RoutingSelection"));
	UCardData* CandidateCard = Fixture.CreateCard(TEXT("RoutingCandidate"), ECardTargetType::Enemy);
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ SelectionCard, CandidateCard })))
	{
		return false;
	}

	UCardInstance* PlayedCard = Fixture.FindHandCard(TEXT("RoutingSelection"));
	UCardInstance* Candidate = Fixture.FindHandCard(TEXT("RoutingCandidate"));
	if (!TestNotNull(TEXT("Selection card exists"), PlayedCard)
		|| !TestNotNull(TEXT("Candidate card exists"), Candidate))
	{
		return false;
	}

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	UBattleHUDWidget* HUD = NewObject<UBattleHUDWidget>(Fixture.World);
	if (!TestNotNull(TEXT("ViewModel created"), ViewModel)
		|| !TestTrue(TEXT("ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, false))
		|| !TestNotNull(TEXT("HUD created"), HUD))
	{
		return false;
	}
	HUD->SetViewModel(ViewModel);

	const int64 DisplayedBattleIdBefore = ViewModel->BattleId;
	const int64 DisplayedRevisionBefore = ViewModel->StateRevision;
	const FText FeedbackBefore = ViewModel->LastFeedback;

	TestTrue(TEXT("Selection card play is accepted"),
		Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution());
	TestTrue(TEXT("Gameplay owns an authoritative pending card selection"),
		ViewModel->HasAuthoritativePendingCardSelection());
	TestTrue(TEXT("Gameplay resolver remains pending"),
		Fixture.Battle->GetSelectionResolver()->HasPendingSelection());

	// Model the exact catch-up window deterministically even if a platform happens
	// to dispatch a scheduled read edge sooner than expected. Candidate exposure
	// remains gated because the displayed revision is still the preceding one.
	ViewModel->BattleId = DisplayedBattleIdBefore;
	ViewModel->StateRevision = DisplayedRevisionBefore;
	TestFalse(TEXT("Pending selection is not display-readable before boundary catch-up"),
		ViewModel->HasPendingCardSelection());
	TestTrue(TEXT("Authoritative pending selection remains visible to fail-closed routing"),
		ViewModel->HasAuthoritativePendingCardSelection());

	TestFalse(TEXT("Catch-up-window candidate click is not accepted as ordinary card play"),
		HUD->SelectCard(Candidate->GetRuntimeId()));
	TestEqual(TEXT("Ordinary card selection state is not entered"),
		ViewModel->SelectedCardRuntimeId,
		INDEX_NONE);
	TestTrue(TEXT("Fail-closed click does not replace feedback with ordinary play validation"),
		ViewModel->LastFeedback.EqualTo(FeedbackBefore));
	TestTrue(TEXT("Gameplay selection stays pending after swallowed click"),
		Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestFalse(TEXT("No resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
