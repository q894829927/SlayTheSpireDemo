#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/UpgradeCardAction.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DrawCardEffect.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Events/BattleEventDispatcher.h"
#include "Selection/SelectionResolver.h"
#include "Selection/SelectionTypes.h"
#include "UI/BattleHUDViewModel.h"
#include "UI/BattleHUDWidget.h"
#include "Engine/World.h"

namespace CardExpansionWave1CSelectExhaustTest
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
			Battle->bEnableCommittedPresentationRecording = true;
		}

		~FFixture()
		{
			UBattleEventDispatcher::OnEventDispatchedForTesting.Clear();
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
			Card->Description = FText::FromString(TEXT("Plain hand card."));
			Card->BaseCost = 0;
			Card->UpgradedCost = 0;
			Card->CardType = ECardType::Skill;
			Card->TargetType = ECardTargetType::None;
			Card->DefaultDestination = ECardDestination::Discard;
			return Card;
		}

		UCardData* CreateSelectExhaustCard(const TCHAR* CardId)
		{
			UCardData* Card = CreatePlainCard(CardId);
			USelectExhaustHandCardEffect* Effect = NewObject<USelectExhaustHandCardEffect>(Card);
			Card->Effects.Add(Effect);
			return Card;
		}

		UCardData* CreateBurningPactCard(const TCHAR* CardId = TEXT("Wave1CBurningPact"))
		{
			UCardData* Card = CreatePlainCard(CardId);
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
			Flush();
			return IsValid(Battle->GetActionQueueForTesting())
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn
				&& Battle->GetDeckRuntimeForTesting()->GetHandCount() == Definitions.Num();
		}

		void Flush() const
		{
			if (IsValid(Battle))
			{
				Battle->FlushScheduledReadStateReadyForTesting();
			}
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

		bool DiscardForDrawFuel(UDeckRuntime* Deck, UCardInstance* Card) const
		{
			return IsValid(Deck)
				&& IsValid(Card)
				&& Deck->TryDiscardCardCommit(Card).bCommitted;
		}

		bool UpgradeCard(UCardInstance* Card) const
		{
			UBattleActionQueue* Queue = IsValid(Battle) ? Battle->GetActionQueueForTesting() : nullptr;
			if (!IsValid(Queue) || !IsValid(Card))
			{
				return false;
			}

			UUpgradeCardAction* Upgrade = NewObject<UUpgradeCardAction>(Queue);
			Upgrade->Initialize(Card);
			if (!Queue->AddToBack(Upgrade) || !Queue->StartProcessing())
			{
				return false;
			}
			return Card->IsUpgraded();
		}

		bool ZoneContains(
			const TArray<TObjectPtr<UCardInstance>>& Zone,
			const UCardInstance* Card
		) const
		{
			return Zone.ContainsByPredicate(
				[Card](const TObjectPtr<UCardInstance>& Item)
				{
					return Item.Get() == Card;
				}
			);
		}
	};
}

using namespace CardExpansionWave1CSelectExhaustTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectExhaustResolveExhaustsChosenTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.SelectExhaust.ExhaustsChosenCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectExhaustResolveExhaustsChosenTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* SelectExhaust = Fixture.CreateSelectExhaustCard(TEXT("Wave1CBurn"));
	UCardData* OtherA = Fixture.CreatePlainCard(TEXT("Wave1COtherA"));
	UCardData* OtherB = Fixture.CreatePlainCard(TEXT("Wave1COtherB"));
	if (!TestTrue(TEXT("Fixture starts with three Hand cards"), Fixture.Start({ SelectExhaust, OtherA, OtherB })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CBurn"));
	UCardInstance* ChosenCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1COtherA"));
	UCardInstance* UntouchedCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1COtherB"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard)
		|| !TestNotNull(TEXT("Chosen card exists"), ChosenCard)
		|| !TestNotNull(TEXT("Untouched card exists"), UntouchedCard))
	{
		return false;
	}

	TestTrue(
		TEXT("Select-exhaust play accepted"),
		Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution()
	);

	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	if (!TestNotNull(TEXT("Resolver exists"), Resolver))
	{
		return false;
	}
	TestTrue(TEXT("Selection is pending after play"), Resolver->HasPendingSelection());

	const FSelectionRequest* Request = Resolver->GetPendingRequest();
	if (!TestNotNull(TEXT("Pending request exposed"), Request))
	{
		return false;
	}
	TestEqual(TEXT("Exactly two candidates (not the played card)"), Request->Candidates.Num(), 2);
	TestTrue(
		TEXT("Select-exhaust is a mandatory selection"),
		Request->CancelPolicy == ESelectionCancelPolicy::Forbidden
	);

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(ChosenCard);
	TestTrue(TEXT("Selection submits successfully"), Resolver->SubmitResult(Result));
	Fixture.Flush();

	TestEqual(TEXT("Exactly one card enters ExhaustPile"), Deck->GetExhaustCount(), 1);
	TestFalse(TEXT("Chosen card is no longer in Hand"), Deck->IsCardInHand(ChosenCard));
	TestTrue(TEXT("Untouched card remains in Hand"), Deck->IsCardInHand(UntouchedCard));
	TestFalse(TEXT("No resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectExhaustSkipsWhenNoCandidateTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.SelectExhaust.SkipsWhenNoCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectExhaustSkipsWhenNoCandidateTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* SelectExhaust = Fixture.CreateSelectExhaustCard(TEXT("Wave1CBurnOnly"));
	if (!TestTrue(TEXT("Fixture starts with a single Hand card"), Fixture.Start({ SelectExhaust })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CBurnOnly"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard))
	{
		return false;
	}

	TestTrue(
		TEXT("Select-exhaust play accepted"),
		Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution()
	);

	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	TestNotNull(TEXT("Resolver exists"), Resolver);
	TestFalse(TEXT("No selection pending when no other card exists"), Resolver->HasPendingSelection());
	TestEqual(TEXT("Nothing is exhausted"), Deck->GetExhaustCount(), 0);
	TestFalse(TEXT("No resolution fault"), Fixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CSelectExhaustMandatoryCancelRejectedTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.SelectExhaust.MandatoryCancelRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CSelectExhaustMandatoryCancelRejectedTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* SelectExhaust = Fixture.CreateSelectExhaustCard(TEXT("Wave1CMandatory"));
	UCardData* Other = Fixture.CreatePlainCard(TEXT("Wave1CMandatoryOther"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ SelectExhaust, Other })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CMandatory"));
	UCardInstance* OtherCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CMandatoryOther"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard)
		|| !TestNotNull(TEXT("Other card exists"), OtherCard))
	{
		return false;
	}

	TestTrue(
		TEXT("Play accepted"),
		Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution()
	);

	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	if (!TestNotNull(TEXT("Resolver exists"), Resolver)
		|| !TestNotNull(TEXT("Queue exists"), Queue))
	{
		return false;
	}

	TestTrue(TEXT("Mandatory selection is pending"), Resolver->HasPendingSelection());
	TestFalse(TEXT("Mandatory selection reports non-cancellable"), Resolver->CanCancelPendingSelection());
	TestFalse(TEXT("SubmitCancel is rejected"), Resolver->SubmitCancel());
	TestTrue(TEXT("Rejected cancel keeps selection pending"), Resolver->HasPendingSelection());
	TestTrue(TEXT("Rejected cancel keeps queue busy"), Queue->IsBusy());
	TestEqual(TEXT("Rejected cancel does not exhaust"), Deck->GetExhaustCount(), 0);

	FSelectionResult Cancelled;
	Cancelled.Status = ESelectionStatus::Cancelled;
	TestFalse(TEXT("Cancelled SubmitResult is also rejected"), Resolver->SubmitResult(Cancelled));
	TestTrue(TEXT("Cancelled result still leaves selection pending"), Resolver->HasPendingSelection());

	FSelectionResult Resolved;
	Resolved.Status = ESelectionStatus::Resolved;
	Resolved.SelectedObjects.Add(OtherCard);
	TestTrue(TEXT("Valid mandatory selection resolves"), Resolver->SubmitResult(Resolved));
	Fixture.Flush();
	TestEqual(TEXT("Chosen card exhausts after valid resolve"), Deck->GetExhaustCount(), 1);
	TestFalse(TEXT("Queue is released after valid resolve"), Queue->IsBusy());
	TestFalse(TEXT("No resolution fault"), Queue->IsResolutionFaulted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CBurningPactBaseDrawTwoTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.BurningPact.BaseExhaustThenDraw2",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CBurningPactBaseDrawTwoTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* BurningPact = Fixture.CreateBurningPactCard();
	UCardData* Chosen = Fixture.CreatePlainCard(TEXT("BPChosen"));
	UCardData* Untouched = Fixture.CreatePlainCard(TEXT("BPUntouched"));
	UCardData* FuelA = Fixture.CreatePlainCard(TEXT("BPFuelA"));
	UCardData* FuelB = Fixture.CreatePlainCard(TEXT("BPFuelB"));
	if (!TestTrue(TEXT("Fixture starts with five cards"), Fixture.Start({ BurningPact, Chosen, Untouched, FuelA, FuelB })))
	{
		return false;
	}

	TestEqual(TEXT("Burning Pact has two authored Effects"), BurningPact->Effects.Num(), 2);
	TestNotNull(TEXT("Effect[0] is SelectExhaust"), Cast<USelectExhaustHandCardEffect>(BurningPact->Effects[0].Get()));
	UDrawCardEffect* DrawEffect = Cast<UDrawCardEffect>(BurningPact->Effects[1].Get());
	if (!TestNotNull(TEXT("Effect[1] is Draw"), DrawEffect))
	{
		return false;
	}
	TestEqual(TEXT("Base draw is 2"), DrawEffect->DrawCount, 2);
	TestEqual(TEXT("Upgraded draw is 3"), DrawEffect->UpgradedDrawCount, 3);
	TestEqual(TEXT("Base cost is 1"), BurningPact->BaseCost, 1);
	TestEqual(TEXT("Upgraded cost remains 1"), BurningPact->UpgradedCost, 1);
	TestTrue(TEXT("Destination is Discard"), BurningPact->DefaultDestination == ECardDestination::Discard);
	TestEqual(
		TEXT("Description shape is authored"),
		BurningPact->Description.ToString(),
		FString(TEXT("Exhaust 1 card. Draw {Draw} cards."))
	);

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CBurningPact"));
	UCardInstance* ChosenCard = Fixture.FindInstanceByCardId(Deck, TEXT("BPChosen"));
	UCardInstance* UntouchedCard = Fixture.FindInstanceByCardId(Deck, TEXT("BPUntouched"));
	UCardInstance* FuelCardA = Fixture.FindInstanceByCardId(Deck, TEXT("BPFuelA"));
	UCardInstance* FuelCardB = Fixture.FindInstanceByCardId(Deck, TEXT("BPFuelB"));
	if (!TestNotNull(TEXT("Burning Pact instance exists"), PlayedCard)
		|| !TestNotNull(TEXT("Chosen card exists"), ChosenCard)
		|| !TestNotNull(TEXT("Untouched card exists"), UntouchedCard)
		|| !TestNotNull(TEXT("Fuel A exists"), FuelCardA)
		|| !TestNotNull(TEXT("Fuel B exists"), FuelCardB))
	{
		return false;
	}

	if (!TestTrue(TEXT("Fuel A moved to Discard"), Fixture.DiscardForDrawFuel(Deck, FuelCardA))
		|| !TestTrue(TEXT("Fuel B moved to Discard"), Fixture.DiscardForDrawFuel(Deck, FuelCardB)))
	{
		return false;
	}
	TestEqual(TEXT("Two draw-fuel cards are in Discard"), Deck->GetDiscardCount(), 2);
	TestEqual(TEXT("Three cards remain in Hand before play"), Deck->GetHandCount(), 3);

	TestTrue(
		TEXT("Burning Pact play accepted"),
		Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution()
	);
	TestEqual(TEXT("Burning Pact spends exactly 1 Energy"), Fixture.Battle->Energy, 2);

	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	if (!TestNotNull(TEXT("Resolver exists"), Resolver))
	{
		return false;
	}
	TestTrue(TEXT("Selection is pending before Draw"), Resolver->HasPendingSelection());
	TestEqual(TEXT("Draw has not run while selection waits"), Deck->GetDiscardCount(), 2);
	TestEqual(TEXT("Nothing exhausted before selection"), Deck->GetExhaustCount(), 0);
	TestEqual(TEXT("Only the two candidate cards remain in Hand while waiting"), Deck->GetHandCount(), 2);

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(ChosenCard);
	TestTrue(TEXT("Chosen card submits"), Resolver->SubmitResult(Result));
	Fixture.Flush();

	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	TestFalse(TEXT("Full Burning Pact resolution finishes"), Queue->IsBusy());
	TestFalse(TEXT("Full Burning Pact resolution has no fault"), Queue->IsResolutionFaulted());
	TestEqual(TEXT("Exactly one chosen card is exhausted"), Deck->GetExhaustCount(), 1);
	TestTrue(TEXT("Chosen card is in ExhaustPile"), Fixture.ZoneContains(Deck->GetExhaustCards(), ChosenCard));
	TestTrue(TEXT("Untouched candidate remains in Hand"), Deck->IsCardInHand(UntouchedCard));
	TestTrue(TEXT("Fuel A is drawn after exhaust"), Deck->IsCardInHand(FuelCardA));
	TestTrue(TEXT("Fuel B is drawn after exhaust"), Deck->IsCardInHand(FuelCardB));
	TestEqual(TEXT("Base Burning Pact leaves three cards in Hand"), Deck->GetHandCount(), 3);
	TestTrue(TEXT("Played Burning Pact finishes into Discard"), Fixture.ZoneContains(Deck->GetDiscardCards(), PlayedCard));
	TestEqual(TEXT("Only Burning Pact remains in Discard"), Deck->GetDiscardCount(), 1);
	TestEqual(TEXT("PlayArea is empty after FinishCardPlay"), Deck->GetPlayAreaCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CBurningPactUpgradedDrawThreeTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.BurningPact.UpgradedExhaustThenDraw3",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CBurningPactUpgradedDrawThreeTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* BurningPact = Fixture.CreateBurningPactCard(TEXT("Wave1CBurningPactPlus"));
	UCardData* Chosen = Fixture.CreatePlainCard(TEXT("BPPlusChosen"));
	UCardData* Untouched = Fixture.CreatePlainCard(TEXT("BPPlusUntouched"));
	UCardData* FuelA = Fixture.CreatePlainCard(TEXT("BPPlusFuelA"));
	UCardData* FuelB = Fixture.CreatePlainCard(TEXT("BPPlusFuelB"));
	UCardData* FuelC = Fixture.CreatePlainCard(TEXT("BPPlusFuelC"));
	if (!TestTrue(TEXT("Fixture starts with six cards"), Fixture.Start({ BurningPact, Chosen, Untouched, FuelA, FuelB, FuelC })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CBurningPactPlus"));
	UCardInstance* ChosenCard = Fixture.FindInstanceByCardId(Deck, TEXT("BPPlusChosen"));
	UCardInstance* UntouchedCard = Fixture.FindInstanceByCardId(Deck, TEXT("BPPlusUntouched"));
	UCardInstance* FuelCardA = Fixture.FindInstanceByCardId(Deck, TEXT("BPPlusFuelA"));
	UCardInstance* FuelCardB = Fixture.FindInstanceByCardId(Deck, TEXT("BPPlusFuelB"));
	UCardInstance* FuelCardC = Fixture.FindInstanceByCardId(Deck, TEXT("BPPlusFuelC"));
	if (!TestNotNull(TEXT("Burning Pact+ instance exists"), PlayedCard)
		|| !TestNotNull(TEXT("Chosen card exists"), ChosenCard)
		|| !TestNotNull(TEXT("Untouched card exists"), UntouchedCard)
		|| !TestNotNull(TEXT("Fuel A exists"), FuelCardA)
		|| !TestNotNull(TEXT("Fuel B exists"), FuelCardB)
		|| !TestNotNull(TEXT("Fuel C exists"), FuelCardC))
	{
		return false;
	}

	if (!TestTrue(TEXT("Fuel A moved to Discard"), Fixture.DiscardForDrawFuel(Deck, FuelCardA))
		|| !TestTrue(TEXT("Fuel B moved to Discard"), Fixture.DiscardForDrawFuel(Deck, FuelCardB))
		|| !TestTrue(TEXT("Fuel C moved to Discard"), Fixture.DiscardForDrawFuel(Deck, FuelCardC))
		|| !TestTrue(TEXT("Burning Pact instance upgrades"), Fixture.UpgradeCard(PlayedCard)))
	{
		return false;
	}
	TestTrue(TEXT("Runtime card is upgraded"), PlayedCard->IsUpgraded());
	TestEqual(TEXT("Upgraded cost remains 1"), PlayedCard->GetCurrentCost(), 1);

	TestTrue(
		TEXT("Burning Pact+ play accepted"),
		Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution()
	);
	USelectionResolver* Resolver = Fixture.Battle->GetSelectionResolver();
	if (!TestNotNull(TEXT("Resolver exists"), Resolver))
	{
		return false;
	}
	TestTrue(TEXT("Selection is pending before upgraded Draw"), Resolver->HasPendingSelection());
	TestEqual(TEXT("Three draw-fuel cards still wait in Discard"), Deck->GetDiscardCount(), 3);

	FSelectionResult Result;
	Result.Status = ESelectionStatus::Resolved;
	Result.SelectedObjects.Add(ChosenCard);
	TestTrue(TEXT("Chosen card submits"), Resolver->SubmitResult(Result));
	Fixture.Flush();

	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	TestFalse(TEXT("Full upgraded resolution finishes"), Queue->IsBusy());
	TestFalse(TEXT("Full upgraded resolution has no fault"), Queue->IsResolutionFaulted());
	TestEqual(TEXT("Exactly one card exhausts"), Deck->GetExhaustCount(), 1);
	TestTrue(TEXT("Untouched card remains in Hand"), Deck->IsCardInHand(UntouchedCard));
	TestTrue(TEXT("Fuel A is drawn"), Deck->IsCardInHand(FuelCardA));
	TestTrue(TEXT("Fuel B is drawn"), Deck->IsCardInHand(FuelCardB));
	TestTrue(TEXT("Fuel C is drawn"), Deck->IsCardInHand(FuelCardC));
	TestEqual(TEXT("Burning Pact+ draws three after exhaust"), Deck->GetHandCount(), 4);
	TestTrue(TEXT("Played Burning Pact+ finishes into Discard"), Fixture.ZoneContains(Deck->GetDiscardCards(), PlayedCard));
	TestEqual(TEXT("PlayArea is empty after upgraded FinishCardPlay"), Deck->GetPlayAreaCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CBurningPactNativeHUDPendingClickTest,
	"SlayTheSpireDemo.CardExpansion.Wave1C.BurningPact.NativeHUDPendingCardClick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CBurningPactNativeHUDPendingClickTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	UCardData* BurningPact = Fixture.CreateBurningPactCard(TEXT("Wave1CHUDBurningPact"));
	UCardData* Chosen = Fixture.CreatePlainCard(TEXT("Wave1CHUDChosen"));
	UCardData* Untouched = Fixture.CreatePlainCard(TEXT("Wave1CHUDUntouched"));
	UCardData* FuelA = Fixture.CreatePlainCard(TEXT("Wave1CHUDFuelA"));
	UCardData* FuelB = Fixture.CreatePlainCard(TEXT("Wave1CHUDFuelB"));
	if (!TestTrue(TEXT("Fixture starts"), Fixture.Start({ BurningPact, Chosen, Untouched, FuelA, FuelB })))
	{
		return false;
	}

	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();
	UCardInstance* PlayedCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CHUDBurningPact"));
	UCardInstance* ChosenCard = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CHUDChosen"));
	UCardInstance* FuelCardA = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CHUDFuelA"));
	UCardInstance* FuelCardB = Fixture.FindInstanceByCardId(Deck, TEXT("Wave1CHUDFuelB"));
	if (!TestNotNull(TEXT("Played card exists"), PlayedCard)
		|| !TestNotNull(TEXT("Chosen card exists"), ChosenCard)
		|| !TestNotNull(TEXT("Fuel A exists"), FuelCardA)
		|| !TestNotNull(TEXT("Fuel B exists"), FuelCardB)
		|| !TestTrue(TEXT("Fuel A discarded"), Fixture.DiscardForDrawFuel(Deck, FuelCardA))
		|| !TestTrue(TEXT("Fuel B discarded"), Fixture.DiscardForDrawFuel(Deck, FuelCardB)))
	{
		return false;
	}

	TestTrue(
		TEXT("Burning Pact play accepted"),
		Fixture.Battle->RequestPlayCard(PlayedCard, nullptr).IsAcceptedForResolution()
	);

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	if (!TestNotNull(TEXT("ViewModel created"), ViewModel)
		|| !TestTrue(TEXT("ViewModel initializes against battle"), ViewModel->Initialize(Fixture.Battle, false)))
	{
		return false;
	}
	TestTrue(TEXT("ViewModel sees pending card selection while resolution is busy"), ViewModel->HasPendingCardSelection());
	TestTrue(TEXT("Chosen runtime id is an exposed candidate"), ViewModel->IsPendingCardSelectionCandidate(ChosenCard->GetRuntimeId()));
	TestFalse(TEXT("Played runtime id is not a candidate"), ViewModel->IsPendingCardSelectionCandidate(PlayedCard->GetRuntimeId()));
	TestFalse(TEXT("Burning Pact pending choice cannot be cancelled"), ViewModel->CanCancelPendingCardSelection());

	UBattleHUDWidget* HUD = NewObject<UBattleHUDWidget>(Fixture.World);
	if (!TestNotNull(TEXT("Native HUD object created"), HUD))
	{
		return false;
	}
	HUD->SetViewModel(ViewModel);

	TestFalse(
		TEXT("Clicking the played/non-candidate card is rejected without releasing selection"),
		HUD->SelectCard(PlayedCard->GetRuntimeId())
	);
	TestTrue(TEXT("Selection remains pending after invalid HUD click"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestTrue(
		TEXT("Native HUD card click submits the candidate during Resolving"),
		HUD->SelectCard(ChosenCard->GetRuntimeId())
	);
	Fixture.Flush();

	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	TestFalse(TEXT("HUD selection releases the waiting ActionQueue"), Queue->IsBusy());
	TestFalse(TEXT("HUD selection completes without fault"), Queue->IsResolutionFaulted());
	TestFalse(TEXT("No selection remains pending"), Fixture.Battle->GetSelectionResolver()->HasPendingSelection());
	TestTrue(TEXT("Chosen card exhausts through normal continuation"), Fixture.ZoneContains(Deck->GetExhaustCards(), ChosenCard));
	TestEqual(TEXT("Draw-2 follow-up ran after HUD selection"), Deck->GetHandCount(), 3);
	return true;
}

#endif
