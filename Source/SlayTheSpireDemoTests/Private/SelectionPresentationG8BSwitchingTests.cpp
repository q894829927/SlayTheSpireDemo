#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG8BTargetChoiceSwitchCardTest,
	"SlayTheSpireDemo.SelectionPresentation.G8B.TargetChoiceSwitchCardParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG8BTargetChoiceSwitchCardTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
	if (!TestNotNull(TEXT("World created"), World)) return false;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatant* Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, Params);
	ACombatant* Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), Params);
	ABattleManager* Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, Params);
	if (!TestNotNull(TEXT("Player created"), Player)
		|| !TestNotNull(TEXT("Enemy created"), Enemy)
		|| !TestNotNull(TEXT("Battle created"), Battle))
	{
		World->DestroyWorld(false);
		return false;
	}

	Player->MaxHP = 80;
	Enemy->MaxHP = 50;
	Player->PresentationId = TEXT("G8BSwitchPlayer");
	Enemy->PresentationId = TEXT("G8BSwitchEnemy");
	Player->DisplayName = FText::FromString(TEXT("Player"));
	Enemy->DisplayName = FText::FromString(TEXT("Enemy"));
	Battle->Player = Player;
	Battle->Enemy = Enemy;
	Battle->OpeningHandDrawCount = 2;
	Battle->PlayerTurnDrawCount = 0;
	Battle->EnemyTestAttackDamage = 0;

	auto MakeAttack = [World](const TCHAR* Id)
	{
		UCardData* Card = NewObject<UCardData>(World);
		Card->CardId = FName(Id);
		Card->DisplayName = FText::FromString(Id);
		Card->BaseCost = 0;
		Card->UpgradedCost = 0;
		Card->CardType = ECardType::Attack;
		Card->TargetType = ECardTargetType::Enemy;
		Card->DefaultDestination = ECardDestination::Discard;
		return Card;
	};
	Battle->DebugStartingDeck.Add(MakeAttack(TEXT("G8BSwitchA")));
	Battle->DebugStartingDeck.Add(MakeAttack(TEXT("G8BSwitchB")));
	Battle->StartBattle();
	Battle->FlushScheduledReadStateReadyForTesting();

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(World);
	if (!TestTrue(TEXT("Direct ViewModel initializes"), ViewModel->Initialize(Battle, false))
		|| !TestEqual(TEXT("Two cards are displayed"), ViewModel->HandCards.Num(), 2))
	{
		World->DestroyWorld(false);
		return false;
	}

	const int32 FirstRuntimeId = ViewModel->HandCards[0].RuntimeId;
	const int32 SecondRuntimeId = ViewModel->HandCards[1].RuntimeId;
	if (!TestTrue(TEXT("First Attack enters target choice"), ViewModel->SelectCardByRuntimeId(FirstRuntimeId)))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("First card selected"), ViewModel->SelectedCardRuntimeId, FirstRuntimeId);
	TestEqual(TEXT("First selection is ChoosingTarget"), ViewModel->InteractionState, EBattleHUDInteractionState::ChoosingTarget);
	TestFalse(TEXT("EndTurn disabled on first target choice"), ViewModel->bCanEndTurn);

	TestTrue(TEXT("Another legal Hand card can replace target choice"), ViewModel->SelectCardByRuntimeId(SecondRuntimeId));
	TestEqual(TEXT("Second card becomes selected"), ViewModel->SelectedCardRuntimeId, SecondRuntimeId);
	TestEqual(TEXT("Switch remains ChoosingTarget"), ViewModel->InteractionState, EBattleHUDInteractionState::ChoosingTarget);
	TestTrue(TEXT("Switched card rebuilds legal target surface"), ViewModel->LegalTargets.Num() > 0);
	TestFalse(TEXT("EndTurn remains disabled after switch"), ViewModel->bCanEndTurn);

	const FBattleHUDInteractionReadinessShadow Shadow = ViewModel->EvaluateInteractionReadinessShadow(nullptr);
	TestTrue(TEXT("Switched target-choice shadow remains ready"), Shadow.bReady);
	TestTrue(TEXT("Switched target-choice shadow matches baseline"), Shadow.bMatchesBaseline);

	ViewModel->Shutdown();
	World->DestroyWorld(false);
	return true;
}

#endif
