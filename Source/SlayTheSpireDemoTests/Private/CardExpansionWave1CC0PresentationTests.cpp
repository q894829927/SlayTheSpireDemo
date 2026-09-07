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
#include "Presentation/BattlePresentationController.h"
#include "Presentation/PresentationCardView.h"
#include "Presentation/PresentationTypes.h"
#include "Selection/SelectionResolver.h"
#include "Engine/World.h"

namespace CardExpansionWave1CC0PresentationTest
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
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

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
			if (IsValid(World)) World->DestroyWorld(false);
		}

		UCardData* CreatePlainCard(const TCHAR* CardId)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(CardId);
			Card->DisplayName = FText::FromString(CardId);
			Card->Description = FText::FromString(TEXT("C0 presentation card."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* CreatePlayerSelectExhaustCard(const TCHAR* CardId, int32 Count)
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
			if (!IsValid(Battle) || Definitions.Num() == 0) return false;
			Battle->DebugStartingDeck.Reset();
			for (UCardData* Definition : Definitions) Battle->DebugStartingDeck.Add(Definition);
			Battle->OpeningHandDrawCount = Definitions.Num();
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			return IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn
				&& Battle->GetDeckRuntimeForTesting()->GetHandCount() == Definitions.Num();
		}

		UCardInstance* FindHandCard(FName CardId) const
		{
			UDeckRuntime* Deck = IsValid(Battle) ? Battle->GetDeckRuntimeForTesting() : nullptr;
			if (!IsValid(Deck)) return nullptr;
			for (const TObjectPtr<UCardInstance>& Card : Deck->GetHandCards())
			{
				if (IsValid(Card.Get()) && Card->GetCardId() == CardId) return Card.Get();
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

	FPresentationCardSnapshot MakeCardSnapshot(int32 RuntimeId, const TCHAR* CardId)
	{
		FPresentationCardSnapshot Card;
		Card.RuntimeId = RuntimeId;
		Card.CardId = FName(CardId);
		Card.DisplayName = FText::FromString(CardId);
		Card.Description = FText::FromString(TEXT("C0 reducer card."));
		return Card;
	}
}

using namespace CardExpansionWave1CC0PresentationTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0MultiExhaustPresentationRecordOrderTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Presentation.MultiExhaustRecordOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC0MultiExhaustPresentationRecordOrderTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* EffectCard = Fixture.CreatePlayerSelectExhaustCard(TEXT("C0PresentationMulti"), 3);
	UCardData* A = Fixture.CreatePlainCard(TEXT("C0PresentationA"));
	UCardData* B = Fixture.CreatePlainCard(TEXT("C0PresentationB"));
	UCardData* C = Fixture.CreatePlainCard(TEXT("C0PresentationC"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ EffectCard, A, B, C }))) return false;

	Fixture.Deliveries.Reset();
	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* Played = Fixture.FindHandCard(TEXT("C0PresentationMulti"));
	if (!TestNotNull(TEXT("Effect card exists"), Played)) return false;
	TestTrue(TEXT("Play accepted"), Fixture.Battle->RequestPlayCard(Played, nullptr).IsAcceptedForResolution());

	FPendingCardSelectionReadView View;
	if (!TestTrue(TEXT("Three-card read view available"), BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Fixture.Battle, View))) return false;
	if (!TestEqual(TEXT("Exactly three candidates"), View.CandidateRuntimeIds.Num(), 3)) return false;
	TestEqual(TEXT("RequiredCount is three"), View.RequiredCount, 3);

	TArray<int32> ReverseSubmission{ View.CandidateRuntimeIds[2], View.CandidateRuntimeIds[1], View.CandidateRuntimeIds[0] };
	TestTrue(TEXT("Reverse click-order membership submits"), BattleSelectionRequest::SubmitPendingCardSelection(Fixture.Battle, ReverseSubmission));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	if (!TestEqual(TEXT("Exactly one play-resolution envelope delivered"), Fixture.Deliveries.Num(), 1)) return false;
	const FPresentationResolutionEnvelope& Envelope = Fixture.Deliveries[0];
	const int32 CardPlayedIndex = Envelope.Records.IndexOfByPredicate(
		[Played](const FPresentationRecord& Record)
		{
			return Record.Type == EBattlePresentationRecordType::CardPlayed
				&& Record.CardPlayed.Card.RuntimeId == Played->GetRuntimeId();
		});
	const int32 FirstExhaust = FindZoneRecord(Envelope, ECardZone::Hand, ECardZone::ExhaustPile, View.CandidateRuntimeIds[0]);
	const int32 SecondExhaust = FindZoneRecord(Envelope, ECardZone::Hand, ECardZone::ExhaustPile, View.CandidateRuntimeIds[1]);
	const int32 ThirdExhaust = FindZoneRecord(Envelope, ECardZone::Hand, ECardZone::ExhaustPile, View.CandidateRuntimeIds[2]);
	const int32 FinishIndex = FindZoneRecord(Envelope, ECardZone::PlayArea, ECardZone::DiscardPile, Played->GetRuntimeId());

	if (!TestTrue(TEXT("CardPlayed record exists"), CardPlayedIndex != INDEX_NONE)
		|| !TestTrue(TEXT("First canonical Hand->Exhaust exists"), FirstExhaust != INDEX_NONE)
		|| !TestTrue(TEXT("Second canonical Hand->Exhaust exists"), SecondExhaust != INDEX_NONE)
		|| !TestTrue(TEXT("Third canonical Hand->Exhaust exists"), ThirdExhaust != INDEX_NONE)
		|| !TestTrue(TEXT("Played card finish record exists"), FinishIndex != INDEX_NONE))
	{
		return false;
	}

	TestTrue(TEXT("Committed presentation preserves canonical multi-exhaust order"),
		CardPlayedIndex < FirstExhaust
			&& FirstExhaust < SecondExhaust
			&& SecondExhaust < ThirdExhaust
			&& ThirdExhaust < FinishIndex);
	TestEqual(TEXT("First exhaust appends at Exhaust index 0"), Envelope.Records[FirstExhaust].CardZoneChanged.ToIndex, 0);
	TestEqual(TEXT("Second exhaust appends at Exhaust index 1"), Envelope.Records[SecondExhaust].CardZoneChanged.ToIndex, 1);
	TestEqual(TEXT("Third exhaust appends at Exhaust index 2"), Envelope.Records[ThirdExhaust].CardZoneChanged.ToIndex, 2);
	TestEqual(TEXT("Gameplay exhaust pile has three cards"), Deck->GetExhaustCount(), 3);
	TestEqual(TEXT("Final presentation snapshot has three exhausted cards"), Envelope.FinalSnapshot.ExhaustCount, 3);
	TestEqual(TEXT("Final presentation snapshot has no remaining candidate Hand cards"), Envelope.FinalSnapshot.HandCards.Num(), 0);
	TestFalse(TEXT("Multi-exhaust presentation resolution has no fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0MultiExhaustPresentationReducerTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Presentation.MultiExhaustReducer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC0MultiExhaustPresentationReducerTest::RunTest(const FString& Parameters)
{
	const FPresentationCardSnapshot CardA = MakeCardSnapshot(201, TEXT("C0ReducerA"));
	const FPresentationCardSnapshot CardB = MakeCardSnapshot(202, TEXT("C0ReducerB"));
	const FPresentationCardSnapshot CardC = MakeCardSnapshot(203, TEXT("C0ReducerC"));
	const FPresentationCardSnapshot Untouched = MakeCardSnapshot(204, TEXT("C0ReducerUntouched"));

	FPresentationStateSnapshot Baseline;
	Baseline.BattleId = 88;
	Baseline.StateRevision = 20;
	Baseline.BattleState = EBattleState::PlayerTurn;
	Baseline.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(CardA));
	Baseline.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(CardB));
	Baseline.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(CardC));
	Baseline.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(Untouched));
	Baseline.ExhaustCount = 0;

	FPresentationResolutionEnvelope Envelope;
	Envelope.BattleId = 88;
	Envelope.ResolutionId = 21;

	auto AddExhaustRecord = [&Envelope](const FPresentationCardSnapshot& Card, int32 Sequence, int32 FromIndex, int32 ToIndex)
	{
		FPresentationRecord Record;
		Record.Type = EBattlePresentationRecordType::CardZoneChanged;
		Record.BattleId = 88;
		Record.ResolutionId = 21;
		Record.PresentationSequence = Sequence;
		Record.CardZoneChanged.Card = Card;
		Record.CardZoneChanged.FromZone = ECardZone::Hand;
		Record.CardZoneChanged.ToZone = ECardZone::ExhaustPile;
		Record.CardZoneChanged.FromIndex = FromIndex;
		Record.CardZoneChanged.ToIndex = ToIndex;
		Envelope.Records.Add(Record);
	};

	AddExhaustRecord(CardA, 1, 0, 0);
	AddExhaustRecord(CardB, 2, 0, 1);
	AddExhaustRecord(CardC, 3, 0, 2);

	Envelope.FinalSnapshot = Baseline;
	Envelope.FinalSnapshot.StateRevision = 21;
	Envelope.FinalSnapshot.HandCards.Reset();
	Envelope.FinalSnapshot.HandCards.Add(PresentationCardView::MakePresentationOnlyCardView(Untouched));
	Envelope.FinalSnapshot.ExhaustCount = 3;
	Envelope.FinalStateRevision = 21;

	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(GetTransientPackage());
	if (!TestNotNull(TEXT("Controller created"), Controller)) return false;

	FPresentationStateSnapshot Reduced;
	TestTrue(TEXT("Reducer accepts three sequential exact Hand->Exhaust records"), Controller->ReduceEnvelopeForTesting(Baseline, Envelope, Reduced));
	if (TestEqual(TEXT("Only untouched card remains in historical Hand"), Reduced.HandCards.Num(), 1))
	{
		TestEqual(TEXT("Untouched card identity remains stable"), Reduced.HandCards[0].RuntimeId, Untouched.RuntimeId);
	}
	TestEqual(TEXT("Reducer increments ExhaustCount once per card"), Reduced.ExhaustCount, 3);

	FPresentationResolutionEnvelope BadEnvelope = Envelope;
	BadEnvelope.Records[1].CardZoneChanged.FromIndex = 1;
	FPresentationStateSnapshot Ignored;
	TestFalse(TEXT("Reducer rejects stale second FromIndex after first removal"), Controller->ReduceEnvelopeForTesting(Baseline, BadEnvelope, Ignored));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
