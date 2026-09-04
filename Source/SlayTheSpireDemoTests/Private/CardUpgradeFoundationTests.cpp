#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Actions/UpgradeCardAction.h"
#include "Battle/BattleTextResolver.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/ApplyStatusCardEffect.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Cards/Effects/DrawCardEffect.h"
#include "Cards/Effects/GainBlockCardEffect.h"
#include "Presentation/PresentationCardSnapshotBuilder.h"

namespace CardUpgradeFoundation
{
	UCardData* MakeSingleConfigCard(UObject* Outer)
	{
		UCardData* Definition = NewObject<UCardData>(Outer);
		Definition->CardId = TEXT("UpgradeFixture");
		Definition->DisplayName = FText::FromString(TEXT("Shared Card Name"));
		Definition->Description = FText::FromString(TEXT("Draw {Draw}."));
		Definition->CardType = ECardType::Attack;
		Definition->TargetType = ECardTargetType::Enemy;
		Definition->BaseCost = 2;
		Definition->UpgradedCost = 1;
		Definition->DefaultDestination = ECardDestination::Discard;

		UDrawCardEffect* Draw = NewObject<UDrawCardEffect>(Definition);
		Draw->DrawCount = 1;
		Draw->UpgradedDrawCount = 2;
		Definition->Effects.Add(Draw);
		return Definition;
	}

	UCardInstance* MakeCard(
		UObject* Outer,
		UCardData* Definition,
		int32 RuntimeId = 1,
		bool bStartUpgraded = false)
	{
		UCardInstance* Card = NewObject<UCardInstance>(Outer);
		Card->Initialize(Definition, RuntimeId, bStartUpgraded);
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

	UCardData* Definition = MakeSingleConfigCard(GetTransientPackage());
	UCardInstance* Card = MakeCard(GetTransientPackage(), Definition);

	TestFalse(TEXT("Card starts unupgraded"), Card->IsUpgraded());
	TestTrue(TEXT("Valid ordinary card can upgrade"), Card->CanUpgrade());
	TestEqual(TEXT("Base display name"), Card->GetDisplayName().ToString(), FString(TEXT("Shared Card Name")));
	TestEqual(TEXT("Shared card type"), Card->GetCardType(), ECardType::Attack);
	TestEqual(TEXT("Shared target type"), Card->GetTargetType(), ECardTargetType::Enemy);
	TestEqual(TEXT("Base cost"), Card->GetCurrentCost(), 2);
	TestEqual(TEXT("Shared destination"), Card->ResolveDestination(), ECardDestination::Discard);
	if (!TestEqual(TEXT("Card has one shared Effect"), Card->GetEffects().Num(), 1)) return false;
	const UDrawCardEffect* SharedDraw = Cast<UDrawCardEffect>(Card->GetEffects()[0].Get());
	if (!TestNotNull(TEXT("Shared Draw effect"), SharedDraw)) return false;
	TestEqual(TEXT("Base Draw typed value"), SharedDraw->GetEffectiveDrawCount(false), 1);
	TestEqual(TEXT("Upgraded Draw typed value"), SharedDraw->GetEffectiveDrawCount(true), 2);

	if (!RunUpgradeThroughQueue(*this, Card)) return false;
	TestTrue(TEXT("Card becomes upgraded"), Card->IsUpgraded());
	TestFalse(TEXT("Normal card cannot upgrade twice"), Card->CanUpgrade());

	TestEqual(TEXT("Upgraded card keeps the same authored display name"), Card->GetDisplayName().ToString(), FString(TEXT("Shared Card Name")));
	TestEqual(TEXT("Card type remains shared after upgrade"), Card->GetCardType(), ECardType::Attack);
	TestEqual(TEXT("Target type remains shared after upgrade"), Card->GetTargetType(), ECardTargetType::Enemy);
	TestEqual(TEXT("Upgraded cost uses typed UpgradedCost"), Card->GetCurrentCost(), 1);
	TestEqual(TEXT("Destination remains shared after upgrade"), Card->ResolveDestination(), ECardDestination::Discard);
	if (!TestEqual(TEXT("Upgraded card still has one Effect"), Card->GetEffects().Num(), 1)) return false;
	TestTrue(TEXT("Upgrade keeps the exact same Effect object"), Card->GetEffects()[0].Get() == SharedDraw);
	TestEqual(TEXT("Shared Effect resolves upgraded Draw"), SharedDraw->GetEffectiveDrawCount(Card->IsUpgraded()), 2);

	if (!RunUpgradeThroughQueue(*this, Card)) return false;
	TestTrue(TEXT("Second attempt leaves card upgraded"), Card->IsUpgraded());

	UCardInstance* SpawnedUpgraded = MakeCard(GetTransientPackage(), Definition, 2, true);
	TestTrue(TEXT("Creation boundary can start an instance upgraded"), SpawnedUpgraded->IsUpgraded());
	TestFalse(TEXT("Creation-time upgraded card cannot normal-upgrade again"), SpawnedUpgraded->CanUpgrade());
	TestEqual(TEXT("Creation-time upgraded card uses UpgradedCost"), SpawnedUpgraded->GetCurrentCost(), 1);
	TestTrue(TEXT("Creation-time upgraded card uses the same shared Effect"), SpawnedUpgraded->GetEffects()[0].Get() == SharedDraw);

	UDamageCardEffect* Damage = NewObject<UDamageCardEffect>(GetTransientPackage());
	Damage->BaseAmount = 6;
	Damage->UpgradedAmount = 9;
	Damage->HitCount = 2;
	Damage->UpgradedHitCount = 3;
	TestEqual(TEXT("Damage helper resolves base amount"), Damage->GetEffectiveAmount(false), 6);
	TestEqual(TEXT("Damage helper resolves upgraded amount"), Damage->GetEffectiveAmount(true), 9);
	TestEqual(TEXT("Damage helper resolves base hit count"), Damage->GetEffectiveHitCount(false), 2);
	TestEqual(TEXT("Damage helper resolves upgraded hit count"), Damage->GetEffectiveHitCount(true), 3);

	UGainBlockCardEffect* Block = NewObject<UGainBlockCardEffect>(GetTransientPackage());
	Block->BaseAmount = 5;
	Block->UpgradedAmount = 8;
	TestEqual(TEXT("Block helper resolves base amount"), Block->GetEffectiveAmount(false), 5);
	TestEqual(TEXT("Block helper resolves upgraded amount"), Block->GetEffectiveAmount(true), 8);

	UApplyStatusCardEffect* Status = NewObject<UApplyStatusCardEffect>(GetTransientPackage());
	Status->Amount = 1;
	Status->UpgradedAmount = 2;
	TestEqual(TEXT("Status helper resolves base amount"), Status->GetEffectiveAmount(false), 1);
	TestEqual(TEXT("Status helper resolves upgraded amount"), Status->GetEffectiveAmount(true), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCardUpgradeEffectiveConsumersTest,
	"SlayTheSpireDemo.CardUpgrade.EffectiveConsumers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCardUpgradeEffectiveConsumersTest::RunTest(const FString& Parameters)
{
	using namespace CardUpgradeFoundation;

	UCardData* Definition = MakeSingleConfigCard(GetTransientPackage());
	UCardInstance* Card = MakeCard(GetTransientPackage(), Definition, 7);
	const UCardEffect* SharedEffectBeforeUpgrade = Card->GetEffects()[0].Get();

	TArray<FText> ValidationErrors;
	TestTrue(TEXT("Single Description/Effects configuration validates typed base and upgraded values"),
		FBattleTextResolver::ValidateCardDefinition(Definition, ValidationErrors));

	TestEqual(
		TEXT("Base text uses base typed Effect value"),
		FBattleTextResolver::ResolveCardDescription(Card, nullptr).ToString(),
		FString(TEXT("Draw 1.")));

	FPresentationCardSnapshot BaseSnapshot;
	TestTrue(TEXT("Base card snapshot freezes"), PresentationCardSnapshot::TryBuild(Card, nullptr, BaseSnapshot));
	TestEqual(TEXT("Base snapshot display"), BaseSnapshot.DisplayName.ToString(), FString(TEXT("Shared Card Name")));
	TestFalse(TEXT("Base snapshot is not upgraded"), BaseSnapshot.bUpgraded);
	TestEqual(TEXT("Base snapshot cost"), BaseSnapshot.Cost, 2);
	TestEqual(TEXT("Base snapshot shared type"), BaseSnapshot.CardType, ECardType::Attack);
	TestEqual(TEXT("Base snapshot shared target"), BaseSnapshot.TargetType, ECardTargetType::Enemy);
	TestEqual(TEXT("Base snapshot description"), BaseSnapshot.Description.ToString(), FString(TEXT("Draw 1.")));

	if (!RunUpgradeThroughQueue(*this, Card)) return false;

	TestTrue(TEXT("Upgrade keeps shared Effect identity"), Card->GetEffects()[0].Get() == SharedEffectBeforeUpgrade);
	TestEqual(
		TEXT("Upgraded text uses upgraded typed Effect value"),
		FBattleTextResolver::ResolveCardDescription(Card, nullptr).ToString(),
		FString(TEXT("Draw 2.")));

	FPresentationCardSnapshot UpgradedSnapshot;
	TestTrue(TEXT("Upgraded card snapshot freezes"), PresentationCardSnapshot::TryBuild(Card, nullptr, UpgradedSnapshot));
	TestEqual(TEXT("Runtime identity is unchanged by upgrade"), UpgradedSnapshot.RuntimeId, BaseSnapshot.RuntimeId);
	TestEqual(TEXT("Card identity is unchanged by upgrade"), UpgradedSnapshot.CardId, BaseSnapshot.CardId);
	TestEqual(TEXT("Snapshot keeps shared display name"), UpgradedSnapshot.DisplayName.ToString(), FString(TEXT("Shared Card Name")));
	TestTrue(TEXT("Snapshot freezes upgraded presentation state"), UpgradedSnapshot.bUpgraded);
	TestEqual(TEXT("Upgraded snapshot cost"), UpgradedSnapshot.Cost, 1);
	TestEqual(TEXT("Card type stays shared"), UpgradedSnapshot.CardType, ECardType::Attack);
	TestEqual(TEXT("Target type stays shared"), UpgradedSnapshot.TargetType, ECardTargetType::Enemy);
	TestEqual(TEXT("Upgraded snapshot description"), UpgradedSnapshot.Description.ToString(), FString(TEXT("Draw 2.")));
	return true;
}

#endif
