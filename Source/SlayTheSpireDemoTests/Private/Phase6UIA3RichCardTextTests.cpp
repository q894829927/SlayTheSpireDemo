#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleTextResolver.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Cards/Effects/GainBlockCardEffect.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Modifiers/Block/BlockFlatAddModifier.h"
#include "Modifiers/Block/BlockRatioModifier.h"
#include "Modifiers/Damage/DamageFlatAddModifier.h"
#include "Modifiers/Damage/DamageRatioModifier.h"
#include "Modifiers/ModifierTypes.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"

namespace Phase6UIA3RichCardText
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		uint64 NextSequence = 1;

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
				SpawnParameters);
			if (IsValid(Player))
			{
				Player->InitializeCombatant();
			}
			if (IsValid(Enemy))
			{
				Enemy->InitializeCombatant();
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
			return IsValid(World) && IsValid(Player) && IsValid(Enemy);
		}

		void ApplyStatus(ACombatant* Target, UStatusData* Definition, int32 Amount = 1)
		{
			UStatusContainer* Container = IsValid(Target) ? Target->GetStatusContainer() : nullptr;
			if (!IsValid(Container) || !IsValid(Definition))
			{
				return;
			}
			bool bCreated = false;
			Container->ApplyStatus(Definition, Amount, NextSequence++, bCreated);
		}

		UCardInstance* MakeCard(UCardData* Definition, int32 RuntimeId = 1) const
		{
			UCardInstance* Card = IsValid(World) ? NewObject<UCardInstance>(World) : nullptr;
			if (IsValid(Card))
			{
				Card->Initialize(Definition, RuntimeId);
			}
			return Card;
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

	UStatusData* MakeDamageRatioStatus(UObject* Outer, const TCHAR* Id, int32 Numerator, int32 Denominator)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		Definition->StatusId = Id;
		UDamageRatioModifier* Modifier = NewObject<UDamageRatioModifier>(Definition);
		Modifier->Scope = EModifierScope::Source;
		Modifier->Phase = EDamageModifierPhase::SourceMultiplier;
		Modifier->Numerator = Numerator;
		Modifier->Denominator = Denominator;
		Definition->DamageModifiers.Add(Modifier);
		return Definition;
	}
}

using namespace Phase6UIA3RichCardText;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCurrentDamageRichStyleTest,
	"SlayTheSpireDemo.UIA3.RichCardTextBaseline.DamageTracksStrengthAndWeak",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCurrentDamageRichStyleTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!Fixture.IsReady())
	{
		AddError(TEXT("Failed to create RichText Damage fixture."));
		return false;
	}

	UCardData* Definition = NewObject<UCardData>(Fixture.World);
	Definition->CardId = TEXT("RichStrike");
	Definition->TargetType = ECardTargetType::Enemy;
	Definition->Description = FText::FromString(TEXT("Deal {Damage} damage."));
	UDamageCardEffect* Damage = NewObject<UDamageCardEffect>(Definition);
	Damage->BaseAmount = 6;
	Damage->DescriptionArgumentName = TEXT("Damage");
	Definition->Effects.Add(Damage);
	UCardInstance* Card = Fixture.MakeCard(Definition);
	if (!TestNotNull(TEXT("Damage card exists"), Card))
	{
		return false;
	}

	TestEqual(
		TEXT("Authored-base Damage has no comparison tag"),
		FBattleTextResolver::ResolveCardRichDescription(Card, Fixture.Player).ToString(),
		FString(TEXT("Deal 6 damage.")));

	Fixture.ApplyStatus(Fixture.Player, MakeDamageFlatStatus(Fixture.World, TEXT("Strength"), 1));
	TestEqual(
		TEXT("Strength marks only increased Damage value"),
		FBattleTextResolver::ResolveCardRichDescription(Card, Fixture.Player).ToString(),
		FString(TEXT("Deal <PreviewIncrease>7</> damage.")));

	Fixture.ApplyStatus(Fixture.Player, MakeDamageRatioStatus(Fixture.World, TEXT("Weak"), 3, 4));
	TestEqual(
		TEXT("Weak below authored base marks only decreased Damage value"),
		FBattleTextResolver::ResolveCardRichDescription(Card, Fixture.Player).ToString(),
		FString(TEXT("Deal <PreviewDecrease>5</> damage.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCurrentBlockRichStyleTest,
	"SlayTheSpireDemo.UIA3.RichCardTextBaseline.BlockTracksDexterityAndFrailty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FCurrentBlockRichStyleTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!Fixture.IsReady())
	{
		AddError(TEXT("Failed to create RichText Block fixture."));
		return false;
	}

	UCardData* Definition = NewObject<UCardData>(Fixture.World);
	Definition->CardId = TEXT("RichDefend");
	Definition->TargetType = ECardTargetType::Self;
	Definition->Description = FText::FromString(TEXT("Gain {Block} Block."));
	UGainBlockCardEffect* Block = NewObject<UGainBlockCardEffect>(Definition);
	Block->BaseAmount = 5;
	Block->DescriptionArgumentName = TEXT("Block");
	Definition->Effects.Add(Block);
	UCardInstance* Card = Fixture.MakeCard(Definition);
	if (!TestNotNull(TEXT("Block card exists"), Card))
	{
		return false;
	}

	TestEqual(
		TEXT("Authored-base Block has no comparison tag"),
		FBattleTextResolver::ResolveCardRichDescription(Card, Fixture.Player).ToString(),
		FString(TEXT("Gain 5 Block.")));

	UStatusData* Dexterity = NewObject<UStatusData>(Fixture.World);
	Dexterity->StatusId = TEXT("Dexterity");
	UBlockFlatAddModifier* DexterityModifier = NewObject<UBlockFlatAddModifier>(Dexterity);
	DexterityModifier->Value = 1;
	DexterityModifier->AmountMode = EModifierAmountMode::ScaleWithAmount;
	Dexterity->BlockModifiers.Add(DexterityModifier);
	Fixture.ApplyStatus(Fixture.Player, Dexterity);
	TestEqual(
		TEXT("Dexterity marks only increased Block value"),
		FBattleTextResolver::ResolveCardRichDescription(Card, Fixture.Player).ToString(),
		FString(TEXT("Gain <PreviewIncrease>6</> Block.")));

	UStatusData* Frailty = NewObject<UStatusData>(Fixture.World);
	Frailty->StatusId = TEXT("Frailty");
	UBlockRatioModifier* FrailtyModifier = NewObject<UBlockRatioModifier>(Frailty);
	FrailtyModifier->Numerator = 3;
	FrailtyModifier->Denominator = 4;
	Frailty->BlockModifiers.Add(FrailtyModifier);
	Fixture.ApplyStatus(Fixture.Player, Frailty);
	TestEqual(
		TEXT("Frailty below authored base marks only decreased Block value"),
		FBattleTextResolver::ResolveCardRichDescription(Card, Fixture.Player).ToString(),
		FString(TEXT("Gain <PreviewDecrease>4</> Block.")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
