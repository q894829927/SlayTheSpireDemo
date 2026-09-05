#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleTextResolver.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/GainEnergyCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Events/BattleEvent.h"
#include "Events/BattleEventDispatcher.h"
#include "Presentation/BattlePresentationRecorder.h"
#include "Presentation/PresentationTypes.h"
#include "Engine/World.h"

namespace CardExpansionWave1ATest
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
			Battle->bEnableCommittedPresentationRecording = true;
			Battle->OnPresentationResolutionReady.AddLambda(
				[this](const FPresentationResolutionEnvelope& Envelope)
				{
					Deliveries.Add(Envelope);
				}
			);
		}

		~FFixture()
		{
			UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		UCardData* CreateCard(
			const TCHAR* CardId,
			ECardDestination Destination,
			const FText& Description = FText::FromString(TEXT("Test card."))
		)
		{
			UCardData* Card = NewObject<UCardData>(World);
			Card->CardId = FName(CardId);
			Card->DisplayName = FText::FromString(CardId);
			Card->Description = Description;
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = Destination;
			return Card;
		}

		UGainEnergyCardEffect* AddGainEnergyEffect(UCardData* Card, int32 BaseAmount, int32 UpgradedAmount)
		{
			UGainEnergyCardEffect* Effect = NewObject<UGainEnergyCardEffect>(Card);
			Effect->DescriptionArgumentName = TEXT("Energy");
			Effect->BaseAmount = BaseAmount;
			Effect->UpgradedAmount = UpgradedAmount;
			Card->Effects.Add(Effect);
			return Effect;
		}

		bool Start(UCardData* Definition)
		{
			if (!IsValid(Battle) || !IsValid(Definition))
			{
				return false;
			}

			Battle->DebugStartingDeck.Reset();
			Battle->DebugStartingDeck.Add(Definition);
			Battle->OpeningHandDrawCount = 1;
			Battle->StartBattle();
			Flush();
			return IsValid(Battle->GetActionQueueForTesting())
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn
				&& Battle->GetDeckRuntimeForTesting()->GetHandCount() == 1;
		}

		void Flush() const
		{
			if (IsValid(Battle))
			{
				Battle->FlushScheduledReadStateReadyForTesting();
			}
		}

		const FPresentationResolutionEnvelope* LastDelivery() const
		{
			return Deliveries.Num() > 0 ? &Deliveries.Last() : nullptr;
		}
	};

	const FPresentationRecord* FindFinishZoneRecord(
		const FPresentationResolutionEnvelope& Envelope,
		ECardZone ToZone
	)
	{
		return Envelope.Records.FindByPredicate(
			[ToZone](const FPresentationRecord& Record)
			{
				return Record.Type == EBattlePresentationRecordType::CardZoneChanged
					&& Record.CardZoneChanged.ToZone == ToZone;
			}
		);
	}
}

using namespace CardExpansionWave1ATest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1ACommittedExhaustEventTest,
	"SlayTheSpireDemo.CardExpansion.Wave1A.ExhaustFact.CommittedEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1ACommittedExhaustEventTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* Definition = Fixture.CreateCard(TEXT("Wave1AExhaust"), ECardDestination::Exhaust);
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start(Definition)))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* Card = Deck->GetFirstHandCard();
	if (!TestNotNull(TEXT("Hand card exists"), Card))
	{
		return false;
	}

	int32 EventCount = 0;
	UCardInstance* EventCard = nullptr;
	int32 EventRuntimeId = INDEX_NONE;
	FName EventCardId = NAME_None;
	ECardZone EventFromZone = ECardZone::DrawPile;
	ECardZone EventToZone = ECardZone::Hand;
	bool bObservedCommittedExhaust = false;
	int32 ActivePresentationRecordsAtEvent = INDEX_NONE;

	UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
	UBattleEventDispatcher::OnEventDispatchedForTesting.AddLambda(
		[&](const FBattleEvent& Event)
		{
			const FCardExhaustedEvent* Payload = Event.TryGet<FCardExhaustedEvent>();
			if (Payload == nullptr)
			{
				return;
			}

			++EventCount;
			EventCard = Payload->Card;
			EventRuntimeId = Payload->CardRuntimeId;
			EventCardId = Payload->CardId;
			EventFromZone = Payload->FromZone;
			EventToZone = Payload->ToZone;
			bObservedCommittedExhaust = Deck->GetExhaustCount() == 1 && !Deck->IsCardInPlayArea(Card);
			if (UBattlePresentationRecorder* Recorder = Fixture.Battle->GetPresentationRecorderForTesting())
			{
				ActivePresentationRecordsAtEvent = Recorder->GetActiveRecordCountForTesting();
			}
		}
	);

	TestTrue(
		TEXT("Self-exhaust play accepted"),
		Fixture.Battle->RequestPlayCard(Card, nullptr).IsAcceptedForResolution()
	);
	Fixture.Flush();
	UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();

	TestEqual(TEXT("Exactly one CardExhausted event"), EventCount, 1);
	TestTrue(TEXT("Event keeps exact runtime Card pointer"), EventCard == Card);
	TestEqual(TEXT("Event RuntimeId matches exact Card"), EventRuntimeId, Card->GetRuntimeId());
	TestEqual(TEXT("Event CardId matches exact Card"), EventCardId, Card->GetCardId());
	TestEqual(TEXT("Event FromZone is committed PlayArea"), EventFromZone, ECardZone::PlayArea);
	TestEqual(TEXT("Event ToZone is committed ExhaustPile"), EventToZone, ECardZone::ExhaustPile);
	TestTrue(TEXT("Dispatch observes already-committed Exhaust state"), bObservedCommittedExhaust);
	TestTrue(TEXT("Committed CardZoneChanged record exists before event dispatch"), ActivePresentationRecordsAtEvent > 0);
	TestEqual(TEXT("Final Exhaust count"), Deck->GetExhaustCount(), 1);
	TestFalse(TEXT("Zero-listener exhaust does not fault resolution"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());

	const FPresentationResolutionEnvelope* Envelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Self-exhaust presentation envelope exists"), Envelope))
	{
		return false;
	}
	const FPresentationRecord* ZoneRecord = FindFinishZoneRecord(*Envelope, ECardZone::ExhaustPile);
	if (!TestNotNull(TEXT("Self-exhaust finish zone record exists"), ZoneRecord))
	{
		return false;
	}
	TestEqual(TEXT("Event/record RuntimeId agree"), EventRuntimeId, ZoneRecord->CardZoneChanged.Card.RuntimeId);
	TestEqual(TEXT("Event/record CardId agree"), EventCardId, ZoneRecord->CardZoneChanged.Card.CardId);
	TestEqual(TEXT("Event/record FromZone agree"), EventFromZone, ZoneRecord->CardZoneChanged.FromZone);
	TestEqual(TEXT("Event/record ToZone agree"), EventToZone, ZoneRecord->CardZoneChanged.ToZone);

	const int32 CountBeforeRejectedReplay = EventCount;
	TestFalse(
		TEXT("Already exhausted card cannot be played again"),
		Fixture.Battle->RequestPlayCard(Card, nullptr).IsAcceptedForResolution()
	);
	Fixture.Flush();
	TestEqual(TEXT("Rejected replay emits no duplicate CardExhausted"), EventCount, CountBeforeRejectedReplay);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1ANonExhaustDestinationsTest,
	"SlayTheSpireDemo.CardExpansion.Wave1A.ExhaustFact.NonExhaustDestinations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1ANonExhaustDestinationsTest::RunTest(const FString& Parameters)
{
	for (const ECardDestination Destination : { ECardDestination::Discard, ECardDestination::Removed })
	{
		FFixture Fixture;
		UCardData* Definition = Fixture.CreateCard(
			Destination == ECardDestination::Discard ? TEXT("Wave1ADiscard") : TEXT("Wave1ARemoved"),
			Destination
		);
		if (!TestTrue(TEXT("Non-exhaust fixture starts"), Fixture.Start(Definition)))
		{
			return false;
		}

		int32 ExhaustEventCount = 0;
		UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
		UBattleEventDispatcher::OnEventDispatchedForTesting.AddLambda(
			[&](const FBattleEvent& Event)
			{
				if (Event.TryGet<FCardExhaustedEvent>() != nullptr)
				{
					++ExhaustEventCount;
				}
			}
		);

		UCardInstance* Card = Fixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
		if (!TestNotNull(TEXT("Non-exhaust card exists"), Card))
		{
			return false;
		}
		TestTrue(TEXT("Non-exhaust play accepted"), Fixture.Battle->RequestPlayCard(Card, nullptr).IsAcceptedForResolution());
		Fixture.Flush();
		UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
		TestEqual(TEXT("Non-exhaust destination emits zero CardExhausted"), ExhaustEventCount, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1AGainEnergyCardEffectTest,
	"SlayTheSpireDemo.CardExpansion.Wave1A.GainEnergyCardEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1AGainEnergyCardEffectTest::RunTest(const FString& Parameters)
{
	{
		FFixture Fixture;
		UCardData* Definition = Fixture.CreateCard(
			TEXT("Wave1AEnergyBase"),
			ECardDestination::Discard,
			FText::FromString(TEXT("Gain {Energy} Energy."))
		);
		Fixture.AddGainEnergyEffect(Definition, 2, 4);
		TArray<FText> ValidationErrors;
		TestTrue(TEXT("GainEnergy definition validates"), FBattleTextResolver::ValidateCardDefinition(Definition, ValidationErrors));
		if (!TestTrue(TEXT("Base energy fixture starts"), Fixture.Start(Definition)))
		{
			return false;
		}

		UCardInstance* Card = Fixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
		if (!TestNotNull(TEXT("Base energy card exists"), Card))
		{
			return false;
		}
		TestTrue(TEXT("Base preview uses BaseAmount"), FBattleTextResolver::ResolveCardDescription(Card, Fixture.Player).ToString().Contains(TEXT("2")));
		const int32 EnergyBefore = Fixture.Battle->Energy;
		TestTrue(TEXT("Base energy card play accepted"), Fixture.Battle->RequestPlayCard(Card, nullptr).IsAcceptedForResolution());
		Fixture.Flush();
		TestEqual(TEXT("Base card gains BaseAmount energy"), Fixture.Battle->Energy, EnergyBefore + 2);
	}

	{
		FFixture Fixture;
		UCardData* Definition = Fixture.CreateCard(
			TEXT("Wave1AEnergyUpgraded"),
			ECardDestination::Discard,
			FText::FromString(TEXT("Gain {Energy} Energy."))
		);
		Fixture.AddGainEnergyEffect(Definition, 2, 4);
		if (!TestTrue(TEXT("Upgraded energy fixture starts"), Fixture.Start(Definition)))
		{
			return false;
		}

		UCardInstance* Card = Fixture.Battle->GetDeckRuntimeForTesting()->GetFirstHandCard();
		if (!TestNotNull(TEXT("Upgraded energy card exists"), Card))
		{
			return false;
		}
		Fixture.Battle->TestUpgradeFirstHandCard();
		Fixture.Flush();
		TestTrue(TEXT("Card upgraded through real upgrade action"), Card->IsUpgraded());
		TestTrue(TEXT("Upgraded preview uses UpgradedAmount"), FBattleTextResolver::ResolveCardDescription(Card, Fixture.Player).ToString().Contains(TEXT("4")));
		const int32 EnergyBefore = Fixture.Battle->Energy;
		TestTrue(TEXT("Upgraded energy card play accepted"), Fixture.Battle->RequestPlayCard(Card, nullptr).IsAcceptedForResolution());
		Fixture.Flush();
		TestEqual(TEXT("Upgraded card gains UpgradedAmount energy"), Fixture.Battle->Energy, EnergyBefore + 4);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
