#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "../Actions/BattleActionQueue.h"
#include "../Actions/DamageAction.h"
#include "../Actions/GainBlockAction.h"
#include "../Combat/Combatant.h"
#include "../Modifiers/Block/BlockFlatAddModifier.h"
#include "../Modifiers/Block/BlockModifierPipeline.h"
#include "../Modifiers/Block/BlockRatioModifier.h"
#include "../Modifiers/Block/BlockSpec.h"
#include "../Modifiers/Damage/DamageFlatAddModifier.h"
#include "../Modifiers/Damage/DamageModifierPipeline.h"
#include "../Modifiers/Damage/DamageRatioModifier.h"
#include "../Modifiers/Damage/DamageSpec.h"
#include "../Status/StatusContainer.h"
#include "../Status/StatusData.h"
#include "../Status/StatusInstance.h"
#include "Engine/World.h"

namespace Phase5Regression
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Source = nullptr;
		ACombatant* Target = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World))
			{
				return;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			Source = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Target = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(),
				FTransform(FVector(100.0, 0.0, 0.0)),
				SpawnParameters
			);

			if (IsValid(Source))
			{
				Source->MaxHP = 100;
				Source->InitializeCombatant();
			}

			if (IsValid(Target))
			{
				Target->MaxHP = 100;
				Target->InitializeCombatant();
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
			return IsValid(World)
				&& IsValid(Source)
				&& IsValid(Target)
				&& IsValid(Source->GetStatusContainer())
				&& IsValid(Target->GetStatusContainer());
		}
	};

	UStatusData* CreateStatus(UObject* Outer, const TCHAR* StatusId)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = FName(StatusId);
		return Definition;
	}

	UDamageFlatAddModifier* AddDamageFlat(
		UStatusData* Definition,
		EModifierScope Scope,
		int32 Value,
		EModifierAmountMode AmountMode,
		EDamageKind DamageKind = EDamageKind::Attack,
		int32 Priority = 0
	)
	{
		UDamageFlatAddModifier* Modifier = NewObject<UDamageFlatAddModifier>(Definition);
		Modifier->Scope = Scope;
		Modifier->Priority = Priority;
		Modifier->ApplicableDamageKind = DamageKind;
		Modifier->Value = Value;
		Modifier->AmountMode = AmountMode;
		Definition->DamageModifiers.Add(Modifier);
		return Modifier;
	}

	UDamageRatioModifier* AddDamageRatio(
		UStatusData* Definition,
		EModifierScope Scope,
		EDamageModifierPhase Phase,
		int32 Numerator,
		int32 Denominator,
		EModifierAmountMode AmountMode,
		EDamageKind DamageKind = EDamageKind::Attack,
		int32 Priority = 0
	)
	{
		UDamageRatioModifier* Modifier = NewObject<UDamageRatioModifier>(Definition);
		Modifier->Scope = Scope;
		Modifier->Priority = Priority;
		Modifier->ApplicableDamageKind = DamageKind;
		Modifier->Phase = Phase;
		Modifier->Numerator = Numerator;
		Modifier->Denominator = Denominator;
		Modifier->AmountMode = AmountMode;
		Definition->DamageModifiers.Add(Modifier);
		return Modifier;
	}

	UBlockFlatAddModifier* AddBlockFlat(
		UStatusData* Definition,
		EModifierScope Scope,
		int32 Value,
		EModifierAmountMode AmountMode,
		int32 Priority = 0
	)
	{
		UBlockFlatAddModifier* Modifier = NewObject<UBlockFlatAddModifier>(Definition);
		Modifier->Scope = Scope;
		Modifier->Priority = Priority;
		Modifier->Value = Value;
		Modifier->AmountMode = AmountMode;
		Definition->BlockModifiers.Add(Modifier);
		return Modifier;
	}

	UBlockRatioModifier* AddBlockRatio(
		UStatusData* Definition,
		EModifierScope Scope,
		int32 Numerator,
		int32 Denominator,
		EModifierAmountMode AmountMode,
		int32 Priority = 0
	)
	{
		UBlockRatioModifier* Modifier = NewObject<UBlockRatioModifier>(Definition);
		Modifier->Scope = Scope;
		Modifier->Priority = Priority;
		Modifier->Numerator = Numerator;
		Modifier->Denominator = Denominator;
		Modifier->AmountMode = AmountMode;
		Definition->BlockModifiers.Add(Modifier);
		return Modifier;
	}

	UStatusInstance* ApplyStatus(
		ACombatant* Combatant,
		UStatusData* Definition,
		int32 Amount,
		uint64 RuntimeSequence,
		bool* bOutCreated = nullptr
	)
	{
		if (!IsValid(Combatant) || !IsValid(Combatant->GetStatusContainer()))
		{
			return nullptr;
		}

		bool bCreated = false;
		UStatusInstance* Instance = Combatant->GetStatusContainer()->ApplyStatus(
			Definition,
			Amount,
			RuntimeSequence,
			bCreated
		);
		if (bOutCreated)
		{
			*bOutCreated = bCreated;
		}
		return Instance;
	}

	int32 ResolveDamage(ACombatant* Source, ACombatant* Target, int32 BaseAmount, EDamageKind DamageKind)
	{
		FDamageSpec Spec;
		Spec.Source = Source;
		Spec.Target = Target;
		Spec.BaseAmount = BaseAmount;
		Spec.DamageKind = DamageKind;
		FDamageModifierPipeline::Resolve(Spec);
		return Spec.ResolvedAmount;
	}

	int32 ResolveBlock(ACombatant* Source, ACombatant* Target, int32 BaseAmount)
	{
		FBlockSpec Spec;
		Spec.Source = Source;
		Spec.Target = Target;
		Spec.BaseAmount = BaseAmount;
		FBlockModifierPipeline::Resolve(Spec);
		return Spec.ResolvedAmount;
	}

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (!Fixture.IsReady())
		{
			Test.AddError(TEXT("Failed to create the transient Phase 5 automation-test fixture."));
			return false;
		}
		return true;
	}
}

using namespace Phase5Regression;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5DamageBaseZeroCanReceiveFlatAddTest,
	"SlayTheSpireDemo.Phase5.Damage.BaseZeroCanReceiveFlatAdd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5DamageBaseZeroCanReceiveFlatAddTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* Strength = CreateStatus(Fixture.World, TEXT("Strength"));
	AddDamageFlat(Strength, EModifierScope::Source, 1, EModifierAmountMode::ScaleWithAmount);
	if (!TestNotNull(TEXT("Strength runtime status"), ApplyStatus(Fixture.Source, Strength, 2, 1)))
	{
		return false;
	}

	UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
	UDamageAction* Damage = NewObject<UDamageAction>(Queue);
	Damage->Initialize(Fixture.Source, Fixture.Target, 0, EDamageKind::Attack);
	Queue->AddToBack(Damage);
	Queue->StartProcessing();

	TestEqual(TEXT("Base 0 + Strength 2 commits 2 damage"), Fixture.Target->HP, 98);
	TestFalse(TEXT("Queue drained"), Queue->IsBusy());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5DamageStrengthScalesWithAmountTest,
	"SlayTheSpireDemo.Phase5.Damage.StrengthScalesWithAmount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5DamageStrengthScalesWithAmountTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* Strength = CreateStatus(Fixture.World, TEXT("Strength"));
	AddDamageFlat(Strength, EModifierScope::Source, 1, EModifierAmountMode::ScaleWithAmount);
	ApplyStatus(Fixture.Source, Strength, 3, 1);

	TestEqual(TEXT("Strength Amount 3 adds three damage"), ResolveDamage(Fixture.Source, Fixture.Target, 9, EDamageKind::Attack), 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5DamageWeakPresenceOnlyTest,
	"SlayTheSpireDemo.Phase5.Damage.WeakPresenceOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5DamageWeakPresenceOnlyTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* Weak = CreateStatus(Fixture.World, TEXT("Weak"));
	AddDamageRatio(
		Weak,
		EModifierScope::Source,
		EDamageModifierPhase::SourceMultiplier,
		3,
		4,
		EModifierAmountMode::PresenceOnly
	);
	ApplyStatus(Fixture.Source, Weak, 3, 1);

	TestEqual(TEXT("Weak Amount 3 applies 3/4 exactly once"), ResolveDamage(Fixture.Source, Fixture.Target, 12, EDamageKind::Attack), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5DamageVulnerablePresenceOnlyTest,
	"SlayTheSpireDemo.Phase5.Damage.VulnerablePresenceOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5DamageVulnerablePresenceOnlyTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* Vulnerable = CreateStatus(Fixture.World, TEXT("Vulnerable"));
	AddDamageRatio(
		Vulnerable,
		EModifierScope::Target,
		EDamageModifierPhase::TargetMultiplier,
		3,
		2,
		EModifierAmountMode::PresenceOnly
	);
	ApplyStatus(Fixture.Target, Vulnerable, 2, 1);

	TestEqual(TEXT("Vulnerable Amount 2 applies 3/2 exactly once"), ResolveDamage(Fixture.Source, Fixture.Target, 8, EDamageKind::Attack), 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5DamageRatioFloorsPerModifierTest,
	"SlayTheSpireDemo.Phase5.Damage.RatioFloorsPerModifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5DamageRatioFloorsPerModifierTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* SourceRatio = CreateStatus(Fixture.World, TEXT("SourceRatio"));
	AddDamageRatio(
		SourceRatio,
		EModifierScope::Source,
		EDamageModifierPhase::SourceMultiplier,
		2,
		3,
		EModifierAmountMode::PresenceOnly
	);

	UStatusData* TargetRatio = CreateStatus(Fixture.World, TEXT("TargetRatio"));
	AddDamageRatio(
		TargetRatio,
		EModifierScope::Target,
		EDamageModifierPhase::TargetMultiplier,
		3,
		2,
		EModifierAmountMode::PresenceOnly
	);

	ApplyStatus(Fixture.Source, SourceRatio, 1, 2);
	ApplyStatus(Fixture.Target, TargetRatio, 1, 1);

	// Sequential integer resolution is 5 * 2 / 3 = 3, then 3 * 3 / 2 = 4.
	// Combining the ratios before rounding would incorrectly return 5.
	TestEqual(TEXT("Each ratio floors before the next modifier"), ResolveDamage(Fixture.Source, Fixture.Target, 5, EDamageKind::Attack), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5DamagePhaseBeforeRuntimeSequenceTest,
	"SlayTheSpireDemo.Phase5.Damage.PhaseBeforeRuntimeSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5DamagePhaseBeforeRuntimeSequenceTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* Vulnerable = CreateStatus(Fixture.World, TEXT("Vulnerable"));
	AddDamageRatio(Vulnerable, EModifierScope::Target, EDamageModifierPhase::TargetMultiplier, 3, 2, EModifierAmountMode::PresenceOnly);
	UStatusData* Weak = CreateStatus(Fixture.World, TEXT("Weak"));
	AddDamageRatio(Weak, EModifierScope::Source, EDamageModifierPhase::SourceMultiplier, 3, 4, EModifierAmountMode::PresenceOnly);
	UStatusData* Strength = CreateStatus(Fixture.World, TEXT("Strength"));
	AddDamageFlat(Strength, EModifierScope::Source, 1, EModifierAmountMode::ScaleWithAmount);

	const UStatusInstance* VulnerableInstance = ApplyStatus(Fixture.Target, Vulnerable, 2, 1);
	const UStatusInstance* WeakInstance = ApplyStatus(Fixture.Source, Weak, 3, 2);
	const UStatusInstance* StrengthInstance = ApplyStatus(Fixture.Source, Strength, 2, 3);

	TestEqual(TEXT("Vulnerable sequence"), VulnerableInstance ? VulnerableInstance->GetRuntimeSequence() : 0ull, 1ull);
	TestEqual(TEXT("Weak sequence"), WeakInstance ? WeakInstance->GetRuntimeSequence() : 0ull, 2ull);
	TestEqual(TEXT("Strength sequence"), StrengthInstance ? StrengthInstance->GetRuntimeSequence() : 0ull, 3ull);
	TestEqual(TEXT("Phase ordering outranks runtime sequence"), ResolveDamage(Fixture.Source, Fixture.Target, 9, EDamageKind::Attack), 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5DamageEffectFiltersAttackModifiersTest,
	"SlayTheSpireDemo.Phase5.Damage.EffectFiltersAttackModifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5DamageEffectFiltersAttackModifiersTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* Strength = CreateStatus(Fixture.World, TEXT("Strength"));
	AddDamageFlat(Strength, EModifierScope::Source, 1, EModifierAmountMode::ScaleWithAmount, EDamageKind::Attack);
	UStatusData* Weak = CreateStatus(Fixture.World, TEXT("Weak"));
	AddDamageRatio(Weak, EModifierScope::Source, EDamageModifierPhase::SourceMultiplier, 3, 4, EModifierAmountMode::PresenceOnly, EDamageKind::Attack);
	UStatusData* Vulnerable = CreateStatus(Fixture.World, TEXT("Vulnerable"));
	AddDamageRatio(Vulnerable, EModifierScope::Target, EDamageModifierPhase::TargetMultiplier, 3, 2, EModifierAmountMode::PresenceOnly, EDamageKind::Attack);

	ApplyStatus(Fixture.Source, Strength, 2, 1);
	ApplyStatus(Fixture.Source, Weak, 3, 2);
	ApplyStatus(Fixture.Target, Vulnerable, 2, 3);

	TestEqual(TEXT("Effect damage ignores Attack-only status modifiers"), ResolveDamage(Fixture.Source, Fixture.Target, 9, EDamageKind::Effect), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5BlockBaseZeroCanReceiveFlatAddTest,
	"SlayTheSpireDemo.Phase5.Block.BaseZeroCanReceiveFlatAdd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5BlockBaseZeroCanReceiveFlatAddTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* Dexterity = CreateStatus(Fixture.World, TEXT("Dexterity"));
	AddBlockFlat(Dexterity, EModifierScope::Target, 1, EModifierAmountMode::ScaleWithAmount);
	ApplyStatus(Fixture.Target, Dexterity, 2, 1);

	UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);
	UGainBlockAction* GainBlock = NewObject<UGainBlockAction>(Queue);
	GainBlock->Initialize(Fixture.Source, Fixture.Target, 0);
	Queue->AddToBack(GainBlock);
	Queue->StartProcessing();

	TestEqual(TEXT("Base 0 + Dexterity 2 commits 2 Block"), Fixture.Target->Block, 2);
	TestFalse(TEXT("Queue drained"), Queue->IsBusy());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5BlockDexterityScalesWithAmountTest,
	"SlayTheSpireDemo.Phase5.Block.DexterityScalesWithAmount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5BlockDexterityScalesWithAmountTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* Dexterity = CreateStatus(Fixture.World, TEXT("Dexterity"));
	AddBlockFlat(Dexterity, EModifierScope::Target, 1, EModifierAmountMode::ScaleWithAmount);
	ApplyStatus(Fixture.Target, Dexterity, 2, 1);

	TestEqual(TEXT("Dexterity Amount 2 adds two Block"), ResolveBlock(Fixture.Source, Fixture.Target, 5), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5BlockFrailtyPresenceOnlyTest,
	"SlayTheSpireDemo.Phase5.Block.FrailtyPresenceOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5BlockFrailtyPresenceOnlyTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* Frailty = CreateStatus(Fixture.World, TEXT("Frailty"));
	AddBlockRatio(Frailty, EModifierScope::Target, 3, 4, EModifierAmountMode::PresenceOnly);
	ApplyStatus(Fixture.Target, Frailty, 3, 1);

	TestEqual(TEXT("Frailty Amount 3 applies 3/4 exactly once"), ResolveBlock(Fixture.Source, Fixture.Target, 8), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5StatusReapplyPreservesRuntimeSequenceTest,
	"SlayTheSpireDemo.Phase5.Status.ReapplyPreservesRuntimeSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5StatusReapplyPreservesRuntimeSequenceTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UStatusData* Status = CreateStatus(Fixture.World, TEXT("MergeTest"));
	bool bFirstCreated = false;
	bool bSecondCreated = true;
	UStatusInstance* First = ApplyStatus(Fixture.Source, Status, 2, 7, &bFirstCreated);
	UStatusInstance* Second = ApplyStatus(Fixture.Source, Status, 1, 99, &bSecondCreated);

	TestTrue(TEXT("First application creates runtime status"), bFirstCreated);
	TestFalse(TEXT("Reapplication merges instead of creating"), bSecondCreated);
	TestTrue(TEXT("Reapplication returns the same runtime instance"), First != nullptr && First == Second);
	TestEqual(TEXT("Merged Amount"), Second ? Second->GetAmount() : 0, 3);
	TestEqual(TEXT("Original RuntimeSequence is preserved"), Second ? Second->GetRuntimeSequence() : 0ull, 7ull);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase5QueueFrontBackOrderingTest,
	"SlayTheSpireDemo.Phase5.Queue.FrontBackOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase5QueueFrontBackOrderingTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(Fixture.World);

	UDamageAction* BackDamage = NewObject<UDamageAction>(Queue);
	BackDamage->Initialize(Fixture.Source, Fixture.Target, 3, EDamageKind::Attack);
	Queue->AddToBack(BackDamage);

	UGainBlockAction* FirstFront = NewObject<UGainBlockAction>(Queue);
	FirstFront->Initialize(Fixture.Target, Fixture.Target, 5);
	Queue->AddToFront(FirstFront);

	UDamageAction* SecondFront = NewObject<UDamageAction>(Queue);
	SecondFront->Initialize(Fixture.Source, Fixture.Target, 7, EDamageKind::Attack);
	Queue->AddToFront(SecondFront);

	// Individual AddToFront calls are intentionally LIFO:
	// Damage 7 -> Gain Block 5 -> Damage 3.
	Queue->StartProcessing();

	TestEqual(TEXT("LIFO front insertion executes Damage 7 before Block 5"), Fixture.Target->HP, 93);
	TestEqual(TEXT("Damage 3 consumes three of the five Block"), Fixture.Target->Block, 2);
	TestEqual(TEXT("No pending actions remain"), Queue->GetPendingCount(), 0);
	TestFalse(TEXT("Queue is no longer busy"), Queue->IsBusy());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
