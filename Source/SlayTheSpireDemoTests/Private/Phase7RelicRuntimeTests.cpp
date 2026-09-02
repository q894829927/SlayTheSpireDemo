#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Relics/RelicContainer.h"
#include "Relics/RelicData.h"
#include "Relics/RelicInstance.h"
#include "Relics/RelicRuntimeTypes.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"

namespace Phase7RelicRuntimeTest
{
	ACombatant* SpawnCombatant(UWorld* World)
	{
		if (!IsValid(World))
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACombatant* Combatant = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
		if (IsValid(Combatant))
		{
			Combatant->MaxHP = 100;
		}
		return Combatant;
	}

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

	struct FFixture
	{
		UWorld* World = nullptr;
		ABattleManager* Battle = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
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
			Player = SpawnCombatant(World);
			Enemy = SpawnCombatant(World);
			if (!IsValid(Battle) || !IsValid(Player) || !IsValid(Enemy))
			{
				return;
			}

			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->StartBattle();
			Container = Battle->GetPlayerRelicContainer();
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
			return IsValid(World)
				&& IsValid(Battle)
				&& IsValid(Player)
				&& IsValid(Enemy)
				&& IsValid(Container);
		}
	};

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

using namespace Phase7RelicRuntimeTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase7RelicRuntimeMembershipTest,
	"SlayTheSpireDemo.Phase7.RelicRuntime.MembershipAndIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase7RelicRuntimeMembershipTest::RunTest(const FString& Parameters)
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

	TestTrue(TEXT("BattleManager owns the authoritative player RelicContainer"), Fixture.Container->GetOuter() == Fixture.Battle);
	TestTrue(TEXT("Runtime instance freezes exact definition reference"), SundialInstance->GetDefinition() == Sundial);
	TestEqual(TEXT("Runtime instance exposes RelicId"), SundialInstance->GetRelicId(), FName(TEXT("Sundial")));
	TestTrue(TEXT("RuntimeSequence is non-zero"), SundialInstance->GetRuntimeSequence() > 0);
	TestTrue(TEXT("Runtime instance carries explicit Battle context"), SundialInstance->GetBattle() == Fixture.Battle);
	TestTrue(TEXT("Container carries explicit Battle context"), Fixture.Container->GetBattle() == Fixture.Battle);
	TestTrue(TEXT("FindRelicById resolves exact runtime instance"), Fixture.Container->FindRelicById(TEXT("Sundial")) == SundialInstance);
	TestTrue(TEXT("ContainsRelic resolves logical identity"), Fixture.Container->ContainsRelic(TEXT("Sundial")));
	TestTrue(TEXT("ContainsRelicInstance resolves exact identity"), Fixture.Container->ContainsRelicInstance(SundialInstance));

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
	FPhase7RelicRuntimeInvalidAndResetTest,
	"SlayTheSpireDemo.Phase7.RelicRuntime.InvalidAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase7RelicRuntimeInvalidAndResetTest::RunTest(const FString& Parameters)
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
	TestTrue(TEXT("Reinitialized runtime identity is newer in the same battle allocator session"), SecondAdd.Instance != nullptr && SecondAdd.Instance->GetRuntimeSequence() > OldInstance->GetRuntimeSequence());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase7RelicRuntimeDefinitionIsolationTest,
	"SlayTheSpireDemo.Phase7.RelicRuntime.DefinitionIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase7RelicRuntimeDefinitionIsolationTest::RunTest(const FString& Parameters)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase7RelicRuntimeBattleRestartTest,
	"SlayTheSpireDemo.Phase7.RelicRuntime.BattleRestartLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase7RelicRuntimeBattleRestartTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("Restart test world"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
	ACombatant* Player = SpawnCombatant(World);
	ACombatant* Enemy = SpawnCombatant(World);
	URelicData* Sundial = CreateRelic(World, TEXT("Sundial"));
	if (!TestNotNull(TEXT("Restart BattleManager"), Battle)
		|| !TestNotNull(TEXT("Restart Player"), Player)
		|| !TestNotNull(TEXT("Restart Enemy"), Enemy)
		|| !TestNotNull(TEXT("Restart Sundial definition"), Sundial))
	{
		World->DestroyWorld(false);
		return false;
	}

	Battle->Player = Player;
	Battle->Enemy = Enemy;
	Battle->OpeningHandDrawCount = 0;
	Battle->PlayerTurnDrawCount = 0;
	Battle->DebugStartingRelics.Add(Sundial);

	Battle->StartBattle();
	URelicContainer* FirstContainer = Battle->GetPlayerRelicContainer();
	const URelicInstance* FirstSundial = IsValid(FirstContainer)
		? FirstContainer->FindRelicById(TEXT("Sundial"))
		: nullptr;
	if (!TestNotNull(TEXT("First battle RelicContainer"), FirstContainer)
		|| !TestNotNull(TEXT("First battle Sundial runtime"), FirstSundial))
	{
		World->DestroyWorld(false);
		return false;
	}
	const uint64 FirstRuntimeSequence = FirstSundial->GetRuntimeSequence();

	Battle->StartBattle();
	URelicContainer* SecondContainer = Battle->GetPlayerRelicContainer();
	const URelicInstance* SecondSundial = IsValid(SecondContainer)
		? SecondContainer->FindRelicById(TEXT("Sundial"))
		: nullptr;

	TestTrue(TEXT("BattleManager retains one owned Container object"), SecondContainer == FirstContainer);
	TestNotNull(TEXT("Second battle Sundial runtime"), SecondSundial);
	TestTrue(TEXT("Old exact runtime instance is no longer current membership"), SecondContainer != nullptr && !SecondContainer->ContainsRelicInstance(FirstSundial));
	TestTrue(TEXT("New battle creates a different exact runtime object"), SecondSundial != nullptr && SecondSundial != FirstSundial);
	TestTrue(TEXT("New battle RuntimeSequence restarts from a valid battle-scoped value"), SecondSundial != nullptr && SecondSundial->GetRuntimeSequence() > 0);
	TestTrue(TEXT("First battle RuntimeSequence was valid"), FirstRuntimeSequence > 0);
	TestEqual(TEXT("Restart rebuilds exactly one configured relic"), SecondContainer != nullptr ? SecondContainer->GetRelics().Num() : 0, 1);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase7RelicRuntimeSetupOrderingTest,
	"SlayTheSpireDemo.Phase7.RelicRuntime.StartingRelicsPrecedeLaterStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase7RelicRuntimeSetupOrderingTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("Ordering test world"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
	ACombatant* Player = SpawnCombatant(World);
	ACombatant* Enemy = SpawnCombatant(World);
	URelicData* FirstRelic = CreateRelic(World, TEXT("FirstRelic"));
	URelicData* SecondRelic = CreateRelic(World, TEXT("SecondRelic"));
	if (!TestNotNull(TEXT("Ordering BattleManager"), Battle)
		|| !TestNotNull(TEXT("Ordering Player"), Player)
		|| !TestNotNull(TEXT("Ordering Enemy"), Enemy)
		|| !TestNotNull(TEXT("First configured Relic"), FirstRelic)
		|| !TestNotNull(TEXT("Second configured Relic"), SecondRelic))
	{
		World->DestroyWorld(false);
		return false;
	}

	Battle->Player = Player;
	Battle->Enemy = Enemy;
	Battle->OpeningHandDrawCount = 0;
	Battle->PlayerTurnDrawCount = 0;
	Battle->DebugStartingRelics.Add(FirstRelic);
	Battle->DebugStartingRelics.Add(SecondRelic);

	TestTrue(TEXT("Relic getter is not a lazy initialization path before StartBattle"), Battle->GetPlayerRelicContainer() == nullptr);
	Battle->StartBattle();

	URelicContainer* Container = Battle->GetPlayerRelicContainer();
	if (!TestNotNull(TEXT("StartBattle explicitly creates RelicContainer"), Container))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Configured Relics instantiate exactly once"), Container->GetRelics().Num(), 2);
	if (Container->GetRelics().Num() != 2)
	{
		World->DestroyWorld(false);
		return false;
	}

	const URelicInstance* FirstInstance = Container->GetRelics()[0].Get();
	const URelicInstance* SecondInstance = Container->GetRelics()[1].Get();
	TestTrue(TEXT("Configured authored order is preserved"), FirstInstance != nullptr && SecondInstance != nullptr
		&& FirstInstance->GetRelicId() == TEXT("FirstRelic")
		&& SecondInstance->GetRelicId() == TEXT("SecondRelic"));
	TestTrue(TEXT("First configured Relic receives non-zero sequence"), FirstInstance != nullptr && FirstInstance->GetRuntimeSequence() > 0);
	TestTrue(TEXT("Second configured Relic follows first in RuntimeSequence"), FirstInstance != nullptr && SecondInstance != nullptr
		&& SecondInstance->GetRuntimeSequence() > FirstInstance->GetRuntimeSequence());

	UStatusData* LaterStatus = NewObject<UStatusData>(World);
	if (!TestNotNull(TEXT("Later Status definition"), LaterStatus))
	{
		World->DestroyWorld(false);
		return false;
	}
	LaterStatus->StatusId = TEXT("LaterStatus");
	const uint64 StatusSequence = Battle->AllocateRuntimeSequence();
	const FStatusMutationResult StatusCommit = Player->GetStatusContainer()->ApplyStatusCommit(LaterStatus, 1, StatusSequence);
	TestTrue(TEXT("Later Status commits"), StatusCommit.IsCommitted());
	TestTrue(TEXT("Later Status receives a later cross-source RuntimeSequence"), SecondInstance != nullptr
		&& StatusCommit.RuntimeSequence > SecondInstance->GetRuntimeSequence());

	World->DestroyWorld(false);
	return true;
}

#endif
