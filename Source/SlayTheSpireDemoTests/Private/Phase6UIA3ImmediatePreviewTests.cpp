#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleImmediatePreview.h"
#include "Battle/BattleTextTypes.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Cards/Effects/DrawCardEffect.h"
#include "Cards/Effects/GainBlockCardEffect.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Modifiers/Block/BlockFlatAddModifier.h"
#include "Modifiers/Block/BlockRatioModifier.h"
#include "Modifiers/Damage/DamageFlatAddModifier.h"
#include "Modifiers/Damage/DamageRatioModifier.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Status/StatusInstance.h"

namespace Phase6UIA3ImmediatePreview
{
	struct FPreviewFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		uint64 NextStatusSequence = 1;

		FPreviewFixture()
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
			if (IsValid(Player))
			{
				Player->InitializeCombatant();
				Player->PresentationId = TEXT("PlayerPreview");
			}
			if (IsValid(Enemy))
			{
				Enemy->InitializeCombatant();
				Enemy->PresentationId = TEXT("EnemyPreview");
			}
		}

		~FPreviewFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
		}

		UStatusInstance* ApplyStatus(ACombatant* Target, UStatusData* Definition, int32 Amount = 1)
		{
			UStatusContainer* Container = IsValid(Target) ? Target->GetStatusContainer() : nullptr;
			if (!IsValid(Container) || !IsValid(Definition))
			{
				return nullptr;
			}

			bool bCreated = false;
			return Container->ApplyStatus(Definition, Amount, NextStatusSequence++, bCreated);
		}
	};

	UStatusData* MakeDamageFlatStatus(UObject* Outer, const TCHAR* Id, int32 Value)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = Id;
		UDamageFlatAddModifier* Modifier = NewObject<UDamageFlatAddModifier>(Definition);
		Modifier->Value = Value;
		Modifier->AmountMode = EModifierAmountMode::ScaleWithAmount;
		Definition->DamageModifiers.Add(Modifier);
		return Definition;
	}

	UStatusData* MakeDamageRatioStatus(
		UObject* Outer,
		const TCHAR* Id,
		EModifierScope Scope,
		EDamageModifierPhase Phase,
		int32 Numerator,
		int32 Denominator
	)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = Id;
		UDamageRatioModifier* Modifier = NewObject<UDamageRatioModifier>(Definition);
		Modifier->Scope = Scope;
		Modifier->Phase = Phase;
		Modifier->Numerator = Numerator;
		Modifier->Denominator = Denominator;
		Definition->DamageModifiers.Add(Modifier);
		return Definition;
	}
}

using namespace Phase6UIA3ImmediatePreview;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmediateDamagePreviewPipelineTest,
	"SlayTheSpireDemo.UIA3.ImmediatePreview.DamageUsesTargetSpecificPipelineAndPreservesHits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FImmediateDamagePreviewPipelineTest::RunTest(const FString& Parameters)
{
	FPreviewFixture Fixture;
	if (!IsValid(Fixture.Player) || !IsValid(Fixture.Enemy))
	{
		AddError(TEXT("Failed to create combatants for immediate Damage preview test."));
		return false;
	}

	UStatusInstance* Strength = Fixture.ApplyStatus(
		Fixture.Player,
		MakeDamageFlatStatus(Fixture.World, TEXT("Strength"), 2)
	);
	UStatusInstance* Weak = Fixture.ApplyStatus(
		Fixture.Player,
		MakeDamageRatioStatus(
			Fixture.World,
			TEXT("Weak"),
			EModifierScope::Source,
			EDamageModifierPhase::SourceMultiplier,
			3,
			4
		)
	);
	UStatusInstance* Vulnerable = Fixture.ApplyStatus(
		Fixture.Enemy,
		MakeDamageRatioStatus(
			Fixture.World,
			TEXT("Vulnerable"),
			EModifierScope::Target,
			EDamageModifierPhase::TargetMultiplier,
			3,
			2
		)
	);

	UDamageCardEffect* Effect = NewObject<UDamageCardEffect>(Fixture.World);
	Effect->BaseAmount = 8;
	Effect->HitCount = 2;
	Effect->DescriptionArgumentName = TEXT("Damage");

	FCardEffectPreviewContext Context;
	Context.Source = Fixture.Player;
	Context.Target = Fixture.Enemy;

	const int32 PlayerHPBefore = Fixture.Player->HP;
	const int32 PlayerBlockBefore = Fixture.Player->Block;
	const int32 EnemyHPBefore = Fixture.Enemy->HP;
	const int32 EnemyBlockBefore = Fixture.Enemy->Block;
	const int32 StrengthBefore = IsValid(Strength) ? Strength->GetAmount() : INDEX_NONE;
	const int32 WeakBefore = IsValid(Weak) ? Weak->GetAmount() : INDEX_NONE;
	const int32 VulnerableBefore = IsValid(Vulnerable) ? Vulnerable->GetAmount() : INDEX_NONE;

	TArray<FImmediatePreviewOperation> First;
	TArray<FImmediatePreviewOperation> Second;
	Effect->BuildImmediatePreviewOperations(Context, 4, First);
	Effect->BuildImmediatePreviewOperations(Context, 4, Second);

	TestEqual(TEXT("Damage contributes exactly one operation"), First.Num(), 1);
	TestEqual(TEXT("Repeated same-state query contributes exactly one operation"), Second.Num(), 1);
	if (First.Num() == 1 && Second.Num() == 1)
	{
		const FImmediatePreviewOperation& A = First[0];
		const FImmediatePreviewOperation& B = Second[0];
		TestEqual(TEXT("EffectIndex is preserved"), A.EffectIndex, 4);
		TestEqual(TEXT("Semantic argument name is preserved"), A.SemanticArgumentName, FName(TEXT("Damage")));
		TestEqual(TEXT("Operation is Damage"), A.Type, EImmediatePreviewOperationType::Damage);
		TestEqual(TEXT("Strength then Weak then target Vulnerable resolve per hit"), A.ResolvedAmount, 10);
		TestEqual(TEXT("Fixed multi-hit count is preserved"), A.HitCount, 2);
		TestEqual(TEXT("Repeated query keeps EffectIndex deterministic"), B.EffectIndex, A.EffectIndex);
		TestEqual(TEXT("Repeated query keeps semantic identity deterministic"), B.SemanticArgumentName, A.SemanticArgumentName);
		TestEqual(TEXT("Repeated query keeps operation type deterministic"), B.Type, A.Type);
		TestEqual(TEXT("Repeated query keeps resolved amount deterministic"), B.ResolvedAmount, A.ResolvedAmount);
		TestEqual(TEXT("Repeated query keeps hit count deterministic"), B.HitCount, A.HitCount);
	}

	TestEqual(TEXT("Damage preview does not mutate Player HP"), Fixture.Player->HP, PlayerHPBefore);
	TestEqual(TEXT("Damage preview does not mutate Player Block"), Fixture.Player->Block, PlayerBlockBefore);
	TestEqual(TEXT("Damage preview does not mutate Enemy HP"), Fixture.Enemy->HP, EnemyHPBefore);
	TestEqual(TEXT("Damage preview does not mutate Enemy Block"), Fixture.Enemy->Block, EnemyBlockBefore);
	if (IsValid(Strength)) TestEqual(TEXT("Damage preview does not mutate Strength"), Strength->GetAmount(), StrengthBefore);
	if (IsValid(Weak)) TestEqual(TEXT("Damage preview does not mutate Weak"), Weak->GetAmount(), WeakBefore);
	if (IsValid(Vulnerable)) TestEqual(TEXT("Damage preview does not mutate Vulnerable"), Vulnerable->GetAmount(), VulnerableBefore);

	TArray<FImmediatePreviewOperation> MissingTarget;
	Context.Target = nullptr;
	Effect->BuildImmediatePreviewOperations(Context, 4, MissingTarget);
	TestEqual(TEXT("Missing concrete Damage target fabricates no target-specific operation"), MissingTarget.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmediateBlockPreviewPipelineTest,
	"SlayTheSpireDemo.UIA3.ImmediatePreview.BlockUsesSelfPipelineAndIgnoresHoveredEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FImmediateBlockPreviewPipelineTest::RunTest(const FString& Parameters)
{
	FPreviewFixture Fixture;
	if (!IsValid(Fixture.Player) || !IsValid(Fixture.Enemy))
	{
		AddError(TEXT("Failed to create combatants for immediate Block preview test."));
		return false;
	}

	UStatusData* DexterityDefinition = NewObject<UStatusData>(Fixture.World);
	DexterityDefinition->StatusId = TEXT("Dexterity");
	UBlockFlatAddModifier* DexterityModifier = NewObject<UBlockFlatAddModifier>(DexterityDefinition);
	DexterityModifier->Value = 1;
	DexterityDefinition->BlockModifiers.Add(DexterityModifier);
	UStatusInstance* Dexterity = Fixture.ApplyStatus(Fixture.Player, DexterityDefinition);

	UStatusData* FrailtyDefinition = NewObject<UStatusData>(Fixture.World);
	FrailtyDefinition->StatusId = TEXT("Frailty");
	UBlockRatioModifier* FrailtyModifier = NewObject<UBlockRatioModifier>(FrailtyDefinition);
	FrailtyModifier->Numerator = 3;
	FrailtyModifier->Denominator = 4;
	FrailtyDefinition->BlockModifiers.Add(FrailtyModifier);
	UStatusInstance* Frailty = Fixture.ApplyStatus(Fixture.Player, FrailtyDefinition);

	UGainBlockCardEffect* Effect = NewObject<UGainBlockCardEffect>(Fixture.World);
	Effect->BaseAmount = 5;
	Effect->DescriptionArgumentName = TEXT("Block");

	FCardEffectPreviewContext Context;
	Context.Source = Fixture.Player;
	Context.Target = Fixture.Enemy;

	const int32 PlayerHPBefore = Fixture.Player->HP;
	const int32 PlayerBlockBefore = Fixture.Player->Block;
	const int32 EnemyHPBefore = Fixture.Enemy->HP;
	const int32 EnemyBlockBefore = Fixture.Enemy->Block;
	const int32 DexterityBefore = IsValid(Dexterity) ? Dexterity->GetAmount() : INDEX_NONE;
	const int32 FrailtyBefore = IsValid(Frailty) ? Frailty->GetAmount() : INDEX_NONE;

	TArray<FImmediatePreviewOperation> Operations;
	Effect->BuildImmediatePreviewOperations(Context, 7, Operations);

	TestEqual(TEXT("Block contributes exactly one operation"), Operations.Num(), 1);
	if (Operations.Num() == 1)
	{
		const FImmediatePreviewOperation& Operation = Operations[0];
		TestEqual(TEXT("Block EffectIndex is preserved"), Operation.EffectIndex, 7);
		TestEqual(TEXT("Block semantic argument name is preserved"), Operation.SemanticArgumentName, FName(TEXT("Block")));
		TestEqual(TEXT("Operation is Block"), Operation.Type, EImmediatePreviewOperationType::Block);
		TestEqual(TEXT("Self Block uses Player Dexterity/Frailty despite Enemy hover target"), Operation.ResolvedAmount, 4);
		TestEqual(TEXT("Block operation has one hit"), Operation.HitCount, 1);
	}

	TestEqual(TEXT("Block preview does not mutate Player HP"), Fixture.Player->HP, PlayerHPBefore);
	TestEqual(TEXT("Block preview does not commit Player Block"), Fixture.Player->Block, PlayerBlockBefore);
	TestEqual(TEXT("Block preview does not mutate hovered Enemy HP"), Fixture.Enemy->HP, EnemyHPBefore);
	TestEqual(TEXT("Block preview does not mutate hovered Enemy Block"), Fixture.Enemy->Block, EnemyBlockBefore);
	if (IsValid(Dexterity)) TestEqual(TEXT("Block preview does not mutate Dexterity"), Dexterity->GetAmount(), DexterityBefore);
	if (IsValid(Frailty)) TestEqual(TEXT("Block preview does not mutate Frailty"), Frailty->GetAmount(), FrailtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmediatePreviewContributionOrderTest,
	"SlayTheSpireDemo.UIA3.ImmediatePreview.SupportedEffectsKeepDefinitionOrderAndUnsupportedEffectsStayAbsent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FImmediatePreviewContributionOrderTest::RunTest(const FString& Parameters)
{
	FPreviewFixture Fixture;
	if (!IsValid(Fixture.Player) || !IsValid(Fixture.Enemy))
	{
		AddError(TEXT("Failed to create combatants for immediate contribution-order test."));
		return false;
	}

	UDamageCardEffect* Damage = NewObject<UDamageCardEffect>(Fixture.World);
	Damage->BaseAmount = 3;
	Damage->DescriptionArgumentName = TEXT("Damage");
	UDrawCardEffect* UnsupportedDraw = NewObject<UDrawCardEffect>(Fixture.World);
	UGainBlockCardEffect* Block = NewObject<UGainBlockCardEffect>(Fixture.World);
	Block->BaseAmount = 4;
	Block->DescriptionArgumentName = TEXT("Block");

	FCardEffectPreviewContext Context;
	Context.Source = Fixture.Player;
	Context.Target = Fixture.Enemy;

	FImmediateCardPreview Preview;
	Preview.BattleId = 101;
	Preview.StateRevision = 202;
	Preview.CardRuntimeId = 303;
	Preview.SourcePresentationId = Fixture.Player->PresentationId;
	Preview.TargetPresentationId = Fixture.Enemy->PresentationId;

	const int32 PlayerHPBefore = Fixture.Player->HP;
	const int32 PlayerBlockBefore = Fixture.Player->Block;
	const int32 EnemyHPBefore = Fixture.Enemy->HP;
	const int32 EnemyBlockBefore = Fixture.Enemy->Block;

	Damage->BuildImmediatePreviewOperations(Context, 0, Preview.Operations);
	UnsupportedDraw->BuildImmediatePreviewOperations(Context, 1, Preview.Operations);
	Block->BuildImmediatePreviewOperations(Context, 2, Preview.Operations);

	TestEqual(TEXT("Only supported Effects contribute operations"), Preview.Operations.Num(), 2);
	if (Preview.Operations.Num() == 2)
	{
		TestEqual(TEXT("First supported operation retains EffectIndex 0"), Preview.Operations[0].EffectIndex, 0);
		TestEqual(TEXT("First supported operation is Damage"), Preview.Operations[0].Type, EImmediatePreviewOperationType::Damage);
		TestEqual(TEXT("Second supported operation skips unsupported index and retains EffectIndex 2"), Preview.Operations[1].EffectIndex, 2);
		TestEqual(TEXT("Second supported operation is Block"), Preview.Operations[1].Type, EImmediatePreviewOperationType::Block);
	}
	TestEqual(TEXT("DTO keeps BattleId"), Preview.BattleId, int64(101));
	TestEqual(TEXT("DTO keeps StateRevision"), Preview.StateRevision, int64(202));
	TestEqual(TEXT("DTO keeps CardRuntimeId"), Preview.CardRuntimeId, 303);
	TestEqual(TEXT("DTO keeps SourcePresentationId"), Preview.SourcePresentationId, Fixture.Player->PresentationId);
	TestEqual(TEXT("DTO keeps TargetPresentationId"), Preview.TargetPresentationId, Fixture.Enemy->PresentationId);
	TestFalse(TEXT("A3-2A does not fabricate EnergyAfter before A3-3"), Preview.bHasEnergyAfter);

	TestEqual(TEXT("Contribution pass does not mutate Player HP"), Fixture.Player->HP, PlayerHPBefore);
	TestEqual(TEXT("Contribution pass does not mutate Player Block"), Fixture.Player->Block, PlayerBlockBefore);
	TestEqual(TEXT("Contribution pass does not mutate Enemy HP"), Fixture.Enemy->HP, EnemyHPBefore);
	TestEqual(TEXT("Contribution pass does not mutate Enemy Block"), Fixture.Enemy->Block, EnemyBlockBefore);
	return true;
}

#endif
