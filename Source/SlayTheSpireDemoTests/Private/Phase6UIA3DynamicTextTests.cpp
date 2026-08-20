#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/ApplyStatusAction.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleReadSnapshot.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/ApplyStatusCardEffect.h"
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
#include "Battle/BattleTextResolver.h"

namespace Phase6UIA3DynamicText
{
	struct FTextFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		uint64 NextSequence = 1;

		FTextFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			if (IsValid(Player)) Player->InitializeCombatant();
			if (IsValid(Enemy)) Enemy->InitializeCombatant();
		}

		~FTextFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		UStatusInstance* ApplyStatus(ACombatant* Target, UStatusData* Definition, int32 Amount)
		{
			UStatusContainer* Container = IsValid(Target) ? Target->GetStatusContainer() : nullptr;
			if (!IsValid(Container)) return nullptr;
			bool bCreated = false;
			return Container->ApplyStatus(Definition, Amount, NextSequence++, bCreated);
		}

		UCardInstance* MakeCard(UCardData* Definition, int32 RuntimeId = 1)
		{
			UCardInstance* Card = IsValid(World) ? NewObject<UCardInstance>(World) : nullptr;
			if (IsValid(Card)) Card->Initialize(Definition, RuntimeId);
			return Card;
		}
	};

	UStatusData* MakeDamageFlatStatus(UObject* Outer, const TCHAR* Id, int32 Value, FName ArgumentName = FName(TEXT("DamageBonus")))
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = Id;
		UDamageFlatAddModifier* Modifier = NewObject<UDamageFlatAddModifier>(Definition);
		Modifier->Value = Value;
		Modifier->AmountMode = EModifierAmountMode::ScaleWithAmount;
		Modifier->DescriptionArgumentName = ArgumentName;
		Definition->DamageModifiers.Add(Modifier);
		return Definition;
	}

	UStatusData* MakeDamageRatioStatus(
		UObject* Outer,
		const TCHAR* Id,
		EModifierScope Scope,
		EDamageModifierPhase Phase,
		int32 Numerator,
		int32 Denominator,
		FName ArgumentName = FName(TEXT("DamagePercent"))
	)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = Id;
		UDamageRatioModifier* Modifier = NewObject<UDamageRatioModifier>(Definition);
		Modifier->Scope = Scope;
		Modifier->Phase = Phase;
		Modifier->Numerator = Numerator;
		Modifier->Denominator = Denominator;
		Modifier->DescriptionArgumentName = ArgumentName;
		Definition->DamageModifiers.Add(Modifier);
		return Definition;
	}

	UCardData* MakeDamageCard(UObject* Outer, int32 BaseAmount, const FText& Description)
	{
		UCardData* Definition = NewObject<UCardData>(Outer);
		Definition->CardId = TEXT("Strike");
		Definition->TargetType = ECardTargetType::Enemy;
		Definition->Description = Description;
		UDamageCardEffect* Effect = NewObject<UDamageCardEffect>(Definition);
		Effect->BaseAmount = BaseAmount;
		Effect->DescriptionArgumentName = TEXT("Damage");
		Definition->Effects.Add(Effect);
		return Definition;
	}
}

using namespace Phase6UIA3DynamicText;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCardDamageSourceBaselineTest,
	"SlayTheSpireDemo.Phase6UIA3.DynamicText.CardDamageUsesSourcePipelineAndExcludesEnemyTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCardDamageSourceBaselineTest::RunTest(const FString& Parameters)
{
	FTextFixture Fixture;
	UCardInstance* Card = Fixture.MakeCard(MakeDamageCard(Fixture.World, 6, FText::FromString(TEXT("Deal {Damage} damage."))));
	TestEqual(TEXT("Base damage is formatted"), FBattleTextResolver::ResolveCardDescription(Card, Fixture.Player).ToString(), FString(TEXT("Deal 6 damage.")));

	UStatusData* Strength = MakeDamageFlatStatus(Fixture.World, TEXT("Strength"), 1);
	Fixture.ApplyStatus(Fixture.Player, Strength, 1);
	TestEqual(TEXT("Strength changes the card face through the real pipeline"), FBattleTextResolver::ResolveCardDescription(Card, Fixture.Player).ToString(), FString(TEXT("Deal 7 damage.")));

	UStatusData* Weak = MakeDamageRatioStatus(Fixture.World, TEXT("Weak"), EModifierScope::Source, EDamageModifierPhase::SourceMultiplier, 3, 4);
	Fixture.ApplyStatus(Fixture.Player, Weak, 1);
	TestEqual(TEXT("Weak applies after Strength with gameplay flooring"), FBattleTextResolver::ResolveCardDescription(Card, Fixture.Player).ToString(), FString(TEXT("Deal 5 damage.")));

	UStatusData* Vulnerable = MakeDamageRatioStatus(Fixture.World, TEXT("Vulnerable"), EModifierScope::Target, EDamageModifierPhase::TargetMultiplier, 3, 2);
	Fixture.ApplyStatus(Fixture.Enemy, Vulnerable, 1);
	TestEqual(TEXT("Enemy Vulnerable is excluded from the source-side card baseline"), FBattleTextResolver::ResolveCardDescription(Card, Fixture.Player).ToString(), FString(TEXT("Deal 5 damage.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCardBlockPipelineTest,
	"SlayTheSpireDemo.Phase6UIA3.DynamicText.CardBlockUsesSelfTargetPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCardBlockPipelineTest::RunTest(const FString& Parameters)
{
	FTextFixture Fixture;
	UCardData* Definition = NewObject<UCardData>(Fixture.World);
	Definition->CardId = TEXT("Defend");
	Definition->TargetType = ECardTargetType::Self;
	Definition->Description = FText::FromString(TEXT("Gain {Block} Block."));
	UGainBlockCardEffect* Effect = NewObject<UGainBlockCardEffect>(Definition);
	Effect->BaseAmount = 5;
	Definition->Effects.Add(Effect);
	UCardInstance* Card = Fixture.MakeCard(Definition);

	UStatusData* Dexterity = NewObject<UStatusData>(Fixture.World);
	Dexterity->StatusId = TEXT("Dexterity");
	UBlockFlatAddModifier* DexterityModifier = NewObject<UBlockFlatAddModifier>(Dexterity);
	DexterityModifier->Value = 1;
	Dexterity->BlockModifiers.Add(DexterityModifier);
	Fixture.ApplyStatus(Fixture.Player, Dexterity, 1);
	TestEqual(TEXT("Dexterity changes Block preview"), FBattleTextResolver::ResolveCardDescription(Card, Fixture.Player).ToString(), FString(TEXT("Gain 6 Block.")));

	UStatusData* Frailty = NewObject<UStatusData>(Fixture.World);
	Frailty->StatusId = TEXT("Frailty");
	UBlockRatioModifier* FrailtyModifier = NewObject<UBlockRatioModifier>(Frailty);
	FrailtyModifier->Numerator = 3;
	FrailtyModifier->Denominator = 4;
	Frailty->BlockModifiers.Add(FrailtyModifier);
	Fixture.ApplyStatus(Fixture.Player, Frailty, 1);
	TestEqual(TEXT("Frailty uses the real per-modifier floor"), FBattleTextResolver::ResolveCardDescription(Card, Fixture.Player).ToString(), FString(TEXT("Gain 4 Block.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMultipleEffectArgumentsTest,
	"SlayTheSpireDemo.Phase6UIA3.DynamicText.MultipleEffectsUseStableNamedArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FMultipleEffectArgumentsTest::RunTest(const FString& Parameters)
{
	FTextFixture Fixture;
	UCardData* Pommel = MakeDamageCard(Fixture.World, 9, FText::FromString(TEXT("Cost {Cost}. Deal {Damage} damage. Draw {Draw}.")));
	UDrawCardEffect* Draw = NewObject<UDrawCardEffect>(Pommel);
	Draw->DrawCount = 1;
	Pommel->Effects.Add(Draw);
	TestEqual(TEXT("Cost, Damage and Draw resolve independently"), FBattleTextResolver::ResolveCardDescription(Fixture.MakeCard(Pommel), Fixture.Player).ToString(), FString(TEXT("Cost 1. Deal 9 damage. Draw 1.")));

	UStatusData* Weak = NewObject<UStatusData>(Fixture.World);
	Weak->StatusId = TEXT("Weak");
	UStatusData* Vulnerable = NewObject<UStatusData>(Fixture.World);
	Vulnerable->StatusId = TEXT("Vulnerable");
	UCardData* Uppercut = MakeDamageCard(Fixture.World, 13, FText::FromString(TEXT("Deal {Damage}. Apply {Weak} Weak and {Vulnerable} Vulnerable.")));
	UApplyStatusCardEffect* ApplyWeak = NewObject<UApplyStatusCardEffect>(Uppercut);
	ApplyWeak->StatusDefinition = Weak;
	ApplyWeak->Amount = 2;
	ApplyWeak->DescriptionArgumentName = TEXT("Weak");
	Uppercut->Effects.Add(ApplyWeak);
	UApplyStatusCardEffect* ApplyVulnerable = NewObject<UApplyStatusCardEffect>(Uppercut);
	ApplyVulnerable->StatusDefinition = Vulnerable;
	ApplyVulnerable->Amount = 3;
	ApplyVulnerable->DescriptionArgumentName = TEXT("Vulnerable");
	Uppercut->Effects.Add(ApplyVulnerable);
	TestEqual(TEXT("Two status amounts do not overwrite each other"), FBattleTextResolver::ResolveCardDescription(Fixture.MakeCard(Uppercut, 2), Fixture.Player).ToString(), FString(TEXT("Deal 13. Apply 2 Weak and 3 Vulnerable.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStatusFlatValueTracksAmountTest,
	"SlayTheSpireDemo.Phase6UIA3.DynamicText.StatusFlatValueTracksRuntimeAmount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FStatusFlatValueTracksAmountTest::RunTest(const FString& Parameters)
{
	FTextFixture Fixture;
	UStatusData* Strength = MakeDamageFlatStatus(Fixture.World, TEXT("Strength"), 1);
	Strength->Description = FText::FromString(TEXT("Attack damage +{DamageBonus}."));
	UStatusInstance* Instance = Fixture.ApplyStatus(Fixture.Player, Strength, 1);
	TestEqual(TEXT("One Strength shows +1"), FBattleTextResolver::ResolveStatusDescription(Instance).ToString(), FString(TEXT("Attack damage +1.")));
	Fixture.ApplyStatus(Fixture.Player, Strength, 1);
	TestEqual(TEXT("Merged Strength shows +2 from the same runtime value"), FBattleTextResolver::ResolveStatusDescription(Instance).ToString(), FString(TEXT("Attack damage +2.")));

	UStatusData* Dexterity = NewObject<UStatusData>(Fixture.World);
	Dexterity->StatusId = TEXT("Dexterity");
	Dexterity->Description = FText::FromString(TEXT("Block +{BlockBonus}."));
	UBlockFlatAddModifier* Modifier = NewObject<UBlockFlatAddModifier>(Dexterity);
	Modifier->Value = 2;
	Dexterity->BlockModifiers.Add(Modifier);
	UStatusInstance* DexterityInstance = Fixture.ApplyStatus(Fixture.Player, Dexterity, 2);
	TestEqual(TEXT("Modifier Value and runtime Amount share the displayed result"), FBattleTextResolver::ResolveStatusDescription(DexterityInstance).ToString(), FString(TEXT("Block +4.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStatusRatioUsesDefinitionTest,
	"SlayTheSpireDemo.Phase6UIA3.DynamicText.StatusRatioPercentComesFromModifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FStatusRatioUsesDefinitionTest::RunTest(const FString& Parameters)
{
	FTextFixture Fixture;
	UStatusData* Weak = MakeDamageRatioStatus(Fixture.World, TEXT("Weak"), EModifierScope::Source, EDamageModifierPhase::SourceMultiplier, 3, 4, TEXT("DamageReductionPercent"));
	Weak->Description = FText::FromString(TEXT("Damage reduced {DamageReductionPercent}%."));
	TestEqual(TEXT("3/4 is displayed as 25 percent"), FBattleTextResolver::ResolveStatusDescription(Fixture.ApplyStatus(Fixture.Player, Weak, 3)).ToString(), FString(TEXT("Damage reduced 25%.")));

	UStatusData* Vulnerable = MakeDamageRatioStatus(Fixture.World, TEXT("Vulnerable"), EModifierScope::Target, EDamageModifierPhase::TargetMultiplier, 3, 2, TEXT("DamageIncreasePercent"));
	Vulnerable->Description = FText::FromString(TEXT("Damage increased {DamageIncreasePercent}%."));
	TestEqual(TEXT("3/2 is displayed as 50 percent"), FBattleTextResolver::ResolveStatusDescription(Fixture.ApplyStatus(Fixture.Enemy, Vulnerable, 1)).ToString(), FString(TEXT("Damage increased 50%.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPreviewIsReadOnlyTest,
	"SlayTheSpireDemo.Phase6UIA3.DynamicText.PreviewDoesNotCommitGameplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPreviewIsReadOnlyTest::RunTest(const FString& Parameters)
{
	FTextFixture Fixture;
	UStatusData* Strength = MakeDamageFlatStatus(Fixture.World, TEXT("Strength"), 1);
	UStatusInstance* Instance = Fixture.ApplyStatus(Fixture.Player, Strength, 2);
	UCardInstance* Card = Fixture.MakeCard(MakeDamageCard(Fixture.World, 6, FText::FromString(TEXT("Deal {Damage}."))));
	const int32 PlayerHP = Fixture.Player->HP;
	const int32 PlayerBlock = Fixture.Player->Block;
	const int32 EnemyHP = Fixture.Enemy->HP;
	const int32 StatusAmount = Instance->GetAmount();

	FBattleTextResolver::ResolveCardDescription(Card, Fixture.Player);
	FBattleTextResolver::ResolveStatusDescription(Instance);
	TestEqual(TEXT("Preview does not change Player HP"), Fixture.Player->HP, PlayerHP);
	TestEqual(TEXT("Preview does not change Player Block"), Fixture.Player->Block, PlayerBlock);
	TestEqual(TEXT("Preview does not change Enemy HP"), Fixture.Enemy->HP, EnemyHP);
	TestEqual(TEXT("Preview does not change Status Amount"), Instance->GetAmount(), StatusAmount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInvalidTemplateFailsSoftTest,
	"SlayTheSpireDemo.Phase6UIA3.DynamicText.InvalidArgumentsFailSoftAndInvalidateAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FInvalidTemplateFailsSoftTest::RunTest(const FString& Parameters)
{
	FTextFixture Fixture;
	UCardData* CardDefinition = MakeDamageCard(Fixture.World, 6, FText::FromString(TEXT("Deal {Damage}.")));
	UDamageCardEffect* Duplicate = NewObject<UDamageCardEffect>(CardDefinition);
	Duplicate->BaseAmount = 2;
	Duplicate->DescriptionArgumentName = TEXT("Damage");
	CardDefinition->Effects.Add(Duplicate);
	TArray<FText> Errors;
	TestFalse(TEXT("Duplicate arguments invalidate the CardData"), FBattleTextResolver::ValidateCardDefinition(CardDefinition, Errors));
	TestTrue(TEXT("Duplicate validation reports an error"), Errors.Num() > 0);
	AddExpectedErrorPlain(
		TEXT("[BattleText] Duplicate preview text argument 'Damage'."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	TestEqual(TEXT("Duplicate runtime arguments render safely"), FBattleTextResolver::ResolveCardDescription(Fixture.MakeCard(CardDefinition), Fixture.Player).ToString(), FString(TEXT("Deal ?.")));

	UStatusData* InvalidRatio = MakeDamageRatioStatus(Fixture.World, TEXT("InvalidRatio"), EModifierScope::Source, EDamageModifierPhase::SourceMultiplier, 3, 0);
	InvalidRatio->Description = FText::FromString(TEXT("Ratio {DamagePercent}%."));
	Errors.Reset();
	TestFalse(TEXT("Invalid denominator invalidates StatusData"), FBattleTextResolver::ValidateStatusDefinition(InvalidRatio, Errors));
	AddExpectedErrorPlain(
		TEXT("[BattleText] Invalid ratio for 'DamagePercent': 3/0."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	TestEqual(TEXT("Invalid denominator renders safely"), FBattleTextResolver::ResolveStatusDescription(Fixture.ApplyStatus(Fixture.Player, InvalidRatio, 1)).ToString(), FString(TEXT("Ratio ?%.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerFacingSnapshotRefreshTest,
	"SlayTheSpireDemo.Phase6UIA3.DynamicText.ReadyRevisionRefreshesCardDescription",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPlayerFacingSnapshotRefreshTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!IsValid(World))
	{
		AddError(TEXT("Failed to create test World."));
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatant* Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
	ACombatant* Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
	ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle))
	{
		AddError(TEXT("Failed to create battle actors."));
		World->DestroyWorld(false);
		return false;
	}

	UCardData* Strike = MakeDamageCard(World, 6, FText::FromString(TEXT("Deal {Damage} damage.")));
	UStatusData* Strength = MakeDamageFlatStatus(World, TEXT("Strength"), 1);

	Battle->Player = Player;
	Battle->Enemy = Enemy;
	Battle->OpeningHandDrawCount = 1;
	Battle->PlayerTurnDrawCount = 0;
	Battle->DebugStartingDeck.Add(Strike);
	Battle->StartBattle();
	Battle->FlushScheduledReadStateReadyForTesting();

	FBattleReadSnapshot Before;
	if (!Battle->TryBuildPlayerFacingReadSnapshot(Before) || Before.HandCards.Num() != 1)
	{
		AddError(TEXT("Opening player-facing snapshot did not contain exactly one Hand card."));
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Opening card description uses Base 6"), Before.HandCards[0].CurrentDescription.ToString(), FString(TEXT("Deal 6 damage.")));
	UBattleActionQueue* Queue = Battle->GetActionQueueForTesting();
	if (!IsValid(Queue))
	{
		AddError(TEXT("Battle did not expose a valid ActionQueue."));
		World->DestroyWorld(false);
		return false;
	}
	const int32 ExecutedBeforePreview = Queue->GetExecutedCountInResolution();
	FBattleReadSnapshot RepeatedRead;
	TestTrue(TEXT("Repeated preview snapshot is readable"), Battle->TryBuildPlayerFacingReadSnapshot(RepeatedRead));
	TestEqual(TEXT("Snapshot preview does not change Energy"), RepeatedRead.Energy, Before.Energy);
	TestEqual(TEXT("Snapshot preview does not change Hand"), RepeatedRead.HandCount, Before.HandCount);
	TestEqual(TEXT("Snapshot preview does not enqueue Actions"), Queue->GetPendingCount(), 0);
	TestEqual(TEXT("Snapshot preview does not execute Actions"), Queue->GetExecutedCountInResolution(), ExecutedBeforePreview);

	int32 ReadyCount = 0;
	Battle->OnReadStateReady.AddLambda([&ReadyCount](uint64, uint64) { ++ReadyCount; });
	UApplyStatusAction* Apply = NewObject<UApplyStatusAction>(Queue);
	Apply->Initialize(Battle, Player, Player, Strength, 1);
	TestTrue(TEXT("Status Action is queued"), Queue->AddToBack(Apply));
	TestTrue(TEXT("Status Action resolves"), Queue->StartProcessing());
	Battle->FlushScheduledReadStateReadyForTesting();

	FBattleReadSnapshot After;
	if (!Battle->TryBuildPlayerFacingReadSnapshot(After) || After.HandCards.Num() != 1)
	{
		AddError(TEXT("Post-status player-facing snapshot did not contain exactly one Hand card."));
		World->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Committed status resolution advances revision"), After.StateRevision > Before.StateRevision);
	TestEqual(TEXT("Ready publication occurs once"), ReadyCount, 1);
	TestEqual(TEXT("New revision rebuilds Strike as 7"), After.HandCards[0].CurrentDescription.ToString(), FString(TEXT("Deal 7 damage.")));

	UApplyStatusAction* ApplyAgain = NewObject<UApplyStatusAction>(Queue);
	ApplyAgain->Initialize(Battle, Player, Player, Strength, 1);
	TestTrue(TEXT("Second Status Action is queued"), Queue->AddToBack(ApplyAgain));
	TestTrue(TEXT("Second Status Action resolves"), Queue->StartProcessing());
	Battle->FlushScheduledReadStateReadyForTesting();

	FBattleReadSnapshot AfterSecond;
	if (!Battle->TryBuildPlayerFacingReadSnapshot(AfterSecond) || AfterSecond.HandCards.Num() != 1)
	{
		AddError(TEXT("Second post-status snapshot did not contain exactly one Hand card."));
		World->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Second status resolution advances revision again"), AfterSecond.StateRevision > After.StateRevision);
	TestEqual(TEXT("Second Ready publication occurs once"), ReadyCount, 2);
	TestEqual(TEXT("Strength Amount 2 rebuilds Strike as 8"), AfterSecond.HandCards[0].CurrentDescription.ToString(), FString(TEXT("Deal 8 damage.")));

	World->DestroyWorld(false);
	return true;
}

#endif
