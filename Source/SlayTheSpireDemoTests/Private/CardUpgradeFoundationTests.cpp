#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/UpgradeCardAction.h"
#include "Battle/BattleTextResolver.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DrawCardEffect.h"
#include "Presentation/PresentationCardSnapshotBuilder.h"

namespace CardUpgradeFoundation
{
	UCardData* MakeTwoConfigCard(UObject* Outer)
	{
		UCardData* Definition = NewObject<UCardData>(Outer);
		Definition->CardId = TEXT("UpgradeFixture");
		Definition->DisplayName = FText::FromString(TEXT("Shared Card Name"));
		Definition->Description = FText::FromString(TEXT("Draw {Draw}."));
		Definition->CardType = ECardType::Attack;
		Definition->TargetType = ECardTargetType::Enemy;
		Definition->BaseCost = 2;
		Definition->DefaultDestination = ECardDestination::Discard;

		UDrawCardEffect* BaseDraw = NewObject<UDrawCardEffect>(Definition);
		BaseDraw->DrawCount = 1;
		Definition->Effects.Add(BaseDraw);

		Definition->bHasUpgrade = true;
		Definition->Upgrade.Description = FText::FromString(TEXT("Draw {Draw}."));
		Definition->Upgrade.Cost = 1;
		Definition->Upgrade.DefaultDestination = ECardDestination::Exhaust;

		UDrawCardEffect* UpgradedDraw = NewObject<UDrawCardEffect>(Definition);
		UpgradedDraw->DrawCount = 2;
		Definition->Upgrade.Effects.Add(UpgradedDraw);
		return Definition;
	}

	UCardInstance* MakeCard(UObject* Outer, UCardData* Definition, int32 RuntimeId = 1)
	{
		UCardInstance* Card = NewObject<UCardInstance>(Outer);
		Card->Initialize(Definition, RuntimeId);
		return Card;
	}

	bool RunUpgradeThroughQueue(FAutomationTestBase& Test, UCardInstance* Card)
	{
		UBattleActionQueue* Queue = NewObject<UBattleActionQueue>(GetTransientPackage());
		UUpgradeCardAction* Action = NewObject<UUpgradeCardAction>(Queue);
		Action->Initialize(Card);
		if (!Test.TestTrue(TEXT("Upgrade action inserts into Queue"), Queue->AddToBack(Action)))
		{
			return false;
		}
		if (!Test.TestTrue(TEXT("Upgrade Queue starts"), Queue->StartProcessing()))
		{
			return false;
		}
		Test.TestTrue(TEXT("Upgrade action finishes"), Action->IsFinished());
		Test.TestFalse(TEXT("Upgrade does not fault Queue"), Queue->IsResolutionFaulted());
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCardUpgradeSingleConfigTest,
	"SlayTheSpireDemo.CardUpgrade.SingleConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCardUpgradeSingleConfigTest::RunTest(const FString& Parameters)
{
	using namespace CardUpgradeFoundation;

	UCardData* Definition = MakeTwoConfigCard(GetTransientPackage());
	UCardInstance* Card = MakeCard(GetTransientPackage(), Definition);

	TestFalse(TEXT("Card starts unupgraded"), Card->IsUpgraded());
	TestTrue(TEXT("Card with authored Upgrade config can upgrade"), Card->CanUpgrade());
	TestEqual(TEXT("Base display name"), Card->GetDisplayName().ToString(), FString(TEXT("Shared Card Name")));
	TestEqual(TEXT("Shared card type"), Card->GetCardType(), ECardType::Attack);
	TestEqual(TEXT("Shared target type"), Card->GetTargetType(), ECardTargetType::Enemy);
	TestEqual(TEXT("Base cost"), Card->GetCurrentCost(), 2);
	TestEqual(TEXT("Base destination"), Card->ResolveDestination(), ECardDestination::Discard);
	if (!TestEqual(TEXT("Base has one Effect"), Card->GetEffects().Num(), 1)) return false;
	const UDrawCardEffect* BaseDraw = Cast<UDrawCardEffect>(Card->GetEffects()[0].Get());
	if (!TestNotNull(TEXT("Base Draw effect"), BaseDraw)) return false;
	TestEqual(TEXT("Base Draw count"), BaseDraw->DrawCount, 1);

	if (!RunUpgradeThroughQueue(*this, Card)) return false;
	TestTrue(TEXT("Card becomes upgraded"), Card->IsUpgraded());
	TestFalse(TEXT("Normal card cannot upgrade twice"), Card->CanUpgrade());

	TestEqual(TEXT("Upgraded display derives plus suffix"), Card->GetDisplayName().ToString(), FString(TEXT("Shared Card Name+")));
	TestEqual(TEXT("Card type remains shared after upgrade"), Card->GetCardType(), ECardType::Attack);
	TestEqual(TEXT("Target type remains shared after upgrade"), Card->GetTargetType(), ECardTargetType::Enemy);

	TestEqual(TEXT("Upgraded cost"), Card->GetCurrentCost(), 1);
	TestEqual(TEXT("Upgraded destination"), Card->ResolveDestination(), ECardDestination::Exhaust);
	if (!TestEqual(TEXT("Upgraded has one Effect"), Card->GetEffects().Num(), 1)) return false;
	const UDrawCardEffect* UpgradedDraw = Cast<UDrawCardEffect>(Card->GetEffects()[0].Get());
	if (!TestNotNull(TEXT("Upgraded Draw effect"), UpgradedDraw)) return false;
	TestEqual(TEXT("Upgraded Draw count"), UpgradedDraw->DrawCount, 2);
	TestTrue(TEXT("Base and upgraded Effects are distinct authored objects"), BaseDraw != UpgradedDraw);

	if (!RunUpgradeThroughQueue(*this, Card)) return false;
	TestTrue(TEXT("Second attempt leaves card upgraded"), Card->IsUpgraded());

	UCardData* NoUpgradeDefinition = NewObject<UCardData>(GetTransientPackage());
	NoUpgradeDefinition->CardId = TEXT("NoUpgradeFixture");
	UCardInstance* NoUpgradeCard = MakeCard(GetTransientPackage(), NoUpgradeDefinition, 2);
	TestFalse(TEXT("Card without authored Upgrade config cannot upgrade"), NoUpgradeCard->CanUpgrade());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCardUpgradeEffectiveConsumersTest,
	"SlayTheSpireDemo.CardUpgrade.EffectiveConsumers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCardUpgradeEffectiveConsumersTest::RunTest(const FString& Parameters)
{
	using namespace CardUpgradeFoundation;

	UCardData* Definition = MakeTwoConfigCard(GetTransientPackage());
	UCardInstance* Card = MakeCard(GetTransientPackage(), Definition, 7);

	TArray<FText> ValidationErrors;
	TestTrue(TEXT("Base and upgraded authored configurations validate together"),
		FBattleTextResolver::ValidateCardDefinition(Definition, ValidationErrors));

	TestEqual(
		TEXT("Base text uses Base Effects"),
		FBattleTextResolver::ResolveCardDescription(Card, nullptr).ToString(),
		FString(TEXT("Draw 1.")));

	FPresentationCardSnapshot BaseSnapshot;
	TestTrue(TEXT("Base card snapshot freezes"), PresentationCardSnapshot::TryBuild(Card, nullptr, BaseSnapshot));
	TestEqual(TEXT("Base snapshot display"), BaseSnapshot.DisplayName.ToString(), FString(TEXT("Shared Card Name")));
	TestEqual(TEXT("Base snapshot cost"), BaseSnapshot.Cost, 2);
	TestEqual(TEXT("Base snapshot shared type"), BaseSnapshot.CardType, ECardType::Attack);
	TestEqual(TEXT("Base snapshot shared target"), BaseSnapshot.TargetType, ECardTargetType::Enemy);
	TestEqual(TEXT("Base snapshot description"), BaseSnapshot.Description.ToString(), FString(TEXT("Draw 1.")));

	if (!RunUpgradeThroughQueue(*this, Card)) return false;

	TestEqual(
		TEXT("Upgraded text uses Upgrade Effects"),
		FBattleTextResolver::ResolveCardDescription(Card, nullptr).ToString(),
		FString(TEXT("Draw 2.")));

	FPresentationCardSnapshot UpgradedSnapshot;
	TestTrue(TEXT("Upgraded card snapshot freezes"), PresentationCardSnapshot::TryBuild(Card, nullptr, UpgradedSnapshot));
	TestEqual(TEXT("Runtime identity is unchanged by upgrade"), UpgradedSnapshot.RuntimeId, BaseSnapshot.RuntimeId);
	TestEqual(TEXT("Card identity is unchanged by upgrade"), UpgradedSnapshot.CardId, BaseSnapshot.CardId);
	TestEqual(TEXT("Snapshot derives upgraded plus suffix"), UpgradedSnapshot.DisplayName.ToString(), FString(TEXT("Shared Card Name+")));
	TestEqual(TEXT("Upgraded snapshot cost"), UpgradedSnapshot.Cost, 1);
	TestEqual(TEXT("Card type stays shared"), UpgradedSnapshot.CardType, ECardType::Attack);
	TestEqual(TEXT("Target type stays shared"), UpgradedSnapshot.TargetType, ECardTargetType::Enemy);
	TestEqual(TEXT("Upgraded snapshot description"), UpgradedSnapshot.Description.ToString(), FString(TEXT("Draw 2.")));
	return true;
}

#endif
