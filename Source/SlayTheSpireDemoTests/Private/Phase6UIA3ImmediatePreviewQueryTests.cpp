#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/BattleActionQueue.h"
#include "Battle/BattleImmediatePreview.h"
#include "Battle/BattleManager.h"
#include "Battle/BattleReadSnapshot.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Cards/Effects/DrawCardEffect.h"
#include "Cards/Effects/GainBlockCardEffect.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Engine/World.h"

namespace Phase6UIA3ImmediatePreviewQuery
{
	UCardData* MakePreviewCard(UObject* Outer)
	{
		UCardData* Definition = NewObject<UCardData>(Outer);
		Definition->CardId = TEXT("A3_QueryCard");
		Definition->DisplayName = FText::FromString(TEXT("A3 Query Card"));
		Definition->TargetType = ECardTargetType::Enemy;
		Definition->BaseCost = 1;

		UDamageCardEffect* Damage = NewObject<UDamageCardEffect>(Definition);
		Damage->BaseAmount = 7;
		Damage->HitCount = 2;
		Damage->DescriptionArgumentName = TEXT("Damage");
		Definition->Effects.Add(Damage);

		UDrawCardEffect* UnsupportedDraw = NewObject<UDrawCardEffect>(Definition);
		UnsupportedDraw->DrawCount = 1;
		Definition->Effects.Add(UnsupportedDraw);

		UGainBlockCardEffect* Block = NewObject<UGainBlockCardEffect>(Definition);
		Block->BaseAmount = 5;
		Block->DescriptionArgumentName = TEXT("Block");
		Definition->Effects.Add(Block);

		return Definition;
	}

	struct FQueryFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UCardData* Definition = nullptr;

		FQueryFixture()
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
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle))
			{
				return;
			}

			Player->PresentationId = TEXT("A3PreviewPlayer");
			Enemy->PresentationId = TEXT("A3PreviewEnemy");
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 1;
			Battle->PlayerTurnDrawCount = 0;
			Battle->MaxEnergy = 3;
			Definition = MakePreviewCard(World);
			Battle->DebugStartingDeck.Add(Definition);
			Battle->StartBattle();
		}

		~FQueryFixture()
		{
			if (IsValid(World))
			{
				World->DestroyWorld(false);
			}
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

	bool RequireReady(FAutomationTestBase& Test, const FQueryFixture& Fixture)
	{
		if (!Fixture.IsReady() || !IsValid(Fixture.GetCard()))
		{
			Test.AddError(TEXT("Failed to create the transient A3-2B preview query fixture."));
			return false;
		}
		return true;
	}
}

using namespace Phase6UIA3ImmediatePreviewQuery;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmediatePreviewQueryIdentityAndOrderTest,
	"SlayTheSpireDemo.UIA3.ImmediatePreviewQuery.StampsCurrentIdentityAndKeepsDefinitionOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FImmediatePreviewQueryIdentityAndOrderTest::RunTest(const FString& Parameters)
{
	FQueryFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UCardInstance* Card = Fixture.GetCard();
	FBattleReadSnapshot Before;
	TestTrue(TEXT("Current read snapshot is available"), Fixture.Battle->TryBuildReadSnapshot(Before));

	FName ExpectedSourceId = NAME_None;
	FName ExpectedTargetId = NAME_None;
	TestTrue(
		TEXT("Current Player PresentationId resolves"),
		Fixture.Battle->TryResolveCombatantPresentationId(Fixture.Player, ExpectedSourceId)
	);
	TestTrue(
		TEXT("Current Enemy PresentationId resolves"),
		Fixture.Battle->TryResolveCombatantPresentationId(Fixture.Enemy, ExpectedTargetId)
	);

	FImmediateCardPreview Preview;
	TestTrue(
		TEXT("BattleManager builds a coherent current target-specific Preview"),
		Fixture.Battle->TryBuildImmediateCardPreview(Card, Fixture.Enemy, Preview)
	);

	TestEqual(TEXT("Preview stamps current BattleId"), Preview.BattleId, static_cast<int64>(Before.BattleId));
	TestEqual(TEXT("Preview stamps current StateRevision"), Preview.StateRevision, static_cast<int64>(Before.StateRevision));
	TestEqual(TEXT("Preview stamps Card RuntimeId"), Preview.CardRuntimeId, Card->GetRuntimeId());
	TestEqual(TEXT("Preview stamps canonical SourcePresentationId"), Preview.SourcePresentationId, ExpectedSourceId);
	TestEqual(TEXT("Preview stamps canonical TargetPresentationId"), Preview.TargetPresentationId, ExpectedTargetId);

	TestEqual(TEXT("Unsupported Draw contributes no fabricated operation"), Preview.Operations.Num(), 2);
	if (Preview.Operations.Num() == 2)
	{
		const FImmediatePreviewOperation& Damage = Preview.Operations[0];
		TestEqual(TEXT("Damage keeps definition EffectIndex 0"), Damage.EffectIndex, 0);
		TestEqual(TEXT("Damage keeps semantic name"), Damage.SemanticArgumentName, FName(TEXT("Damage")));
		TestEqual(TEXT("Damage operation type"), Damage.Type, EImmediatePreviewOperationType::Damage);
		TestEqual(TEXT("Damage current per-hit amount"), Damage.ResolvedAmount, 7);
		TestEqual(TEXT("Damage preserves authored HitCount"), Damage.HitCount, 2);

		const FImmediatePreviewOperation& Block = Preview.Operations[1];
		TestEqual(TEXT("Block keeps definition EffectIndex 2 across omitted Draw"), Block.EffectIndex, 2);
		TestEqual(TEXT("Block keeps semantic name"), Block.SemanticArgumentName, FName(TEXT("Block")));
		TestEqual(TEXT("Block operation type"), Block.Type, EImmediatePreviewOperationType::Block);
		TestEqual(TEXT("Block resolves self amount even with Enemy PreviewTarget"), Block.ResolvedAmount, 5);
		TestEqual(TEXT("Block uses one logical operation"), Block.HitCount, 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FImmediatePreviewQueryReadOnlyAndFailureTest,
	"SlayTheSpireDemo.UIA3.ImmediatePreviewQuery.IsDeterministicReadOnlyAndRejectsIncoherentInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FImmediatePreviewQueryReadOnlyAndFailureTest::RunTest(const FString& Parameters)
{
	FQueryFixture Fixture;
	if (!RequireReady(*this, Fixture))
	{
		return false;
	}

	UCardInstance* Card = Fixture.GetCard();
	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	UDeckRuntime* Deck = Fixture.Battle->GetDeckRuntimeForTesting();

	FBattleReadSnapshot Before;
	TestTrue(TEXT("Pre-query read snapshot is available"), Fixture.Battle->TryBuildReadSnapshot(Before));
	const int32 PendingBefore = Queue->GetPendingCount();
	const int32 ExecutedBefore = Queue->GetExecutedCountInResolution();
	const bool bBusyBefore = Queue->IsBusy();
	const int32 PlayerHPBefore = Fixture.Player->HP;
	const int32 PlayerBlockBefore = Fixture.Player->Block;
	const int32 EnemyHPBefore = Fixture.Enemy->HP;
	const int32 EnemyBlockBefore = Fixture.Enemy->Block;
	const int32 HandBefore = Deck->GetHandCount();
	const int32 DrawBefore = Deck->GetDrawCount();
	const int32 DiscardBefore = Deck->GetDiscardCount();
	const int32 ExhaustBefore = Deck->GetExhaustCount();

	FImmediateCardPreview First;
	FImmediateCardPreview Second;
	TestTrue(TEXT("First current-state Preview builds"), Fixture.Battle->TryBuildImmediateCardPreview(Card, Fixture.Enemy, First));
	TestTrue(TEXT("Repeated same-state Preview builds"), Fixture.Battle->TryBuildImmediateCardPreview(Card, Fixture.Enemy, Second));

	TestEqual(TEXT("Repeated Preview keeps BattleId"), Second.BattleId, First.BattleId);
	TestEqual(TEXT("Repeated Preview keeps StateRevision"), Second.StateRevision, First.StateRevision);
	TestEqual(TEXT("Repeated Preview keeps Card identity"), Second.CardRuntimeId, First.CardRuntimeId);
	TestEqual(TEXT("Repeated Preview keeps Source identity"), Second.SourcePresentationId, First.SourcePresentationId);
	TestEqual(TEXT("Repeated Preview keeps Target identity"), Second.TargetPresentationId, First.TargetPresentationId);
	TestEqual(TEXT("Repeated Preview keeps operation count"), Second.Operations.Num(), First.Operations.Num());
	if (First.Operations.Num() == Second.Operations.Num())
	{
		for (int32 Index = 0; Index < First.Operations.Num(); ++Index)
		{
			const FImmediatePreviewOperation& A = First.Operations[Index];
			const FImmediatePreviewOperation& B = Second.Operations[Index];
			TestEqual(FString::Printf(TEXT("Operation %d EffectIndex deterministic"), Index), B.EffectIndex, A.EffectIndex);
			TestEqual(FString::Printf(TEXT("Operation %d semantic name deterministic"), Index), B.SemanticArgumentName, A.SemanticArgumentName);
			TestEqual(FString::Printf(TEXT("Operation %d type deterministic"), Index), B.Type, A.Type);
			TestEqual(FString::Printf(TEXT("Operation %d amount deterministic"), Index), B.ResolvedAmount, A.ResolvedAmount);
			TestEqual(FString::Printf(TEXT("Operation %d hit count deterministic"), Index), B.HitCount, A.HitCount);
		}
	}

	FBattleReadSnapshot After;
	TestTrue(TEXT("Post-query read snapshot is available"), Fixture.Battle->TryBuildReadSnapshot(After));
	TestEqual(TEXT("Query does not advance BattleId"), After.BattleId, Before.BattleId);
	TestEqual(TEXT("Query does not advance StateRevision"), After.StateRevision, Before.StateRevision);
	TestEqual(TEXT("Query does not change BattleState"), After.BattleState, Before.BattleState);
	TestEqual(TEXT("Query does not change Energy"), After.Energy, Before.Energy);
	TestEqual(TEXT("Query does not mutate Player HP"), Fixture.Player->HP, PlayerHPBefore);
	TestEqual(TEXT("Query does not mutate Player Block"), Fixture.Player->Block, PlayerBlockBefore);
	TestEqual(TEXT("Query does not mutate Enemy HP"), Fixture.Enemy->HP, EnemyHPBefore);
	TestEqual(TEXT("Query does not mutate Enemy Block"), Fixture.Enemy->Block, EnemyBlockBefore);
	TestEqual(TEXT("Query does not enqueue Actions"), Queue->GetPendingCount(), PendingBefore);
	TestEqual(TEXT("Query does not execute Actions"), Queue->GetExecutedCountInResolution(), ExecutedBefore);
	TestEqual(TEXT("Query does not change Queue busy state"), Queue->IsBusy(), bBusyBefore);
	TestEqual(TEXT("Query does not mutate Hand"), Deck->GetHandCount(), HandBefore);
	TestEqual(TEXT("Query does not mutate DrawPile"), Deck->GetDrawCount(), DrawBefore);
	TestEqual(TEXT("Query does not mutate DiscardPile"), Deck->GetDiscardCount(), DiscardBefore);
	TestEqual(TEXT("Query does not mutate ExhaustPile"), Deck->GetExhaustCount(), ExhaustBefore);

	FImmediateCardPreview Failure;
	Failure.BattleId = 999;
	Failure.Operations.Add(FImmediatePreviewOperation{});
	TestFalse(TEXT("Null Card is an incoherent transport input"), Fixture.Battle->TryBuildImmediateCardPreview(nullptr, Fixture.Enemy, Failure));
	TestEqual(TEXT("Failed query clears stale BattleId output"), Failure.BattleId, int64(0));
	TestEqual(TEXT("Failed query clears stale Operations output"), Failure.Operations.Num(), 0);

	TestFalse(TEXT("Null Target is an incoherent transport input"), Fixture.Battle->TryBuildImmediateCardPreview(Card, nullptr, Failure));

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACombatant* ForeignTarget = Fixture.World->SpawnActor<ACombatant>(
		ACombatant::StaticClass(),
		FTransform(FVector(200.0, 0.0, 0.0)),
		SpawnParameters
	);
	if (!IsValid(ForeignTarget))
	{
		AddError(TEXT("Failed to create foreign target for incoherent target test."));
		return false;
	}
	ForeignTarget->InitializeCombatant();
	TestFalse(
		TEXT("A combatant outside the current battle cannot be identity-stamped"),
		Fixture.Battle->TryBuildImmediateCardPreview(Card, ForeignTarget, Failure)
	);

	UCardInstance* UninitializedCard = NewObject<UCardInstance>(Fixture.World);
	TestFalse(
		TEXT("Uninitialized CardInstance cannot build a coherent Preview"),
		Fixture.Battle->TryBuildImmediateCardPreview(UninitializedCard, Fixture.Enemy, Failure)
	);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
