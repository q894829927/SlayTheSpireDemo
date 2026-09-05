#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Misc/DataValidation.h"
#include "Presentation/PresentationCardSnapshotBuilder.h"
#include "Presentation/PresentationCardView.h"
#include "Presentation/PresentationTypes.h"

namespace CFVCardMetadataContract
{
	UCardData* MakeCard(UObject* Outer, ECardRarity Rarity, ECardColor CardColor)
	{
		UCardData* Card = NewObject<UCardData>(Outer);
		Card->CardId = TEXT("CFVMetadataProbe");
		Card->DisplayName = FText::FromString(TEXT("CFV Metadata Probe"));
		Card->Description = FText::FromString(TEXT("Metadata probe."));
		Card->CardType = ECardType::Skill;
		Card->TargetType = ECardTargetType::Self;
		Card->BaseCost = 0;
		Card->UpgradedCost = 0;
		Card->Rarity = Rarity;
		Card->CardColor = CardColor;
		Card->DefaultDestination = ECardDestination::Discard;
		return Card;
	}

	bool ValidateCard(UCardData* Card)
	{
#if WITH_EDITOR
		FDataValidationContext Context;
		return IsValid(Card) && Card->IsDataValid(Context) != EDataValidationResult::Invalid;
#else
		return false;
#endif
	}
}

using namespace CFVCardMetadataContract;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCFVCardMetadataContractTest,
	"SlayTheSpireDemo.CFV.CardMetadataContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCFVCardMetadataContractTest::RunTest(const FString& Parameters)
{
	UCardData* Defaults = NewObject<UCardData>(GetTransientPackage());
	if (!TestNotNull(TEXT("Default card data exists"), Defaults))
	{
		return false;
	}
	TestEqual(TEXT("Rarity migration default is Common"), Defaults->Rarity, ECardRarity::Common);
	TestEqual(TEXT("CardColor migration default is Red"), Defaults->CardColor, ECardColor::Red);

	UCardInstance* EmptyInstance = NewObject<UCardInstance>(GetTransientPackage());
	if (!TestNotNull(TEXT("Empty runtime card exists"), EmptyInstance))
	{
		return false;
	}
	TestEqual(TEXT("Invalid Definition rarity fallback is Common"), EmptyInstance->GetRarity(), ECardRarity::Common);
	TestEqual(TEXT("Invalid Definition color fallback is Red"), EmptyInstance->GetCardColor(), ECardColor::Red);

	UCardData* Authored = MakeCard(GetTransientPackage(), ECardRarity::Rare, ECardColor::Green);
	if (!TestNotNull(TEXT("Authored card exists"), Authored))
	{
		return false;
	}
	TestTrue(TEXT("Legal authored metadata passes CardData validation"), ValidateCard(Authored));

	UCardInstance* Instance = NewObject<UCardInstance>(GetTransientPackage());
	if (!TestNotNull(TEXT("Runtime card exists"), Instance))
	{
		return false;
	}
	Instance->Initialize(Authored, 73, true);
	TestEqual(TEXT("Runtime getter reads authored rarity"), Instance->GetRarity(), ECardRarity::Rare);
	TestEqual(TEXT("Runtime getter reads authored color"), Instance->GetCardColor(), ECardColor::Green);

	FPresentationCardSnapshot Historical;
	if (!TestTrue(
		TEXT("Historical card snapshot builds"),
		PresentationCardSnapshot::TryBuild(Instance, nullptr, Historical)))
	{
		return false;
	}
	TestEqual(TEXT("Historical snapshot freezes rarity"), Historical.Rarity, ECardRarity::Rare);
	TestEqual(TEXT("Historical snapshot freezes color"), Historical.CardColor, ECardColor::Green);
	TestTrue(TEXT("Historical snapshot freezes upgraded state"), Historical.bUpgraded);

	const FBattleHUDCardView HistoricalView =
		PresentationCardView::MakePresentationOnlyCardView(Historical);
	TestEqual(TEXT("Historical mapper preserves rarity"), HistoricalView.Rarity, ECardRarity::Rare);
	TestEqual(TEXT("Historical mapper preserves color"), HistoricalView.CardColor, ECardColor::Green);
	TestTrue(TEXT("Historical mapper preserves upgraded state"), HistoricalView.bUpgraded);

#if WITH_EDITOR
	UCardData* InvalidRarity = MakeCard(
		GetTransientPackage(),
		static_cast<ECardRarity>(0xFE),
		ECardColor::Red);
	FDataValidationContext InvalidRarityContext;
	TestEqual(
		TEXT("Invalid authored rarity fails CardData validation"),
		InvalidRarity->IsDataValid(InvalidRarityContext),
		EDataValidationResult::Invalid);

	UCardData* InvalidColor = MakeCard(
		GetTransientPackage(),
		ECardRarity::Common,
		static_cast<ECardColor>(0xFE));
	FDataValidationContext InvalidColorContext;
	TestEqual(
		TEXT("Invalid authored CardColor fails CardData validation"),
		InvalidColor->IsDataValid(InvalidColorContext),
		EDataValidationResult::Invalid);
#endif

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("Formal-freeze world exists"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatant* Player = World->SpawnActor<ACombatant>(
		ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
	ACombatant* Enemy = World->SpawnActor<ACombatant>(
		ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
	ABattleManager* Battle = World->SpawnActor<ABattleManager>(
		ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);

	const bool bActorsValid = IsValid(Player) && IsValid(Enemy) && IsValid(Battle);
	if (!TestTrue(TEXT("Formal-freeze actors exist"), bActorsValid))
	{
		World->DestroyWorld(false);
		return false;
	}

	Player->MaxHP = 100;
	Enemy->MaxHP = 100;
	Player->PresentationId = TEXT("PlayerHero");
	Player->DisplayName = FText::FromString(TEXT("Ironclad"));
	Enemy->PresentationId = TEXT("EnemyPrimary");
	Enemy->DisplayName = FText::FromString(TEXT("Cultist"));
	Battle->Player = Player;
	Battle->Enemy = Enemy;
	Battle->OpeningHandDrawCount = 1;
	Battle->PlayerTurnDrawCount = 0;
	Battle->DeckDebugSeed = 1337;
	Battle->DebugStartingDeck.Add(MakeCard(World, ECardRarity::Uncommon, ECardColor::Blue));
	Battle->StartBattle();
	Battle->FlushScheduledReadStateReadyForTesting();

	FPresentationStateSnapshot Baseline;
	const bool bHasBaseline = Battle->TryGetLatestFrozenPresentationBaseline(Baseline);
	TestTrue(TEXT("Formal Hand frozen baseline exists"), bHasBaseline);
	if (bHasBaseline && TestEqual(TEXT("Formal Hand contains one card"), Baseline.HandCards.Num(), 1))
	{
		const FBattleHUDCardView& Frozen = Baseline.HandCards[0];
		TestEqual(TEXT("Formal Hand freeze preserves rarity"), Frozen.Rarity, ECardRarity::Uncommon);
		TestEqual(TEXT("Formal Hand freeze preserves color"), Frozen.CardColor, ECardColor::Blue);
		TestFalse(TEXT("Formal Hand card starts unupgraded"), Frozen.bUpgraded);
	}

	World->DestroyWorld(false);
	return true;
}

#endif
