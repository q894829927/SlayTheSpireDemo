#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleReadSnapshot.h"
#include "Presentation/RelicPresentationSnapshot.h"
#include "Relics/DeckShuffledCountTrigger.h"
#include "Relics/RelicCountTrigger.h"
#include "Relics/RelicData.h"
#include "UI/BattleHUDTypes.h"

namespace Phase7F
{
	FRelicReadView MakeReadView(URelicData* Definition, int32 Counter = 0)
	{
		FRelicReadView View;
		View.Definition = Definition;
		View.RelicId = IsValid(Definition) ? Definition->RelicId : NAME_None;
		View.RuntimeSequence = 1;
		View.Counter = Counter;
		return View;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7FCounterMetadataSingleSourceTest,
		"SlayTheSpireDemo.Phase7F.CounterMetadata.SingleSource",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	bool FPhase7FCounterMetadataSingleSourceTest::RunTest(const FString& Parameters)
	{
		URelicData* Definition = NewObject<URelicData>(GetTransientPackage());
		Definition->RelicId = TEXT("SingleSourceCounterRelic");
		Definition->bShowCounter = true;

		UDeckShuffledCountTrigger* Trigger = NewObject<UDeckShuffledCountTrigger>(Definition);
		Trigger->RequiredCount = 3;
		Definition->Triggers.Add(Trigger);

		TestTrue(TEXT("Concrete count trigger uses generic count contract"),
			IsValid(Cast<URelicCountTrigger>(Trigger)));

		int32 CounterMax = 0;
		TestTrue(TEXT("Relic resolves one authoritative counter max"),
			Definition->TryGetCounterMax(CounterMax));
		TestEqual(TEXT("Counter max comes from RequiredCount"), CounterMax, 3);

		TArray<FBattleHUDRelicView> Frozen;
		TArray<FRelicReadView> ReadViews{MakeReadView(Definition, 2)};
		TestTrue(TEXT("Presentation freeze succeeds"),
			RelicPresentationSnapshot::TryFreeze(ReadViews, Frozen));
		if (!TestEqual(TEXT("One relic freezes"), Frozen.Num(), 1)) return false;
		TestEqual(TEXT("Frozen Counter"), Frozen[0].Counter, 2);
		TestEqual(TEXT("Frozen CounterMax derives from RequiredCount"), Frozen[0].CounterMax, 3);

		Trigger->RequiredCount = 5;
		CounterMax = 0;
		TestTrue(TEXT("Updated Gameplay threshold remains resolvable"),
			Definition->TryGetCounterMax(CounterMax));
		TestEqual(TEXT("No second authored max can drift from Gameplay"), CounterMax, 5);
		TestTrue(TEXT("Freeze observes updated authoritative threshold"),
			RelicPresentationSnapshot::TryFreeze(ReadViews, Frozen));
		TestEqual(TEXT("Frozen CounterMax follows updated RequiredCount"), Frozen[0].CounterMax, 5);

		Definition->bShowCounter = false;
		TestTrue(TEXT("Hidden counter relic can still freeze"),
			RelicPresentationSnapshot::TryFreeze(ReadViews, Frozen));
		TestFalse(TEXT("Hidden counter flag stays hidden"), Frozen[0].bShowCounter);
		TestEqual(TEXT("Hidden counter max normalizes to zero"), Frozen[0].CounterMax, 0);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhase7FCounterMetadataInvalidDefinitionsTest,
		"SlayTheSpireDemo.Phase7F.CounterMetadata.InvalidDefinitions",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	bool FPhase7FCounterMetadataInvalidDefinitionsTest::RunTest(const FString& Parameters)
	{
		URelicData* Definition = NewObject<URelicData>(GetTransientPackage());
		Definition->RelicId = TEXT("InvalidCounterRelic");
		Definition->bShowCounter = true;

		int32 CounterMax = 123;
		TestFalse(TEXT("Visible counter without a count trigger has no max"),
			Definition->TryGetCounterMax(CounterMax));
		TestEqual(TEXT("Failed query resets output"), CounterMax, 0);

		TArray<FBattleHUDRelicView> Frozen;
		TArray<FRelicReadView> ReadViews{MakeReadView(Definition)};
		TestFalse(TEXT("Visible counter without count mechanic is rejected by freeze"),
			RelicPresentationSnapshot::TryFreeze(ReadViews, Frozen));
		TestEqual(TEXT("Rejected freeze leaves no partial output"), Frozen.Num(), 0);

		UDeckShuffledCountTrigger* First = NewObject<UDeckShuffledCountTrigger>(Definition);
		First->RequiredCount = 3;
		UDeckShuffledCountTrigger* Second = NewObject<UDeckShuffledCountTrigger>(Definition);
		Second->RequiredCount = 4;
		Definition->Triggers.Add(First);
		Definition->Triggers.Add(Second);

		TestFalse(TEXT("Two count triggers are ambiguous"), Definition->TryGetCounterMax(CounterMax));
		TestFalse(TEXT("Ambiguous visible counter is rejected by freeze"),
			RelicPresentationSnapshot::TryFreeze(ReadViews, Frozen));

		Definition->Triggers.RemoveAt(1);
		First->RequiredCount = 0;
		TestFalse(TEXT("Non-positive RequiredCount has no valid counter max"),
			Definition->TryGetCounterMax(CounterMax));
		TestFalse(TEXT("Non-positive visible counter threshold is rejected by freeze"),
			RelicPresentationSnapshot::TryFreeze(ReadViews, Frozen));
		return true;
	}
}

#endif
