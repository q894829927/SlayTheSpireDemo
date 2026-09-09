#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleSelectionRequest.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Selection/SelectionResolver.h"
#include "UI/BattleHUDViewModel.h"
#include "UI/BattleHUDWidget.h"
#include "Engine/World.h"

namespace CardExpansionWave1CC0NativeMultiSelectionTest
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
				SpawnParameters
			);
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

		UCardData* CreatePlainCard(const TCHAR* CardId)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(CardId);
			Card->DisplayName = FText::FromString(CardId);
			Card->Description = FText::FromString(TEXT("C0 native multi-selection test card."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* CreateSelectExhaustCard(const TCHAR* CardId, int32 Count)
		{
			UCardData* Card = CreatePlainCard(CardId);
			USelectExhaustHandCardEffect* Effect = NewObject<USelectExhaustHandCardEffect>(Card);
			Effect->BaseSelectionMode = ESelectExhaustSelectionMode::Player;
			Effect->BaseSelectionCount = Count;
			Effect->UpgradedSelectionMode = ESelectExhaustSelectionMode::Player;
			Effect->UpgradedSelectionCount = Count;
			Card->Effects.Add(Effect);
			return Card;
		}

		bool Start(const TArray<UCardData*>& Definitions)
		{
			if (!IsValid(Battle) || Definitions.Num() == 0)
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
			return IsValid(Battle->GetActionQueueForTesting())
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}

		UCardInstance* FindInstanceByCardId(UDeckRuntime* Deck, FName CardId) const
		{
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

using namespace CardExpansionWave1CC0NativeMultiSelectionTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0ExactNFacadeCanonicalSubmitTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.ExactNFacadeCanonicalSubmit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0ExactNFacadeCanonicalSubmitTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* EffectCard = Fixture.CreateSelectExhaustCard(TEXT("C0Facade"), 2);
	UCardData* OtherA = Fixture.CreatePlainCard(TEXT("C0FacadeA"));
	UCardData* OtherB = Fixture.CreatePlainCard(TEXT("C0FacadeB"));
	UCardData* OtherC = Fixture.CreatePlainCard(TEXT("C0FacadeC"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ EffectCard, OtherA, OtherB, OtherC })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("C0Facade"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard))
	{
		return false;
	}
	TestTrue(TEXT("Play accepted"), Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution());

	FPendingCardSelectionReadView View;
	if (!TestTrue(TEXT("Exact-N card read view is available"), BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Fixture.Battle, View)))
	{
		return false;
	}
	TestEqual(TEXT("RequiredCount is 2"), View.RequiredCount, 2);
	TestEqual(TEXT("Three candidate RuntimeIds exposed"), View.CandidateRuntimeIds.Num(), 3);

	TArray<int32> DuplicateIds{ View.CandidateRuntimeIds[0], View.CandidateRuntimeIds[0] };
	TestFalse(TEXT("Duplicate RuntimeIds rejected before Gameplay submit"), BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, DuplicateIds));
	TestTrue(TEXT("Duplicate rejection keeps mandatory request pending"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());

	TArray<int32> ForeignIds{ View.CandidateRuntimeIds[0], PlayedCard->GetRuntimeId() };
	TestFalse(TEXT("Non-candidate RuntimeId rejected"), BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, ForeignIds));
	TestTrue(TEXT("Foreign-id rejection keeps request pending"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());

	TArray<int32> ReverseClickOrder{ View.CandidateRuntimeIds[1], View.CandidateRuntimeIds[0] };
	TestTrue(TEXT("Two valid RuntimeIds submit"), BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, ReverseClickOrder));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	TestEqual(TEXT("Two cards exhaust"), Deck->GetExhaustCount(), 2);
	const TArray<TObjectPtr<UCardInstance>>& ExhaustCards = Deck->GetExhaustCards();
	if (TestEqual(TEXT("Exhaust pile contains two cards"), ExhaustCards.Num(), 2))
	{
		TestEqual(TEXT("Result normalized to candidate order [0]"), ExhaustCards[0]->GetRuntimeId(), View.CandidateRuntimeIds[0]);
		TestEqual(TEXT("Result normalized to candidate order [1]"), ExhaustCards[1]->GetRuntimeId(), View.CandidateRuntimeIds[1]);
	}
	TestFalse(TEXT("No resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0NativeHUDExactNClickTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.NativeHUDExactNClick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0NativeHUDExactNClickTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* EffectCard = Fixture.CreateSelectExhaustCard(TEXT("C0HUDMulti"), 2);
	UCardData* OtherA = Fixture.CreatePlainCard(TEXT("C0HUDMultiA"));
	UCardData* OtherB = Fixture.CreatePlainCard(TEXT("C0HUDMultiB"));
	UCardData* OtherC = Fixture.CreatePlainCard(TEXT("C0HUDMultiC"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ EffectCard, OtherA, OtherB, OtherC })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("C0HUDMulti"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard))
	{
		return false;
	}
	TestTrue(TEXT("Play accepted"), Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution());

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	UBattleHUDWidget* HUD = NewObject<UBattleHUDWidget>(Fixture.World);
	if (!TestNotNull(TEXT("ViewModel created"), ViewModel)
		|| !TestTrue(TEXT("ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, false))
		|| !TestNotNull(TEXT("HUD created"), HUD))
	{
		return false;
	}
	HUD->SetViewModel(ViewModel);

	FPendingCardSelectionReadView View;
	if (!TestTrue(TEXT("Pending read view available"), ViewModel->TryGetPendingCardSelectionReadView(View)))
	{
		return false;
	}
	TestEqual(TEXT("RequiredCount is 2"), View.RequiredCount, 2);
	const int32 FirstId = View.CandidateRuntimeIds[0];
	const int32 SecondId = View.CandidateRuntimeIds[1];

	TestFalse(TEXT("Played card is not a pending candidate"), HUD->SelectCard(PlayedCard->GetRuntimeId()));
	TestEqual(TEXT("Rejected non-candidate changes no local selection"), ViewModel->GetPendingCardSelectionSelectedCount(), 0);

	TestTrue(TEXT("First candidate click is accepted locally"), HUD->SelectCard(FirstId));
	TestEqual(TEXT("One RuntimeId is locally selected"), ViewModel->GetPendingCardSelectionSelectedCount(), 1);
	TestTrue(TEXT("First RuntimeId is marked selected"), ViewModel->IsPendingCardSelectionRuntimeIdSelected(FirstId));
	TestFalse(TEXT("One of two is not confirmable"), ViewModel->CanConfirmPendingCardSelection());
	TestTrue(TEXT("Gameplay remains pending at 1/2"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestEqual(TEXT("Nothing exhausts before exact N"), Deck->GetExhaustCount(), 0);

	TestTrue(TEXT("Clicking selected candidate toggles it off"), HUD->SelectCard(FirstId));
	TestEqual(TEXT("Toggle-off returns to zero selected"), ViewModel->GetPendingCardSelectionSelectedCount(), 0);
	TestTrue(TEXT("Gameplay still pending after toggle-off"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());

	TestTrue(TEXT("Second candidate can be selected first"), HUD->SelectCard(SecondId));
	TestEqual(TEXT("One selected before final click"), ViewModel->GetPendingCardSelectionSelectedCount(), 1);
	TestTrue(TEXT("Final distinct click reaches exact N without submitting"), HUD->SelectCard(FirstId));
	TestEqual(TEXT("Two RuntimeIds remain transiently selected"), ViewModel->GetPendingCardSelectionSelectedCount(), 2);
	TestTrue(TEXT("Exact N enables explicit confirmation"), ViewModel->CanConfirmPendingCardSelection());
	TestTrue(TEXT("Gameplay request remains pending until Confirm"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestEqual(TEXT("No cards exhaust before Confirm"), Deck->GetExhaustCount(), 0);

	TestTrue(TEXT("Explicit Confirm submits the selected RuntimeIds"), ViewModel->ConfirmPendingCardSelection());
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	TestEqual(TEXT("Local transient selection clears after Confirm"), ViewModel->GetPendingCardSelectionSelectedCount(), 0);
	TestFalse(TEXT("Gameplay request is no longer pending"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestEqual(TEXT("Exactly two cards exhaust after Confirm"), Deck->GetExhaustCount(), 2);
	const TArray<TObjectPtr<UCardInstance>>& ExhaustCards = Deck->GetExhaustCards();
	if (TestEqual(TEXT("Two exhausted cards recorded"), ExhaustCards.Num(), 2))
	{
		TestEqual(TEXT("HUD reverse click still commits candidate order [0]"), ExhaustCards[0]->GetRuntimeId(), FirstId);
		TestEqual(TEXT("HUD reverse click still commits candidate order [1]"), ExhaustCards[1]->GetRuntimeId(), SecondId);
	}
	TestFalse(TEXT("No resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
