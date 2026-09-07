#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DrawCardEffect.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/PresentationCardView.h"
#include "Presentation/PresentationTypes.h"
#include "Selection/SelectionResolver.h"
#include "Selection/SelectionTypes.h"
#include "Engine/World.h"

namespace CardExpansionWave1CPresentationTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		TArray<FPresentationResolutionEnvelope> Deliveries;

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
			Battle->bEnableCommittedPresentationRecording = true;
			Battle->OnPresentationResolutionReady.AddLambda(
				[this](const FPresentationResolutionEnvelope& Envelope)
				{
					Deliveries.Add(Envelope);
				});
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
			Card->Description = FText::FromString(TEXT("Wave 1C presentation test card."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* CreateBurningPactCard()
		{
			UCardData* Card = CreatePlainCard(TEXT("Wave1CPresentationBurningPact"));
			Card->DisplayName = FText::FromString(TEXT("Burning Pact"));
			Card->Description = FText::FromString(TEXT("Exhaust 1 card. Draw {Draw} cards."));
			Card->Rarity = ECardRarity::Uncommon;
			Card->CardColor = ECardColor::Red;
			Card->BaseCost = 1;
			Card->UpgradedCost = 1;

			USelectExhaustHandCardEffect* SelectExhaust = NewObject<USelectExhaustHandCardEffect>(Card);
			UDrawCardEffect* Draw = NewObject<UDrawCardEffect>(Card);
			Draw->DrawCount = 2;
			Draw->UpgradedDrawCount = 3;
			Card->Effects.Add(SelectExhaust);
			Card->Effects.Add(Draw);
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
			return IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn;
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

	int32 FindZoneRecord(
		const FPresentationResolutionEnvelope& Envelope,
		ECardZone FromZone,
		ECardZone ToZone,
		int32 RuntimeId,
		int32 StartIndex = 0)
	{
		for (int32 Index = FMath::Max(0, StartIndex); Index < Envelope.Records.Num(); ++Index)
		{
			const FPresentationRecord& Record = Envelope.Records[Index];
			if (Record.Type == EBattlePresentationRecordType::CardZoneChanged
				&& Record.CardZoneChanged.FromZone == FromZone
				&& Record.CardZoneChanged.ToZone == ToZone
				&& (RuntimeId == INDEX_NONE || Record.CardZoneChanged.Card.RuntimeId == RuntimeId))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
}

using namespace CardExpansionWave1CPresentationTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CBurningPactPresentationRecordOrderTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.BurningPact.PresentationRecordOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CBurningPactPresentationRecordOrderTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* BurningPact = Fixture.CreateBurningPactCard();
	UCardData* Chosen = Fixture.CreatePlainCard(TEXT("Wave1CPresentationChosen"));
	UCardData* Untouched = Fixture.CreatePlainCard(TEXT("Wave1CPresentationUntouched"));
	UCardData* FuelA = Fixture.CreatePlainCard(TEXT("Wave1CPresentationFuelA"));
	UCardData* FuelB = Fixture.CreatePlainCard(TEXT("Wave1CPresentationFuelB"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ BurningPact, Chosen, Untouched, FuelA, FuelB })))
	{
		return false;
	}

	Fixture.Deliveries.Reset();
	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindHandCard(TEXT("Wave1CPresentationBurningPact"));
	UCardInstance* ChosenCard = Fixture.FindHandCard(TEXT("Wave1CPresentationChosen"));
	UCardInstance* FuelCardA = Fixture.FindHandCard(TEXT("Wave1CPresentationFuelA"));
	UCardInstance* FuelCardB = Fixture.FindHandCard(TEXT("Wave1CPresentationFuelB"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard)
		|| !TestNotNull(TEXT("Chosen card exists"), ChosenCard)
		|| !TestNotNull(TEXT("Fuel A exists"), FuelCardA)
		|| !TestNotNull(TEXT("Fuel B exists"), FuelCardB)
		|| !TestTrue(TEXT("Fuel A setup discard commits"), Deck->TryDiscardCardCommit(FuelCardA).bCommitted)
		|| !TestTrue(TEXT("Fuel B setup discard commits"), Deck->TryDiscardCardCommit(FuelCardB).bCommitted))
	{
		return false;
	}

	TestTrue(
		TEXT("Burning Pact play accepted"),
		Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution());
	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	if (!TestNotNull(TEXT("Resolver exists"), Resolver))
	{
		return false;
	}
	TestTrue(TEXT("Selection waits before presentation envelope seals"), Resolver->HasPendingSelection());

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(ChosenCard);
	TestTrue(TEXT("Chosen card resolves"), Resolver->SubmitResult(Result));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	if (!TestEqual(TEXT("Exactly one Burning Pact resolution envelope delivered"), Fixture.Deliveries.Num(), 1))
	{
		return false;
	}
	const FPresentationResolutionEnvelope& Envelope = Fixture.Deliveries[0];

	const int32 CardPlayedIndex = Envelope.Records.IndexOfByPredicate(
		[PlayedCard](const FPresentationRecord& Record)
		{
			return Record.Type == EBattlePresentationRecordType::CardPlayed
				&& Record.CardPlayed.Card.RuntimeId == PlayedCard->GetRuntimeId();
		});
	const int32 ExhaustIndex = FindZoneRecord(
		Envelope,
		ECardZone::Hand,
		ECardZone::ExhaustPile,
		ChosenCard->GetRuntimeId());
	const int32 FirstDrawIndex = FindZoneRecord(
		Envelope,
		ECardZone::DrawPile,
		ECardZone::Hand,
		INDEX_NONE);
	const int32 SecondDrawIndex = FirstDrawIndex == INDEX_NONE
		? INDEX_NONE
		: FindZoneRecord(
			Envelope,
			ECardZone::DrawPile,
			ECardZone::Hand,
			INDEX_NONE,
			FirstDrawIndex + 1);
	const int32 FinishIndex = FindZoneRecord(
		Envelope,
		ECardZone::PlayArea,
		ECardZone::DiscardPile,
		PlayedCard->GetRuntimeId());

	TestTrue(TEXT("CardPlayed record exists"), CardPlayedIndex != INDEX_NONE);
	TestTrue(TEXT("Selected Hand card emits Hand->Exhaust record"), ExhaustIndex != INDEX_NONE);
	TestTrue(TEXT("First draw record exists"), FirstDrawIndex != INDEX_NONE);
	TestTrue(TEXT("Second draw record exists"), SecondDrawIndex != INDEX_NONE);
	TestTrue(TEXT("FinishCardPlay destination record exists"), FinishIndex != INDEX_NONE);
	TestTrue(
		TEXT("Presentation order is CardPlayed -> Exhaust -> Draw -> Draw -> Finish"),
		CardPlayedIndex < ExhaustIndex
			&& ExhaustIndex < FirstDrawIndex
			&& FirstDrawIndex < SecondDrawIndex
			&& SecondDrawIndex < FinishIndex);
	TestEqual(TEXT("Hand exhaust record preserves exact FromIndex"), Envelope.Records[ExhaustIndex].CardZoneChanged.FromIndex, 0);
	TestEqual(TEXT("Hand exhaust record targets current Exhaust index"), Envelope.Records[ExhaustIndex].CardZoneChanged.ToIndex, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CHandExhaustPresentationReducerTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.Presentation.HandExhaustReducer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CHandExhaustPresentationReducerTest::RunTest(const FString& Parameters)
{
	FPresentationCardSnapshot Card;
	Card.RuntimeId = 101;
	Card.CardId = TEXT("Wave1CReducerChosen");

	FPresentationStateSnapshot Baseline;
	Baseline.BattleId = 77;
	Baseline.StateRevision = 10;
	Baseline.BattleState = EBattleState::PlayerTurn;
	Baseline.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(Card));
	Baseline.ExhaustCount = 0;

	FPresentationRecord ExhaustRecord;
	ExhaustRecord.Type = EBattlePresentationRecordType::CardZoneChanged;
	ExhaustRecord.BattleId = 77;
	ExhaustRecord.ResolutionId = 11;
	ExhaustRecord.PresentationSequence = 1;
	ExhaustRecord.CardZoneChanged.Card = Card;
	ExhaustRecord.CardZoneChanged.FromZone = ECardZone::Hand;
	ExhaustRecord.CardZoneChanged.ToZone = ECardZone::ExhaustPile;
	ExhaustRecord.CardZoneChanged.FromIndex = 0;
	ExhaustRecord.CardZoneChanged.ToIndex = 0;

	FPresentationResolutionEnvelope Envelope;
	Envelope.BattleId = 77;
	Envelope.ResolutionId = 11;
	Envelope.Records.Add(ExhaustRecord);
	Envelope.FinalSnapshot = Baseline;
	Envelope.FinalSnapshot.StateRevision = 11;
	Envelope.FinalSnapshot.HandCards.Reset();
	Envelope.FinalSnapshot.ExhaustCount = 1;
	Envelope.FinalStateRevision = Envelope.FinalSnapshot.StateRevision;

	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(GetTransientPackage());
	if (!TestNotNull(TEXT("Controller created"), Controller))
	{
		return false;
	}

	FPresentationStateSnapshot Reduced;
	TestTrue(
		TEXT("Reducer accepts exact Hand->Exhaust transition"),
		Controller->ReduceEnvelopeForTesting(Baseline, Envelope, Reduced));
	TestEqual(TEXT("Reducer removes selected card from historical Hand"), Reduced.HandCards.Num(), 0);
	TestEqual(TEXT("Reducer increments ExhaustCount"), Reduced.ExhaustCount, 1);

	FPresentationResolutionEnvelope BadEnvelope = Envelope;
	BadEnvelope.Records[0].CardZoneChanged.ToIndex = 1;
	FPresentationStateSnapshot Ignored;
	TestFalse(
		TEXT("Reducer rejects a Hand->Exhaust record with a stale destination index"),
		Controller->ReduceEnvelopeForTesting(Baseline, BadEnvelope, Ignored));
	return true;
}

#endif
