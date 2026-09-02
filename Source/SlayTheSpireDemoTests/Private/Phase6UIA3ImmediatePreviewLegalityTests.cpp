#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Battle/BattleImmediatePreview.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleReadSnapshot.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Engine/World.h"

namespace Phase6UIA3ImmediatePreviewLegality
{
	UCardData* MakeEnemyCard(UObject* Outer)
	{
		UCardData* Definition = NewObject<UCardData>(Outer);
		Definition->CardId = TEXT("A3_LegalityCard");
		Definition->DisplayName = FText::FromString(TEXT("A3 Legality Card"));
		Definition->TargetType = ECardTargetType::Enemy;
		Definition->BaseCost = 2;

		UDamageCardEffect* Damage = NewObject<UDamageCardEffect>(Definition);
		Damage->BaseAmount = 7;
		Damage->DescriptionArgumentName = TEXT("Damage");
		Definition->Effects.Add(Damage);
		return Definition;
	}

	struct FLegalityFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;

		FLegalityFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

			Player->PresentationId = TEXT("A3LegalityPlayer");
			Enemy->PresentationId = TEXT("A3LegalityEnemy");
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 1;
			Battle->PlayerTurnDrawCount = 0;
			Battle->MaxEnergy = 3;
			Battle->DebugStartingDeck.Add(MakeEnemyCard(World));
			Battle->StartBattle();
		}

		~FLegalityFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		bool IsReady() const
		{
			return IsValid(World)
				&& IsValid(Player)
				&& IsValid(Enemy)
				&& IsValid(Battle)
				&& IsValid(Battle->GetDeckRuntimeForTesting())
				&& IsValid(Battle->GetActionQueueForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}

		UCardInstance* GetCard() const
		{
			return IsReady() ? Battle->GetDeckRuntimeForTesting()->GetFirstHandCard() : nullptr;
		}
	};

	bool RequireReady(FAutomationTestBase& Test, const FLegalityFixture& Fixture)
	{
		if (!Fixture.IsReady() || !IsValid(Fixture.GetCard()))
		{
			Test.AddError(TEXT("Failed to create the transient A3-3 legality fixture."));
			return false;
		}
		return true;
	}
}

using namespace Phase6UIA3ImmediatePreviewLegality;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmediatePreviewPreTargetAndTargetLegalityTest,
	"SlayTheSpireDemo.UIA3.ImmediatePreviewLegality.PreTargetUsesPlayabilityAndBoundTargetUsesPlayCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FImmediatePreviewPreTargetAndTargetLegalityTest::RunTest(const FString& Parameters)
{
	FLegalityFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;

	UCardInstance* Card = Fixture.GetCard();
	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();

	const FGameplayValidationResult PreTargetValidation = Fixture.Battle->QueryCardPlayability(Card);
	TestTrue(TEXT("Enemy-target card is generally playable before target binding"), PreTargetValidation.bAllowed);
	TestEqual(TEXT("Pre-target Gameplay validation is Allowed"), PreTargetValidation.FailureReason, EGameplayRequestFailureReason::None);

	FImmediateCardPreview PreTarget;
	TestTrue(TEXT("Null target builds a coherent pre-target Preview"), Fixture.Battle->TryBuildImmediateCardPreview(Card, nullptr, PreTarget));
	TestEqual(TEXT("Pre-target Preview reuses QueryCardPlayability allowed bit"), PreTarget.Validation.bAllowed, PreTargetValidation.bAllowed);
	TestEqual(TEXT("Pre-target Preview reuses QueryCardPlayability reason"), PreTarget.Validation.FailureReason, PreTargetValidation.FailureReason);
	TestEqual(TEXT("Pre-target Preview has no concrete TargetPresentationId"), PreTarget.TargetPresentationId, NAME_None);
	TestEqual(TEXT("Pre-target Preview exposes current Energy"), PreTarget.EnergyBefore, 3);
	TestEqual(TEXT("Pre-target Preview exposes current effective cost"), PreTarget.EffectiveCost, 2);
	TestTrue(TEXT("Allowed pre-target Preview exposes EnergyAfter"), PreTarget.bHasEnergyAfter);
	TestEqual(TEXT("Allowed pre-target EnergyAfter is exact"), PreTarget.EnergyAfter, 1);
	TestEqual(TEXT("Target-specific Damage is omitted before target binding"), PreTarget.Operations.Num(), 0);

	const FGameplayValidationResult BoundValidation = Fixture.Battle->QueryPlayCard(Card, Fixture.Enemy);
	TestTrue(TEXT("Enemy is a legal bound target"), BoundValidation.bAllowed);

	FImmediateCardPreview Bound;
	TestTrue(TEXT("Concrete Enemy target builds target-bound Preview"), Fixture.Battle->TryBuildImmediateCardPreview(Card, Fixture.Enemy, Bound));
	TestEqual(TEXT("Target-bound Preview reuses QueryPlayCard allowed bit"), Bound.Validation.bAllowed, BoundValidation.bAllowed);
	TestEqual(TEXT("Target-bound Preview reuses QueryPlayCard reason"), Bound.Validation.FailureReason, BoundValidation.FailureReason);
	TestTrue(TEXT("Allowed target-bound Preview exposes EnergyAfter"), Bound.bHasEnergyAfter);
	TestEqual(TEXT("Target-bound EnergyAfter is exact"), Bound.EnergyAfter, 1);
	TestEqual(TEXT("Concrete target enables one Damage operation"), Bound.Operations.Num(), 1);

	const FGameplayValidationResult InvalidTargetValidation = Fixture.Battle->QueryPlayCard(Card, Fixture.Player);
	TestFalse(TEXT("Player is not a legal target for an Enemy-target card"), InvalidTargetValidation.bAllowed);
	TestEqual(TEXT("Gameplay reports InvalidTarget"), InvalidTargetValidation.FailureReason, EGameplayRequestFailureReason::InvalidTarget);

	FImmediateCardPreview InvalidTargetPreview;
	TestTrue(TEXT("Normal InvalidTarget rejection still builds a coherent DTO"), Fixture.Battle->TryBuildImmediateCardPreview(Card, Fixture.Player, InvalidTargetPreview));
	TestFalse(TEXT("Preview exposes target-bound rejection"), InvalidTargetPreview.Validation.bAllowed);
	TestEqual(TEXT("Preview preserves InvalidTarget reason"), InvalidTargetPreview.Validation.FailureReason, EGameplayRequestFailureReason::InvalidTarget);
	TestFalse(TEXT("Rejected target does not fabricate EnergyAfter"), InvalidTargetPreview.bHasEnergyAfter);
	TestEqual(TEXT("Unavailable EnergyAfter remains neutral"), InvalidTargetPreview.EnergyAfter, 0);

	FBattleReadSnapshot BeforeRequest;
	TestTrue(TEXT("Snapshot before rejected Request is available"), Fixture.Battle->TryBuildReadSnapshot(BeforeRequest));
	const int32 PendingBefore = Queue->GetPendingCount();
	const FGameplayRequestResult Request = Fixture.Battle->RequestPlayCard(Card, Fixture.Player);
	TestFalse(TEXT("Authoritative Request remains rejected for invalid target"), Request.IsAcceptedForResolution());
	TestEqual(TEXT("Request preserves InvalidTarget reason"), Request.FailureReason, EGameplayRequestFailureReason::InvalidTarget);
	TestEqual(TEXT("Rejected Request does not spend Energy"), Fixture.Battle->Energy, BeforeRequest.Energy);
	TestEqual(TEXT("Rejected Request does not enqueue Actions"), Queue->GetPendingCount(), PendingBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmediatePreviewInsufficientEnergyTest,
	"SlayTheSpireDemo.UIA3.ImmediatePreviewLegality.InsufficientEnergyHasNoEnergyAfterAndRequestStillRejects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FImmediatePreviewInsufficientEnergyTest::RunTest(const FString& Parameters)
{
	FLegalityFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;

	UCardInstance* Card = Fixture.GetCard();
	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	Fixture.Battle->Energy = 1;

	const FGameplayValidationResult PreTargetValidation = Fixture.Battle->QueryCardPlayability(Card);
	TestFalse(TEXT("Insufficient Energy rejects pre-target playability"), PreTargetValidation.bAllowed);
	TestEqual(TEXT("Pre-target reason is NotEnoughEnergy"), PreTargetValidation.FailureReason, EGameplayRequestFailureReason::NotEnoughEnergy);

	FImmediateCardPreview PreTarget;
	TestTrue(TEXT("Insufficient Energy still produces coherent pre-target DTO"), Fixture.Battle->TryBuildImmediateCardPreview(Card, nullptr, PreTarget));
	TestEqual(TEXT("Preview exposes current EnergyBefore"), PreTarget.EnergyBefore, 1);
	TestEqual(TEXT("Preview exposes current EffectiveCost"), PreTarget.EffectiveCost, 2);
	TestFalse(TEXT("Insufficient Energy has no EnergyAfter"), PreTarget.bHasEnergyAfter);
	TestEqual(TEXT("No fabricated negative EnergyAfter"), PreTarget.EnergyAfter, 0);
	TestEqual(TEXT("Preview preserves NotEnoughEnergy"), PreTarget.Validation.FailureReason, EGameplayRequestFailureReason::NotEnoughEnergy);

	const FGameplayValidationResult TargetValidation = Fixture.Battle->QueryPlayCard(Card, Fixture.Enemy);
	TestFalse(TEXT("Target-bound query remains rejected by insufficient Energy"), TargetValidation.bAllowed);
	TestEqual(TEXT("Target-bound query keeps NotEnoughEnergy"), TargetValidation.FailureReason, EGameplayRequestFailureReason::NotEnoughEnergy);

	FImmediateCardPreview Bound;
	TestTrue(TEXT("Target-bound insufficient-Energy DTO still builds"), Fixture.Battle->TryBuildImmediateCardPreview(Card, Fixture.Enemy, Bound));
	TestFalse(TEXT("Target-bound rejection has no EnergyAfter"), Bound.bHasEnergyAfter);
	TestEqual(TEXT("Target-bound rejection does not fabricate negative Energy"), Bound.EnergyAfter, 0);
	TestEqual(TEXT("Target-bound Preview matches Gameplay reason"), Bound.Validation.FailureReason, TargetValidation.FailureReason);

	const int32 PendingBefore = Queue->GetPendingCount();
	const int32 HandBefore = Fixture.Battle->GetDeckRuntimeForTesting()->GetHandCount();
	const FGameplayRequestResult Request = Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy);
	TestFalse(TEXT("Authoritative Request still rejects insufficient Energy"), Request.IsAcceptedForResolution());
	TestEqual(TEXT("Request preserves NotEnoughEnergy reason"), Request.FailureReason, EGameplayRequestFailureReason::NotEnoughEnergy);
	TestEqual(TEXT("Rejected Request does not spend Energy"), Fixture.Battle->Energy, 1);
	TestEqual(TEXT("Rejected Request does not enqueue Actions"), Queue->GetPendingCount(), PendingBefore);
	TestEqual(TEXT("Rejected Request leaves card in Hand"), Fixture.Battle->GetDeckRuntimeForTesting()->GetHandCount(), HandBefore);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
