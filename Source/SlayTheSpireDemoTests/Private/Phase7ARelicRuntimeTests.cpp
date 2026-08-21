#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleManager.h"
#include "Engine/World.h"
#include "Relics/RelicContainer.h"
#include "Relics/RelicData.h"
#include "Relics/RelicInstance.h"
#include "Relics/RelicRuntimeTypes.h"

namespace Phase7ARelicRuntimeTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ABattleManager* Battle = nullptr;
		URelicContainer* Container = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Battle))
			{
				return;
			}

			Container = NewObject<URelicContainer>(Battle);
			if (IsValid(Container))
			{
				Container->Initialize(Battle);
			}
		}

		~FFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		bool IsReady() const
		{
			return IsValid(World) && IsValid(Battle) && IsValid(Container);
		}
	};

	URelicData* CreateRelic(UObject* Outer, const TCHAR* RelicId)
	{
		URelicData* Definition = NewObject<URelicData>(Outer);
		if (IsValid(Definition))
		{
			Definition->RelicId = FName(RelicId);
			Definition->DisplayName = FText::FromString(RelicId);
			Definition->Description = FText::FromString(FString::Printf(TEXT("%s description"), RelicId));
		}
		return Definition;
	}

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (Fixture.IsReady())
		{
			return true;
		}
		Test.AddError(TEXT("Failed to create the Phase 7A relic runtime fixture."));
		return false;
	}
}

using namespace Phase7ARelicRuntimeTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase7ARelicRuntimeMembershipTest,
	"SlayTheSpireDemo.Phase7A.Runtime.MembershipAndIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase7ARelicRuntimeMembershipTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	URelicData* Sundial = CreateRelic(Fixture.World, TEXT("Sundial"));
	URelicData* Abacus = CreateRelic(Fixture.World, TEXT("Abacus"));
	if (!TestNotNull(TEXT("Sundial definition"), Sundial)
		|| !TestNotNull(TEXT("Abacus definition"), Abacus))
	{
		return false;
	}

	const FRelicAddResult AddSundial = Fixture.Container->AddRelic(Sundial);
	TestTrue(TEXT("Sundial add succeeds"), AddSundial.Outcome == ERelicAddOutcome::Added);
	TestTrue(TEXT("Typed result reports added"), AddSundial.WasAdded());
	URelicInstance* SundialInstance = AddSundial.Instance;
	if (!TestNotNull(TEXT("Sundial runtime instance"), SundialInstance))
	{
		return false;
	}

	TestTrue(TEXT("Runtime instance freezes exact definition reference"), SundialInstance->GetDefinition() == Sundial);
	TestEqual(TEXT("Runtime instance exposes RelicId"), SundialInstance->GetRelicId(), FName(TEXT("Sundial")));
	TestTrue(TEXT("RuntimeSequence is non-zero"), SundialInstance->GetRuntimeSequence() > 0);
	TestTrue(TEXT("Runtime instance carries explicit Battle context"), SundialInstance->GetBattle() == Fixture.Battle);
	TestTrue(TEXT("Container carries explicit Battle context"), Fixture.Container->GetBattle() == Fixture.Battle);
	TestTrue(TEXT("FindRelicById resolves exact runtime instance"), Fixture.Container->FindRelicById(TEXT("Sundial")) == SundialInstance);
	TestTrue(TEXT("ContainsRelic resolves logical identity"), Fixture.Container->ContainsRelic(TEXT("Sundial")));
	TestTrue(TEXT("ContainsRelicInstance resolves exact identity"), Fixture.Container->ContainsRelicInstance(SundialInstance));
	TestEqual(TEXT("Stable debug label"), SundialInstance->GetDebugLabel(), FString::Printf(TEXT("Sundial#%llu"), static_cast<unsigned long long>(SundialInstance->GetRuntimeSequence())));

	const FRelicAddResult Duplicate = Fixture.Container->AddRelic(Sundial);
	TestTrue(TEXT("Duplicate logical RelicId is explicit Duplicate"), Duplicate.Outcome == ERelicAddOutcome::Duplicate);
	TestTrue(TEXT("Duplicate resolves existing exact instance"), Duplicate.Instance == SundialInstance);
	TestEqual(TEXT("Duplicate does not add membership"), Fixture.Container->GetRelics().Num(), 1);

	const FRelicAddResult AddAbacus = Fixture.Container->AddRelic(Abacus);
	TestTrue(TEXT("Second distinct Relic adds"), AddAbacus.Outcome == ERelicAddOutcome::Added);
	if (!TestNotNull(TEXT("Abacus runtime instance"), AddAbacus.Instance))
	{
		return false;
	}
	TestTrue(TEXT("Runtime identities are monotonic without requiring contiguity"), AddAbacus.Instance->GetRuntimeSequence() > SundialInstance->GetRuntimeSequence());
	TestEqual(TEXT("Deterministic container insertion count"), Fixture.Container->GetRelics().Num(), 2);
	TestTrue(TEXT("Enumeration preserves Sundial insertion order"), Fixture.Container->GetRelics()[0].Get() == SundialInstance);
	TestTrue(TEXT("Enumeration preserves Abacus insertion order"), Fixture.Container->GetRelics()[1].Get() == AddAbacus.Instance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase7ARelicRuntimeInvalidAndResetTest,
	"SlayTheSpireDemo.Phase7A.Runtime.InvalidAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase7ARelicRuntimeInvalidAndResetTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	URelicData* InvalidId = CreateRelic(Fixture.World, TEXT("Temporary"));
	URelicData* Sundial = CreateRelic(Fixture.World, TEXT("Sundial"));
	if (!TestNotNull(TEXT("Invalid-id definition object"), InvalidId)
		|| !TestNotNull(TEXT("Sundial definition"), Sundial))
	{
		return false;
	}
	InvalidId->RelicId = NAME_None;

	TestTrue(TEXT("Null definition is Invalid"), Fixture.Container->AddRelic(nullptr).Outcome == ERelicAddOutcome::Invalid);
	TestTrue(TEXT("None RelicId is Invalid"), Fixture.Container->AddRelic(InvalidId).Outcome == ERelicAddOutcome::Invalid);
	TestEqual(TEXT("Invalid inputs do not change membership"), Fixture.Container->GetRelics().Num(), 0);

	const FRelicAddResult FirstAdd = Fixture.Container->AddRelic(Sundial);
	URelicInstance* OldInstance = FirstAdd.Instance;
	if (!TestNotNull(TEXT("Pre-reset Sundial"), OldInstance))
	{
		return false;
	}

	Fixture.Container->Reset();
	TestEqual(TEXT("Reset clears membership"), Fixture.Container->GetRelics().Num(), 0);
	TestFalse(TEXT("Reset removes exact old membership"), Fixture.Container->ContainsRelicInstance(OldInstance));
	TestTrue(TEXT("Reset clears Battle context"), Fixture.Container->GetBattle() == nullptr);
	TestTrue(TEXT("Uninitialized container rejects add"), Fixture.Container->AddRelic(Sundial).Outcome == ERelicAddOutcome::Invalid);

	Fixture.Container->Initialize(Fixture.Battle);
	const FRelicAddResult SecondAdd = Fixture.Container->AddRelic(Sundial);
	TestTrue(TEXT("Reinitialized container accepts same logical Relic"), SecondAdd.Outcome == ERelicAddOutcome::Added);
	TestTrue(TEXT("Reinitialized runtime instance differs from old exact instance"), SecondAdd.Instance != OldInstance);
	TestTrue(TEXT("Reinitialized runtime identity is newer"), SecondAdd.Instance != nullptr && SecondAdd.Instance->GetRuntimeSequence() > OldInstance->GetRuntimeSequence());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase7ARelicRuntimeDefinitionIsolationTest,
	"SlayTheSpireDemo.Phase7A.Runtime.DefinitionIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase7ARelicRuntimeDefinitionIsolationTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	URelicData* Sundial = CreateRelic(Fixture.World, TEXT("Sundial"));
	URelicContainer* SecondContainer = NewObject<URelicContainer>(Fixture.Battle);
	if (!TestNotNull(TEXT("Second battle-scoped runtime container"), SecondContainer)
		|| !TestNotNull(TEXT("Shared Sundial definition"), Sundial))
	{
		return false;
	}
	SecondContainer->Initialize(Fixture.Battle);

	const FRelicAddResult First = Fixture.Container->AddRelic(Sundial);
	const FRelicAddResult Second = SecondContainer->AddRelic(Sundial);
	if (!TestNotNull(TEXT("First runtime instance"), First.Instance)
		|| !TestNotNull(TEXT("Second runtime instance"), Second.Instance))
	{
		return false;
	}

	TestTrue(TEXT("Containers create distinct runtime objects from one immutable definition"), First.Instance != Second.Instance);
	TestTrue(TEXT("Both runtime objects reference the same immutable definition"), First.Instance->GetDefinition() == Sundial && Second.Instance->GetDefinition() == Sundial);
	TestTrue(TEXT("Distinct runtime objects receive distinct battle-scoped identities"), First.Instance->GetRuntimeSequence() != Second.Instance->GetRuntimeSequence());
	return true;
}

#endif
