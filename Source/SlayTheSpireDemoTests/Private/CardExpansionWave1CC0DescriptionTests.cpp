#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleTextResolver.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/SelectExhaustHandCardEffect.h"

namespace CardExpansionWave1CC0DescriptionTest
{
	UCardData* MakeCardDefinition(
		const TCHAR* CardId,
		const TCHAR* Description,
		USelectExhaustHandCardEffect*& OutEffect
	)
	{
		UCardData* Definition = NewObject<UCardData>();
		Definition->CardId = FName(CardId);
		Definition->Description = FText::FromString(Description);
		OutEffect = NewObject<USelectExhaustHandCardEffect>(Definition);
		Definition->Effects.Add(OutEffect);
		return Definition;
	}

	UCardInstance* MakeRuntimeCard(UCardData* Definition, int32 RuntimeId, bool bUpgraded)
	{
		UCardInstance* Card = NewObject<UCardInstance>();
		Card->Initialize(Definition, RuntimeId, bUpgraded);
		return Card;
	}
}

using namespace CardExpansionWave1CC0DescriptionTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0SelectExhaustLegacyDescriptionCompatibilityTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Description.LegacyOptOutCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0SelectExhaustLegacyDescriptionCompatibilityTest::RunTest(const FString& Parameters)
{
	USelectExhaustHandCardEffect* Effect = nullptr;
	UCardData* Definition = MakeCardDefinition(
		TEXT("C0DefaultSelectExhaustText"),
		TEXT("{ExhaustMode}{Exhaust}张牌。"),
		Effect
	);
	if (!TestNotNull(TEXT("Definition exists"), Definition)
		|| !TestNotNull(TEXT("Effect exists"), Effect))
	{
		return false;
	}

	TestEqual(
		TEXT("Count description argument has semantic default Exhaust"),
		Effect->DescriptionArgumentName,
		FName(TEXT("Exhaust"))
	);
	TestEqual(
		TEXT("Selection-mode description argument has semantic default ExhaustMode"),
		Effect->SelectionModeDescriptionArgumentName,
		FName(TEXT("ExhaustMode"))
	);

	TArray<FName> DeclaredNames;
	Effect->GetPreviewArgumentNames(DeclaredNames);
	TestEqual(TEXT("Default SelectExhaust declares two dynamic arguments"), DeclaredNames.Num(), 2);
	if (DeclaredNames.Num() == 2)
	{
		TestEqual(TEXT("Default count argument is Exhaust"), DeclaredNames[0], FName(TEXT("Exhaust")));
		TestEqual(TEXT("Default mode argument is ExhaustMode"), DeclaredNames[1], FName(TEXT("ExhaustMode")));
	}

	TArray<FText> ValidationErrors;
	TestTrue(
		TEXT("Default dynamic SelectExhaust text validates without manual argument-name setup"),
		FBattleTextResolver::ValidateCardDefinition(Definition, ValidationErrors)
	);

	UCardInstance* Card = MakeRuntimeCard(Definition, 1, false);
	TestEqual(
		TEXT("Default Player/1 description resolves from default argument names"),
		FBattleTextResolver::ResolveCardDescription(Card, nullptr).ToString(),
		FString(TEXT("消耗1张牌。"))
	);

	// Explicit None remains available only as an authored legacy opt-out. The
	// important C0 default contract is that a newly-created Effect never starts
	// with None for either DescriptionArgumentName field.
	Effect->DescriptionArgumentName = NAME_None;
	Effect->SelectionModeDescriptionArgumentName = NAME_None;
	Definition->Description = FText::FromString(TEXT("固定描述。"));
	DeclaredNames.Reset();
	Effect->GetPreviewArgumentNames(DeclaredNames);
	TestEqual(TEXT("Explicit legacy opt-out declares no dynamic arguments"), DeclaredNames.Num(), 0);
	ValidationErrors.Reset();
	TestTrue(
		TEXT("Explicit legacy opt-out remains valid when intentionally authored"),
		FBattleTextResolver::ValidateCardDefinition(Definition, ValidationErrors)
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0SelectExhaustBaseUpgradeDescriptionCountTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Description.BaseUpgradeCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0SelectExhaustBaseUpgradeDescriptionCountTest::RunTest(const FString& Parameters)
{
	USelectExhaustHandCardEffect* Effect = nullptr;
	UCardData* Definition = MakeCardDefinition(
		TEXT("C0DynamicSelectExhaustText"),
		TEXT("{ExhaustMode}{Exhaust}张牌。"),
		Effect
	);
	if (!TestNotNull(TEXT("Definition exists"), Definition)
		|| !TestNotNull(TEXT("Effect exists"), Effect))
	{
		return false;
	}

	Effect->BaseSelectionMode = ESelectExhaustSelectionMode::Random;
	Effect->BaseSelectionCount = 2;
	Effect->UpgradedSelectionMode = ESelectExhaustSelectionMode::Player;
	Effect->UpgradedSelectionCount = 3;

	TArray<FName> DeclaredNames;
	Effect->GetPreviewArgumentNames(DeclaredNames);
	TestEqual(TEXT("Dynamic SelectExhaust declares count and mode arguments"), DeclaredNames.Num(), 2);
	if (DeclaredNames.Num() == 2)
	{
		TestEqual(TEXT("Declared count argument is Exhaust"), DeclaredNames[0], FName(TEXT("Exhaust")));
		TestEqual(TEXT("Declared mode argument is ExhaustMode"), DeclaredNames[1], FName(TEXT("ExhaustMode")));
	}

	TArray<FText> ValidationErrors;
	TestTrue(
		TEXT("Dynamic SelectExhaust description validates"),
		FBattleTextResolver::ValidateCardDefinition(Definition, ValidationErrors)
	);

	UCardInstance* BaseCard = MakeRuntimeCard(Definition, 1, false);
	UCardInstance* UpgradedCard = MakeRuntimeCard(Definition, 2, true);
	TestEqual(
		TEXT("Base card description uses Chinese Random mode and BaseSelectionCount"),
		FBattleTextResolver::ResolveCardDescription(BaseCard, nullptr).ToString(),
		FString(TEXT("随机消耗2张牌。"))
	);
	TestEqual(
		TEXT("Upgraded card description uses Chinese Player mode and UpgradedSelectionCount"),
		FBattleTextResolver::ResolveCardDescription(UpgradedCard, nullptr).ToString(),
		FString(TEXT("消耗3张牌。"))
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWave1CC0SelectExhaustDeclaredArgumentMustBeUsedTest,
	"SlayTheSpireDemo.CardExpansion.Wave1CC0.Description.DeclaredArgumentMustBeUsed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FWave1CC0SelectExhaustDeclaredArgumentMustBeUsedTest::RunTest(const FString& Parameters)
{
	USelectExhaustHandCardEffect* Effect = nullptr;
	UCardData* Definition = MakeCardDefinition(
		TEXT("C0UnusedSelectExhaustText"),
		TEXT("消耗牌。"),
		Effect
	);
	if (!TestNotNull(TEXT("Definition exists"), Definition)
		|| !TestNotNull(TEXT("Effect exists"), Effect))
	{
		return false;
	}

	TArray<FText> ValidationErrors;
	TestFalse(
		TEXT("Default dynamic arguments must be referenced by the card description"),
		FBattleTextResolver::ValidateCardDefinition(Definition, ValidationErrors)
	);
	TestTrue(TEXT("Unused default dynamic arguments report validation errors"), ValidationErrors.Num() >= 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
