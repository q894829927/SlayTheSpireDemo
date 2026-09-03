#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2D5TestSupport.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleReadSnapshot.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Events/BattleEvent.h"
#include "Events/BattleEventDispatcher.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/RelicPresentationSnapshot.h"
#include "Relics/DeckShuffledCountTrigger.h"
#include "Relics/Effects/GainEnergyRelicEffect.h"
#include "Relics/RelicData.h"
#include "UI/BattleHUDViewModel.h"
#include "Engine/Texture2D.h"

namespace Phase7D
{
	using namespace Phase6UIA2D5Test;

	URelicData* CreateRelicDefinition(
		UObject* Outer,
		FName RelicId,
		const TCHAR* DisplayName,
		bool bShowCounter,
		int32 CounterMax,
		UTexture2D* Icon = nullptr)
	{
		URelicData* Definition = NewObject<URelicData>(Outer);
		if (!IsValid(Definition))
		{
			return nullptr;
		}

		Definition->RelicId = RelicId;
		Definition->DisplayName = FText::FromString(DisplayName);
		Definition->Description = FText::FromString(
			FString::Printf(TEXT("%s description"), DisplayName));
		Definition->Icon = Icon;
		Definition->bShowCounter = bShowCounter;
		Definition->CounterDisplayMax = CounterMax;
		return Definition;
	}

	URelicData* CreateSundialDefinition(UObject* Outer)
	{
		URelicData* Definition = CreateRelicDefinition(
			Outer,
			TEXT("Sundial"),
			TEXT("Sundial"),
			true,
			3);
		if (!IsValid(Definition))
		{
			return nullptr;
		}

		UDeckShuffledCountTrigger* Trigger = NewObject<UDeckShuffledCountTrigger>(Definition);
		if (!IsValid(Trigger))
		{
			return nullptr;
		}
		Trigger->RequiredCount = 3;

		UGainEnergyRelicEffect* EnergyEffect = NewObject<UGainEnergyRelicEffect>(Trigger);
		if (!IsValid(EnergyEffect))
		{
			return nullptr;
		}
		EnergyEffect->Amount = 2;
		Trigger->Effects.Add(EnergyEffect);
		Definition->Triggers.Add(Trigger);
		return Definition;
	}

	bool ResolveQueuedShuffleEvents(
		FAcceptanceFixture& Fixture,
		int32 Count)
	{
		if (!Fixture.IsReady() || Count <= 0)
		{
			return false;
		}

		ABattleManager* Battle = Fixture.Battle;
		UBattleActionQueue* Queue = Battle->GetActionQueueForTesting();
		UDeckRuntime* Deck = Battle->GetDeckRuntimeForTesting();
		UBattleEventDispatcher* Dispatcher = nullptr;
		TArray<ACombatant*> Combatants;
		if (!IsValid(Queue)
			|| !IsValid(Deck)
			|| !Battle->TryBuildEventDispatchContext(Dispatcher, Combatants)
			|| !IsValid(Dispatcher)
			|| !Battle->BeginSystemPresentationResolutionForTesting())
		{
			return false;
		}

		const FPresentationRecordWriter Writer =
			Battle->GetActivePresentationRecordWriterForTesting();
		if (!Writer.IsAvailable())
		{
			return false;
		}

		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!Dispatcher->Dispatch(
				FBattleEvent::MakeDeckShuffled(Deck),
				Queue,
				Combatants,
			nullptr,
				&Writer))
			{
				return false;
			}
		}

		if (!Queue->StartProcessing())
		{
			return false;
		}
		Fixture.Flush();
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7DRelicReadAndFrozenSnapshotTest,
		"SlayTheSpireDemo.Phase7.RelicPresentation.ReadAndFrozenSnapshot",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	bool FPhase7DRelicReadAndFrozenSnapshotTest::RunTest(const FString& Parameters)
	{
		FAcceptanceFixture Fixture;
		UTexture2D* Icon = NewObject<UTexture2D>(Fixture.World);
		URelicData* CounterRelic = CreateRelicDefinition(
			Fixture.World,
			TEXT("CounterRelic"),
			TEXT("Counter Relic"),
			true,
			3,
			Icon);
		URelicData* PassiveRelic = CreateRelicDefinition(
			Fixture.World,
			TEXT("PassiveRelic"),
			TEXT("Passive Relic"),
			false,
			0);
		if (!TestNotNull(TEXT("Counter relic definition"), CounterRelic)
			|| !TestNotNull(TEXT("Passive relic definition"), PassiveRelic))
		{
			return false;
		}

		Fixture.Battle->DebugStartingRelics.Add(CounterRelic);
		Fixture.Battle->DebugStartingRelics.Add(PassiveRelic);
		TArray<UCardData*> EmptyDeck;
		if (!TestTrue(TEXT("Fixture starts"), Fixture.Start(EmptyDeck)))
		{
			return false;
		}

		FBattleReadSnapshot ReadSnapshot;
		if (!TestTrue(
			TEXT("Player-facing read snapshot contains Relics"),
			Fixture.Battle->TryBuildPlayerFacingReadSnapshot(ReadSnapshot)))
		{
			return false;
		}
		if (!TestEqual(TEXT("Read relic count"), ReadSnapshot.Relics.Num(), 2))
		{
			return false;
		}

		TestEqual(TEXT("First read RelicId"), ReadSnapshot.Relics[0].RelicId, FName(TEXT("CounterRelic")));
		TestEqual(TEXT("Second read RelicId"), ReadSnapshot.Relics[1].RelicId, FName(TEXT("PassiveRelic")));
		TestTrue(TEXT("Read RuntimeSequence is deterministic"),
			ReadSnapshot.Relics[0].RuntimeSequence < ReadSnapshot.Relics[1].RuntimeSequence);
		TestEqual(TEXT("Initial runtime counter"), ReadSnapshot.Relics[0].Counter, 0);
		TestTrue(TEXT("Read runtime observation handle exists"), ReadSnapshot.Relics[0].Relic.IsValid());
		TestTrue(TEXT("Read immutable definition handle exists"), ReadSnapshot.Relics[0].Definition.IsValid());

		FPresentationStateSnapshot Frozen;
		if (!TestTrue(
			TEXT("Latest frozen baseline exists"),
			Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Frozen))
			|| !TestEqual(TEXT("Frozen player relic count"), Frozen.Player.Relics.Num(), 2))
		{
			return false;
		}

		const FBattleHUDRelicView& First = Frozen.Player.Relics[0];
		TestEqual(TEXT("Frozen RelicId"), First.RelicId, FName(TEXT("CounterRelic")));
		TestTrue(TEXT("Frozen DisplayName"), First.DisplayName.EqualTo(CounterRelic->DisplayName));
		TestTrue(TEXT("Frozen Description"), First.Description.EqualTo(CounterRelic->Description));
		TestTrue(TEXT("Frozen counter visibility"), First.bShowCounter);
		TestEqual(TEXT("Frozen Counter"), First.Counter, 0);
		TestEqual(TEXT("Frozen CounterMax"), First.CounterMax, 3);
		TestTrue(TEXT("Frozen immutable icon"), First.Icon == Icon);
		TestEqual(TEXT("Enemy owns no relic HUD views"), Frozen.Enemy.Relics.Num(), 0);
		TestTrue(TEXT("Frozen relic order follows RuntimeSequence"),
			Frozen.Player.Relics[0].RuntimeSequence < Frozen.Player.Relics[1].RuntimeSequence);

		TestEqual(TEXT("ViewModel receives frozen relics by value"), Fixture.ViewModel->Player.Relics.Num(), 2);
		TestEqual(TEXT("ViewModel first relic identity"), Fixture.ViewModel->Player.Relics[0].RelicId, First.RelicId);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7DRelicFreezeContractTest,
		"SlayTheSpireDemo.Phase7.RelicPresentation.FreezeContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	bool FPhase7DRelicFreezeContractTest::RunTest(const FString& Parameters)
	{
		URelicData* LaterDefinition = CreateRelicDefinition(
			GetTransientPackage(),
			TEXT("Later"),
			TEXT("Later"),
			false,
			0);
		URelicData* EarlierDefinition = CreateRelicDefinition(
			GetTransientPackage(),
			TEXT("Earlier"),
			TEXT("Earlier"),
			true,
			5);
		if (!TestNotNull(TEXT("Later definition"), LaterDefinition)
			|| !TestNotNull(TEXT("Earlier definition"), EarlierDefinition))
		{
			return false;
		}

		FRelicReadView Later;
		Later.Definition = LaterDefinition;
		Later.RelicId = TEXT("Later");
		Later.RuntimeSequence = 20;
		Later.Counter = 7;

		FRelicReadView Earlier;
		Earlier.Definition = EarlierDefinition;
		Earlier.RelicId = TEXT("Earlier");
		Earlier.RuntimeSequence = 10;
		Earlier.Counter = 2;

		TArray<FRelicReadView> ReadViews{Later, Earlier};
		TArray<FBattleHUDRelicView> Frozen;
		if (!TestTrue(
			TEXT("Freeze succeeds for valid read facts"),
			RelicPresentationSnapshot::TryFreeze(ReadViews, Frozen))
			|| !TestEqual(TEXT("Frozen count"), Frozen.Num(), 2))
		{
			return false;
		}

		TestEqual(TEXT("Freeze sorts by RuntimeSequence"), Frozen[0].RelicId, FName(TEXT("Earlier")));
		TestTrue(TEXT("Counter view remains data-driven"), Frozen[0].bShowCounter);
		TestEqual(TEXT("Counter payload copied"), Frozen[0].Counter, 2);
		TestEqual(TEXT("Counter max copied from presentation metadata"), Frozen[0].CounterMax, 5);
		TestFalse(TEXT("Passive relic hides counter"), Frozen[1].bShowCounter);
		TestEqual(TEXT("Hidden counter max normalized"), Frozen[1].CounterMax, 0);

		EarlierDefinition->CounterDisplayMax = 0;
		TArray<FRelicReadView> InvalidViews{Earlier};
		TestFalse(
			TEXT("Visible counter without a positive display max is rejected"),
			RelicPresentationSnapshot::TryFreeze(InvalidViews, Frozen));
		TestEqual(TEXT("Rejected freeze leaves no partial relic output"), Frozen.Num(), 0);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7DRelicFinalSnapshotReconciliationTest,
		"SlayTheSpireDemo.Phase7.RelicPresentation.FinalSnapshotReconciliation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	bool FPhase7DRelicFinalSnapshotReconciliationTest::RunTest(const FString& Parameters)
	{
		FAcceptanceFixture Fixture;
		URelicData* Sundial = CreateSundialDefinition(Fixture.World);
		if (!TestNotNull(TEXT("Sundial definition"), Sundial))
		{
			return false;
		}
		Fixture.Battle->DebugStartingRelics.Add(Sundial);

		TArray<UCardData*> EmptyDeck;
		if (!TestTrue(TEXT("Fixture starts"), Fixture.Start(EmptyDeck))
			|| !TestTrue(TEXT("Reset capture"), Fixture.ResetAcceptanceCapture()))
		{
			return false;
		}

		if (!TestTrue(TEXT("First two shuffle events resolve"), ResolveQueuedShuffleEvents(Fixture, 2)))
		{
			return false;
		}
		TestEqual(TEXT("Counter baseline reaches two"), Fixture.ViewModel->Player.Relics[0].Counter, 2);

		if (!TestTrue(TEXT("Reset capture at counter two"), Fixture.ResetAcceptanceCapture()))
		{
			return false;
		}

		if (!TestTrue(TEXT("Third shuffle resolves"), ResolveQueuedShuffleEvents(Fixture, 1)))
		{
			return false;
		}
		if (!TestTrue(
			TEXT("EnergyChanged playback is active"),
			Fixture.Controller->IsWaitingForCompletionForTesting()))
		{
			return false;
		}

		const FCapturedEnvelope* Capture = Fixture.LastCapturedEnvelope();
		if (!TestNotNull(TEXT("Third-shuffle envelope captured"), Capture))
		{
			return false;
		}
		TestEqual(TEXT("Third-shuffle envelope has one record"), Capture->Envelope.Records.Num(), 1);
		TestTrue(TEXT("Third-shuffle record is EnergyChanged"),
			Capture->Envelope.Records[0].Type == EBattlePresentationRecordType::EnergyChanged);
		TestEqual(TEXT("FinalSnapshot already owns reset counter"),
			Capture->Envelope.FinalSnapshot.Player.Relics[0].Counter, 0);
		TestEqual(TEXT("Active historical ViewModel does not jump ahead"),
			Fixture.ViewModel->Player.Relics[0].Counter, 2);

		if (!TestTrue(TEXT("Complete EnergyChanged playback"), Fixture.CompleteCurrentPlayback()))
		{
			return false;
		}
		TestEqual(TEXT("Envelope completion reconciles counter to FinalSnapshot"),
			Fixture.ViewModel->Player.Relics[0].Counter, 0);
		TestFalse(TEXT("Controller is no longer waiting"),
			Fixture.Controller->IsWaitingForCompletionForTesting());
		return true;
	}
}

#endif
