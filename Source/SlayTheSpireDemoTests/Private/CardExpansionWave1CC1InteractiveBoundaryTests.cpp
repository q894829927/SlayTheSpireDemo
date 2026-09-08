#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleManager.h"
#include "Battle/BattleSelectionRequest.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DrawCardEffect.h"
#include "Cards/Effects/SelectHandCardToDrawPileTopEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Presentation/PresentationTypes.h"
#include "Selection/SelectionResolver.h"
#include "UI/BattleHUDViewModel.h"
#include "Engine/World.h"

namespace CardExpansionWave1CC1InteractiveBoundaryTest
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
			Battle->OpeningHandDrawCount = 2;
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

		UCardData* CreateWarcryStyleCard()
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = TEXT("C1InteractiveWarcry");
			Card->DisplayName = FText::FromString(TEXT("C1 Interactive Warcry"));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Exhaust;

			UDrawCardEffect* Draw = NewObject<UDrawCardEffect>(Card);
			Draw->DrawCount = 1;
			Draw->UpgradedDrawCount = 1;
			Card->Effects.Add(Draw);

			USelectHandCardToDrawPileTopEffect* PutOnTop =
				NewObject<USelectHandCardToDrawPileTopEffect>(Card);
			PutOnTop->BaseSelectionCount = 1;
			PutOnTop->UpgradedSelectionCount = 1;
			Card->Effects.Add(PutOnTop);
			return Card;
		}

		bool Start(UCardData* Definition)
		{
			if (!IsValid(Battle) || !IsValid(Definition)) return false;
			Battle->DebugStartingDeck = { Definition, Definition, Definition };
			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			Deliveries.Reset();
			return Battle->BattleState == EBattleState::PlayerTurn
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->GetDeckRuntimeForTesting()->GetHandCount() == 2
				&& Battle->GetDeckRuntimeForTesting()->GetDrawCount() == 1;
		}
	};

	int32 FindZoneRecord(
		const FPresentationResolutionEnvelope& Envelope,
		ECardZone FromZone,
		ECardZone ToZone,
		int32 RuntimeId = INDEX_NONE)
	{
		return Envelope.Records.IndexOfByPredicate(
			[FromZone, ToZone, RuntimeId](const FPresentationRecord& Record)
			{
				return Record.Type == EBattlePresentationRecordType::CardZoneChanged
					&& Record.CardZoneChanged.FromZone == FromZone
					&& Record.CardZoneChanged.ToZone == ToZone
					&& (RuntimeId == INDEX_NONE || Record.CardZoneChanged.Card.RuntimeId == RuntimeId);
			});
	}
}

using namespace CardExpansionWave1CC1InteractiveBoundaryTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC1DrawBeforeSelectionPresentationBoundaryTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop.Presentation.DrawBeforeSelectionBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWave1CC1DrawBeforeSelectionPresentationBoundaryTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* Definition = Fixture.CreateWarcryStyleCard();
	if (!TestTrue(TEXT("Fixture starts with two Hand cards and one DrawPile card"), Fixture.Start(Definition)))
	{
		return false;
	}

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	if (!TestTrue(TEXT("Presentation-owned ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true)))
	{
		return false;
	}
	const int64 DisplayRevisionBeforePlay = ViewModel->StateRevision;

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* Played = Deck->GetHandCards()[0].Get();
	if (!TestNotNull(TEXT("Played runtime card exists"), Played)) return false;

	TestTrue(
		TEXT("Warcry-style play is accepted"),
		Fixture.Battle->RequestPlayCard(Played, nullptr).IsAcceptedForResolution());

	// Gameplay has already committed Draw and is waiting for the selection.
	FPendingCardSelectionReadView GameplaySelection;
	if (!TestTrue(
		TEXT("Gameplay selection is pending immediately after Draw commit"),
		BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Fixture.Battle, GameplaySelection)))
	{
		return false;
	}
	TestEqual(TEXT("Selection requires exactly one current Hand card"), GameplaySelection.RequiredCount, 1);
	TestEqual(TEXT("Current Hand has original survivor plus newly drawn card"), GameplaySelection.CandidateRuntimeIds.Num(), 2);

	FPresentationStateSnapshot BoundarySnapshot;
	if (!TestTrue(
		TEXT("Interactive boundary freezes a new presentation baseline"),
		Fixture.Battle->TryGetLatestFrozenPresentationBaseline(BoundarySnapshot)))
	{
		return false;
	}
	TestTrue(TEXT("Interactive boundary advances the frozen revision"), BoundarySnapshot.StateRevision > DisplayRevisionBeforePlay);
	TestFalse(
		TEXT("Presentation-owned ViewModel hides pending selection before Draw playback catches up"),
		ViewModel->HasPendingCardSelection());

	Fixture.Battle->FlushScheduledReadStateReadyForTesting();
	if (!TestEqual(TEXT("Exactly one pre-selection Presentation envelope is delivered"), Fixture.Deliveries.Num(), 1))
	{
		return false;
	}

	const FPresentationResolutionEnvelope& PreSelectionEnvelope = Fixture.Deliveries[0];
	const int32 CardPlayedIndex = PreSelectionEnvelope.Records.IndexOfByPredicate(
		[Played](const FPresentationRecord& Record)
		{
			return Record.Type == EBattlePresentationRecordType::CardPlayed
				&& Record.CardPlayed.Card.RuntimeId == Played->GetRuntimeId();
		});
	const int32 DrawIndex = FindZoneRecord(PreSelectionEnvelope, ECardZone::DrawPile, ECardZone::Hand);
	const int32 PrematureTopMove = FindZoneRecord(PreSelectionEnvelope, ECardZone::Hand, ECardZone::DrawPile);
	TestTrue(TEXT("Pre-selection envelope contains CardPlayed"), CardPlayedIndex != INDEX_NONE);
	TestTrue(TEXT("Pre-selection envelope contains the committed Draw"), DrawIndex != INDEX_NONE);
	TestTrue(TEXT("CardPlayed is recorded before Draw"), CardPlayedIndex < DrawIndex);
	TestEqual(TEXT("No Hand->DrawPileTop record exists before player selection"), PrematureTopMove, INDEX_NONE);
	if (DrawIndex == INDEX_NONE) return false;

	const int32 NewlyDrawnRuntimeId = PreSelectionEnvelope.Records[DrawIndex].CardZoneChanged.Card.RuntimeId;
	TestTrue(
		TEXT("Newly drawn exact RuntimeId is a selection candidate"),
		GameplaySelection.CandidateRuntimeIds.Contains(NewlyDrawnRuntimeId));

	// Simulate Controller catch-up to the exact sealed boundary. The Gameplay
	// selection was pending all along; only its UI exposure changes here.
	ViewModel->ApplyPresentationSnapshot(PreSelectionEnvelope.FinalSnapshot, true);
	TestTrue(
		TEXT("Pending selection becomes visible only after Draw boundary is displayed"),
		ViewModel->HasPendingCardSelection());
	TestTrue(
		TEXT("Newly drawn card becomes selectable after catch-up"),
		ViewModel->IsPendingCardSelectionCandidate(NewlyDrawnRuntimeId));

	Fixture.Deliveries.Reset();
	TestTrue(
		TEXT("Selecting the newly drawn card resolves the exact pending choice"),
		ViewModel->SubmitPendingCardSelectionByRuntimeId(NewlyDrawnRuntimeId));
	Fixture.Battle->FlushScheduledReadStateReadyForTesting();

	if (!TestEqual(TEXT("Exactly one post-selection Presentation envelope is delivered"), Fixture.Deliveries.Num(), 1))
	{
		return false;
	}
	const FPresentationResolutionEnvelope& PostSelectionEnvelope = Fixture.Deliveries[0];
	TestTrue(
		TEXT("Post-selection envelope moves the chosen exact card Hand->DrawPileTop"),
		FindZoneRecord(PostSelectionEnvelope, ECardZone::Hand, ECardZone::DrawPile, NewlyDrawnRuntimeId) != INDEX_NONE);
	TestTrue(
		TEXT("Post-selection envelope performs played-card cleanup after the choice"),
		FindZoneRecord(PostSelectionEnvelope, ECardZone::PlayArea, ECardZone::ExhaustPile, Played->GetRuntimeId()) != INDEX_NONE);
	TestFalse(TEXT("Gameplay queue finishes without resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
