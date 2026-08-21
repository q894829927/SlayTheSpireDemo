#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/DamageAction.h"
#include "Actions/GainBlockAction.h"
#include "Battle/BattleManager.h"
#include "Cards/CardPlayContext.h"
#include "Cards/Effects/DamageCardEffect.h"
#include "Cards/Effects/GainBlockCardEffect.h"
#include "Combat/Combatant.h"
#include "Modifiers/Damage/DamageFlatAddModifier.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/BattlePresentationRecorder.h"
#include "Presentation/PresentationTypes.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Engine/World.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2BTest
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		TArray<FPresentationResolutionEnvelope> Deliveries;

		explicit FFixture(bool bEnablePresentation = true, int32 EnemyDamage = 0)
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

			Player->MaxHP = 100;
			Enemy->MaxHP = 100;
			Player->PresentationId = TEXT("PlayerHero");
			Enemy->PresentationId = TEXT("EnemyPrimary");
			Player->DisplayName = FText::FromString(TEXT("Player"));
			Enemy->DisplayName = FText::FromString(TEXT("Enemy"));

			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 0;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = EnemyDamage;
			Battle->PlayerTestAttackDamage = 6;
			Battle->PlayerTestBlockAmount = 4;
			Battle->bEnableCommittedPresentationRecording = bEnablePresentation;
			Battle->OnPresentationResolutionReady.AddLambda(
				[this](const FPresentationResolutionEnvelope& Envelope)
				{
					Deliveries.Add(Envelope);
				}
			);

			Battle->StartBattle();
			Flush();
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
			return IsValid(World)
				&& IsValid(Player)
				&& IsValid(Enemy)
				&& IsValid(Battle)
				&& IsValid(Battle->GetActionQueueForTesting())
				&& Battle->BattleState == EBattleState::PlayerTurn;
		}

		void Flush() const
		{
			if (IsValid(Battle))
			{
				Battle->FlushScheduledReadStateReadyForTesting();
			}
		}

		void ResetDeliveries()
		{
			Deliveries.Reset();
		}

		const FPresentationResolutionEnvelope* LastDelivery() const
		{
			return Deliveries.Num() > 0 ? &Deliveries.Last() : nullptr;
		}
	};

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (Fixture.IsReady())
		{
			return true;
		}
		Test.AddError(TEXT("Failed to create the Phase 6UI-A2B fixture."));
		return false;
	}

	void AssertDamageInvariant(FAutomationTestBase& Test, const FDamageCommitResult& Result, const TCHAR* Prefix)
	{
		Test.TestTrue(FString::Printf(TEXT("%s committed"), Prefix), Result.bCommitted);
		Test.TestEqual(
			FString::Printf(TEXT("%s BlockedDamage identity"), Prefix),
			Result.BlockedDamage,
			Result.BlockBefore - Result.BlockAfter
		);
		Test.TestEqual(
			FString::Printf(TEXT("%s HPDamage identity"), Prefix),
			Result.HPDamage,
			Result.HPBefore - Result.HPAfter
		);
		Test.TestTrue(FString::Printf(TEXT("%s non-negative blocked"), Prefix), Result.BlockedDamage >= 0);
		Test.TestTrue(FString::Printf(TEXT("%s non-negative HP damage"), Prefix), Result.HPDamage >= 0);
		Test.TestTrue(
			FString::Printf(TEXT("%s committed deltas do not exceed incoming damage"), Prefix),
			Result.BlockedDamage + Result.HPDamage <= Result.IncomingDamage
		);
	}

	FName ResolveId(ABattleManager* Battle, ACombatant* Combatant)
	{
		FName Id = NAME_None;
		if (IsValid(Battle) && IsValid(Combatant))
		{
			Battle->TryResolveCombatantPresentationId(Combatant, Id);
		}
		return Id;
	}

	bool RunSystemDamage(
		FFixture& Fixture,
		ACombatant* Source,
		ACombatant* Target,
		int32 BaseAmount,
		bool bSupplyIds = true
	)
	{
		if (!Fixture.Battle->BeginSystemPresentationResolutionForTesting())
		{
			return false;
		}

		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		UDamageAction* Action = NewObject<UDamageAction>(Queue);
		Action->Initialize(Source, Target, BaseAmount, EDamageKind::Attack);
		if (bSupplyIds)
		{
			Action->SetPresentationParticipantIds(
				IsValid(Source) ? ResolveId(Fixture.Battle, Source) : NAME_None,
				ResolveId(Fixture.Battle, Target)
			);
		}
		Action->SetPresentationRecordWriter(Fixture.Battle->GetActivePresentationRecordWriterForTesting());
		if (!Queue->AddToBack(Action) || !Queue->StartProcessing())
		{
			return false;
		}
		Fixture.Flush();
		return true;
	}

	bool RunSystemBlock(
		FFixture& Fixture,
		ACombatant* Source,
		ACombatant* Target,
		int32 BaseAmount,
		bool bSupplyIds = true
	)
	{
		if (!Fixture.Battle->BeginSystemPresentationResolutionForTesting())
		{
			return false;
		}

		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		UGainBlockAction* Action = NewObject<UGainBlockAction>(Queue);
		Action->Initialize(Source, Target, BaseAmount);
		if (bSupplyIds)
		{
			Action->SetPresentationParticipantIds(
				IsValid(Source) ? ResolveId(Fixture.Battle, Source) : NAME_None,
				ResolveId(Fixture.Battle, Target)
			);
		}
		Action->SetPresentationRecordWriter(Fixture.Battle->GetActivePresentationRecordWriterForTesting());
		if (!Queue->AddToBack(Action) || !Queue->StartProcessing())
		{
			return false;
		}
		Fixture.Flush();
		return true;
	}

	const FPresentationRecord* FindFirstRecord(
		const FPresentationResolutionEnvelope& Envelope,
		EBattlePresentationRecordType Type
	)
	{
		return Envelope.Records.FindByPredicate(
			[Type](const FPresentationRecord& Record)
			{
				return Record.Type == Type;
			}
		);
	}

	int32 CountRecords(
		const FPresentationResolutionEnvelope& Envelope,
		EBattlePresentationRecordType Type
	)
	{
		int32 Count = 0;
		for (const FPresentationRecord& Record : Envelope.Records)
		{
			if (Record.Type == Type)
			{
				++Count;
			}
		}
		return Count;
	}

	UStatusData* ApplyStrength(FFixture& Fixture, int32 Amount)
	{
		UStatusData* Strength = NewObject<UStatusData>(Fixture.World);
		Strength->StatusId = TEXT("A2BStrength");
		UDamageFlatAddModifier* Modifier = NewObject<UDamageFlatAddModifier>(Strength);
		Modifier->Scope = EModifierScope::Source;
		Modifier->ApplicableDamageKind = EDamageKind::Attack;
		Modifier->Value = 1;
		Modifier->AmountMode = EModifierAmountMode::ScaleWithAmount;
		Strength->DamageModifiers.Add(Modifier);

		bool bCreated = false;
		Fixture.Player->GetStatusContainer()->ApplyStatus(
			Strength,
			Amount,
			Fixture.Battle->AllocateRuntimeSequence(),
			bCreated
		);
		return Strength;
	}
}

using namespace Phase6UIA2BTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2BDamageCommitResultTest,
	"SlayTheSpireDemo.Phase6UIA2B.Commit.DamageResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2BDamageCommitResultTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;

	Fixture.Enemy->HP = 100;
	Fixture.Enemy->Block = 0;
	const FDamageCommitResult Normal = Fixture.Enemy->TakeCombatDamage(12);
	AssertDamageInvariant(*this, Normal, TEXT("Normal"));
	TestEqual(TEXT("Normal HP after"), Normal.HPAfter, 88);
	TestEqual(TEXT("Normal HP damage"), Normal.HPDamage, 12);

	Fixture.Enemy->HP = 100;
	Fixture.Enemy->Block = 5;
	const FDamageCommitResult Partial = Fixture.Enemy->TakeCombatDamage(12);
	AssertDamageInvariant(*this, Partial, TEXT("Partial block"));
	TestEqual(TEXT("Partial block after"), Partial.BlockAfter, 0);
	TestEqual(TEXT("Partial blocked damage"), Partial.BlockedDamage, 5);
	TestEqual(TEXT("Partial HP after"), Partial.HPAfter, 93);
	TestEqual(TEXT("Partial HP damage"), Partial.HPDamage, 7);

	Fixture.Enemy->HP = 100;
	Fixture.Enemy->Block = 20;
	const FDamageCommitResult Full = Fixture.Enemy->TakeCombatDamage(12);
	AssertDamageInvariant(*this, Full, TEXT("Full block"));
	TestEqual(TEXT("Full block remains"), Full.BlockAfter, 8);
	TestEqual(TEXT("Full blocked damage"), Full.BlockedDamage, 12);
	TestEqual(TEXT("Full block HP unchanged"), Full.HPAfter, 100);
	TestEqual(TEXT("Full block HP damage zero"), Full.HPDamage, 0);

	Fixture.Enemy->HP = 3;
	Fixture.Enemy->Block = 0;
	const FDamageCommitResult Overkill = Fixture.Enemy->TakeCombatDamage(10);
	AssertDamageInvariant(*this, Overkill, TEXT("Overkill"));
	TestEqual(TEXT("Overkill HP clamps to zero"), Overkill.HPAfter, 0);
	TestEqual(TEXT("Overkill reports actual HP delta"), Overkill.HPDamage, 3);
	TestTrue(TEXT("Overkill inequality can be strict"), Overkill.HPDamage < Overkill.IncomingDamage);

	const FDamageCommitResult DeadNoop = Fixture.Enemy->TakeCombatDamage(5);
	TestFalse(TEXT("Dead target damage does not commit"), DeadNoop.bCommitted);
	Fixture.Enemy->HP = 10;
	const FDamageCommitResult ZeroNoop = Fixture.Enemy->TakeCombatDamage(0);
	TestFalse(TEXT("Zero incoming damage does not commit"), ZeroNoop.bCommitted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2BBlockCommitResultTest,
	"SlayTheSpireDemo.Phase6UIA2B.Commit.BlockResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2BBlockCommitResultTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;

	Fixture.Player->Block = 2;
	const FBlockCommitResult Gain = Fixture.Player->GainBlock(5);
	TestTrue(TEXT("Positive GainBlock commits"), Gain.bCommitted);
	TestEqual(TEXT("Gain before"), Gain.BlockBefore, 2);
	TestEqual(TEXT("Gain after"), Gain.BlockAfter, 7);
	TestEqual(TEXT("Gain signed delta"), Gain.BlockDelta, 5);
	TestEqual(TEXT("Gain delta identity"), Gain.BlockDelta, Gain.BlockAfter - Gain.BlockBefore);

	const FBlockCommitResult Clear = Fixture.Player->ClearBlock();
	TestTrue(TEXT("Non-zero ClearBlock commits"), Clear.bCommitted);
	TestEqual(TEXT("Clear before"), Clear.BlockBefore, 7);
	TestEqual(TEXT("Clear after"), Clear.BlockAfter, 0);
	TestEqual(TEXT("Clear signed delta"), Clear.BlockDelta, -7);
	TestEqual(TEXT("Clear delta identity"), Clear.BlockDelta, Clear.BlockAfter - Clear.BlockBefore);

	const FBlockCommitResult ClearNoop = Fixture.Player->ClearBlock();
	TestFalse(TEXT("Zero ClearBlock is a no-op"), ClearNoop.bCommitted);
	const FBlockCommitResult ZeroGain = Fixture.Player->GainBlock(0);
	TestFalse(TEXT("Zero GainBlock is a no-op"), ZeroGain.bCommitted);
	Fixture.Player->HP = 0;
	const FBlockCommitResult DeadGain = Fixture.Player->GainBlock(5);
	TestFalse(TEXT("Dead target GainBlock is a no-op"), DeadGain.bCommitted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2BDamageRecordTest,
	"SlayTheSpireDemo.Phase6UIA2B.Record.Damage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2BDamageRecordTest::RunTest(const FString& Parameters)
{
	FFixture ModifierFixture;
	if (!RequireReady(*this, ModifierFixture)) return false;
	ModifierFixture.ResetDeliveries();
	ModifierFixture.Enemy->HP = 50;
	ModifierFixture.Enemy->Block = 3;
	ApplyStrength(ModifierFixture, 2);
	TestTrue(TEXT("Modifier damage resolution executes"), RunSystemDamage(ModifierFixture, ModifierFixture.Player, ModifierFixture.Enemy, 5));
	const FPresentationResolutionEnvelope* ModifierEnvelope = ModifierFixture.LastDelivery();
	if (!TestNotNull(TEXT("Modifier damage envelope"), ModifierEnvelope)) return false;
	const FPresentationRecord* ModifierDamage = FindFirstRecord(*ModifierEnvelope, EBattlePresentationRecordType::Damage);
	if (!TestNotNull(TEXT("Modifier damage record"), ModifierDamage)) return false;
	TestEqual(TEXT("Record stores modifier-resolved incoming damage"), ModifierDamage->Damage.IncomingDamage, 7);
	TestEqual(TEXT("Record stores source id"), ModifierDamage->Damage.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Record stores target id"), ModifierDamage->Damage.TargetPresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("Record stores block before"), ModifierDamage->Damage.BlockBefore, 3);
	TestEqual(TEXT("Record stores blocked amount"), ModifierDamage->Damage.BlockedDamage, 3);
	TestEqual(TEXT("Record stores actual HP damage"), ModifierDamage->Damage.HPDamage, 4);
	TestEqual(TEXT("Damage absorption has no duplicate BlockChanged"), CountRecords(*ModifierEnvelope, EBattlePresentationRecordType::BlockChanged), 0);

	FFixture FullBlockFixture;
	FullBlockFixture.ResetDeliveries();
	FullBlockFixture.Enemy->Block = 20;
	TestTrue(TEXT("Fully blocked damage resolution executes"), RunSystemDamage(FullBlockFixture, FullBlockFixture.Player, FullBlockFixture.Enemy, 12));
	const FPresentationResolutionEnvelope* FullBlockEnvelope = FullBlockFixture.LastDelivery();
	if (!TestNotNull(TEXT("Fully blocked envelope"), FullBlockEnvelope)) return false;
	const FPresentationRecord* FullBlockDamage = FindFirstRecord(*FullBlockEnvelope, EBattlePresentationRecordType::Damage);
	if (!TestNotNull(TEXT("Fully blocked record"), FullBlockDamage)) return false;
	TestEqual(TEXT("Fully blocked incoming"), FullBlockDamage->Damage.IncomingDamage, 12);
	TestEqual(TEXT("Fully blocked amount"), FullBlockDamage->Damage.BlockedDamage, 12);
	TestEqual(TEXT("Fully blocked HP damage"), FullBlockDamage->Damage.HPDamage, 0);
	TestEqual(TEXT("Fully blocked damage still has no BlockChanged duplicate"), CountRecords(*FullBlockEnvelope, EBattlePresentationRecordType::BlockChanged), 0);

	FFixture MultiFixture;
	MultiFixture.ResetDeliveries();
	MultiFixture.Enemy->HP = 20;
	TestTrue(TEXT("Multi-hit system Resolution begins"), MultiFixture.Battle->BeginSystemPresentationResolutionForTesting());
	UDamageCardEffect* MultiEffect = NewObject<UDamageCardEffect>(MultiFixture.World);
	MultiEffect->BaseAmount = 5;
	MultiEffect->HitCount = 2;
	FCardPlayContext MultiContext;
	MultiContext.Source = MultiFixture.Player;
	MultiContext.Target = MultiFixture.Enemy;
	MultiContext.ActionOuter = MultiFixture.Battle->GetActionQueueForTesting();
	MultiContext.SourcePresentationId = ResolveId(MultiFixture.Battle, MultiFixture.Player);
	MultiContext.TargetPresentationId = ResolveId(MultiFixture.Battle, MultiFixture.Enemy);
	MultiContext.PresentationRecordWriter = MultiFixture.Battle->GetActivePresentationRecordWriterForTesting();
	TArray<UBattleAction*> MultiActions;
	MultiEffect->BuildActions(MultiContext, MultiActions);
	for (UBattleAction* Action : MultiActions)
	{
		Action->SetPresentationRecordWriter(MultiContext.PresentationRecordWriter);
	}
	TestEqual(TEXT("HitCount two builds two actions"), MultiActions.Num(), 2);
	TestTrue(TEXT("Multi-hit batch inserts"), MultiFixture.Battle->GetActionQueueForTesting()->AddBatchToBackPreserveOrder(MultiActions));
	TestTrue(TEXT("Multi-hit batch executes"), MultiFixture.Battle->GetActionQueueForTesting()->StartProcessing());
	MultiFixture.Flush();
	const FPresentationResolutionEnvelope* MultiEnvelope = MultiFixture.LastDelivery();
	if (!TestNotNull(TEXT("Multi-hit envelope"), MultiEnvelope)) return false;
	TArray<const FPresentationRecord*> MultiDamage;
	for (const FPresentationRecord& Record : MultiEnvelope->Records)
	{
		if (Record.Type == EBattlePresentationRecordType::Damage) MultiDamage.Add(&Record);
	}
	TestEqual(TEXT("Living target produces two Damage records"), MultiDamage.Num(), 2);
	if (MultiDamage.Num() == 2)
	{
		TestEqual(TEXT("First hit HP 20 to 15"), MultiDamage[0]->Damage.HPBefore, 20);
		TestEqual(TEXT("First hit HP after"), MultiDamage[0]->Damage.HPAfter, 15);
		TestEqual(TEXT("Second hit begins from first final HP"), MultiDamage[1]->Damage.HPBefore, MultiDamage[0]->Damage.HPAfter);
		TestEqual(TEXT("Second hit HP after"), MultiDamage[1]->Damage.HPAfter, 10);
		TestTrue(TEXT("Multi-hit presentation sequence increases"), MultiDamage[1]->PresentationSequence > MultiDamage[0]->PresentationSequence);
	}

	FFixture LethalMultiFixture;
	LethalMultiFixture.ResetDeliveries();
	LethalMultiFixture.Enemy->HP = 4;
	TestTrue(TEXT("Lethal multi-hit Resolution begins"), LethalMultiFixture.Battle->BeginSystemPresentationResolutionForTesting());
	UDamageCardEffect* LethalEffect = NewObject<UDamageCardEffect>(LethalMultiFixture.World);
	LethalEffect->BaseAmount = 5;
	LethalEffect->HitCount = 2;
	FCardPlayContext LethalContext;
	LethalContext.Source = LethalMultiFixture.Player;
	LethalContext.Target = LethalMultiFixture.Enemy;
	LethalContext.ActionOuter = LethalMultiFixture.Battle->GetActionQueueForTesting();
	LethalContext.SourcePresentationId = ResolveId(LethalMultiFixture.Battle, LethalMultiFixture.Player);
	LethalContext.TargetPresentationId = ResolveId(LethalMultiFixture.Battle, LethalMultiFixture.Enemy);
	LethalContext.PresentationRecordWriter = LethalMultiFixture.Battle->GetActivePresentationRecordWriterForTesting();
	TArray<UBattleAction*> LethalActions;
	LethalEffect->BuildActions(LethalContext, LethalActions);
	for (UBattleAction* Action : LethalActions)
	{
		Action->SetPresentationRecordWriter(LethalContext.PresentationRecordWriter);
	}
	TestTrue(TEXT("Lethal multi-hit inserts"), LethalMultiFixture.Battle->GetActionQueueForTesting()->AddBatchToBackPreserveOrder(LethalActions));
	TestTrue(TEXT("Lethal multi-hit executes"), LethalMultiFixture.Battle->GetActionQueueForTesting()->StartProcessing());
	LethalMultiFixture.Flush();
	const FPresentationResolutionEnvelope* LethalEnvelope = LethalMultiFixture.LastDelivery();
	if (!TestNotNull(TEXT("Lethal multi-hit envelope"), LethalEnvelope)) return false;
	TestEqual(TEXT("Only first lethal hit records Damage"), CountRecords(*LethalEnvelope, EBattlePresentationRecordType::Damage), 1);
	TestEqual(TEXT("Lethal multi-hit records one Victory"), CountRecords(*LethalEnvelope, EBattlePresentationRecordType::Victory), 1);
	if (LethalEnvelope->Records.Num() == 2)
	{
		TestEqual(TEXT("Lethal history begins with Damage"), LethalEnvelope->Records[0].Type, EBattlePresentationRecordType::Damage);
		TestEqual(TEXT("Lethal history ends with Victory"), LethalEnvelope->Records[1].Type, EBattlePresentationRecordType::Victory);
	}

	FFixture ManagerFixture;
	ManagerFixture.ResetDeliveries();
	ManagerFixture.Battle->TestAttack();
	ManagerFixture.Flush();
	const FPresentationResolutionEnvelope* ManagerEnvelope = ManagerFixture.LastDelivery();
	if (!TestNotNull(TEXT("Manager TestAttack envelope"), ManagerEnvelope)) return false;
	const FPresentationRecord* ManagerDamage = FindFirstRecord(*ManagerEnvelope, EBattlePresentationRecordType::Damage);
	if (!TestNotNull(TEXT("Manager TestAttack Damage record"), ManagerDamage)) return false;
	TestEqual(TEXT("Manager producer source id"), ManagerDamage->Damage.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Manager producer target id"), ManagerDamage->Damage.TargetPresentationId, FName(TEXT("EnemyPrimary")));

	FFixture SystemFixture;
	SystemFixture.ResetDeliveries();
	TestTrue(TEXT("No-source system damage executes"), RunSystemDamage(SystemFixture, nullptr, SystemFixture.Enemy, 3));
	const FPresentationResolutionEnvelope* SystemEnvelope = SystemFixture.LastDelivery();
	if (!TestNotNull(TEXT("No-source envelope"), SystemEnvelope)) return false;
	const FPresentationRecord* SystemDamage = FindFirstRecord(*SystemEnvelope, EBattlePresentationRecordType::Damage);
	if (!TestNotNull(TEXT("No-source Damage record"), SystemDamage)) return false;
	TestTrue(TEXT("No-source Damage keeps SourcePresentationId=None"), SystemDamage->Damage.SourcePresentationId.IsNone());
	TestEqual(TEXT("No-source Damage still resolves Target"), SystemDamage->Damage.TargetPresentationId, FName(TEXT("EnemyPrimary")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2BBlockChangedRecordTest,
	"SlayTheSpireDemo.Phase6UIA2B.Record.BlockChanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2BBlockChangedRecordTest::RunTest(const FString& Parameters)
{
	FFixture CardFixture;
	if (!RequireReady(*this, CardFixture)) return false;
	CardFixture.ResetDeliveries();
	TestTrue(TEXT("Card block Resolution begins"), CardFixture.Battle->BeginSystemPresentationResolutionForTesting());
	UGainBlockCardEffect* Effect = NewObject<UGainBlockCardEffect>(CardFixture.World);
	Effect->BaseAmount = 5;
	FCardPlayContext Context;
	Context.Source = CardFixture.Player;
	Context.Target = CardFixture.Player;
	Context.ActionOuter = CardFixture.Battle->GetActionQueueForTesting();
	Context.SourcePresentationId = ResolveId(CardFixture.Battle, CardFixture.Player);
	Context.TargetPresentationId = Context.SourcePresentationId;
	Context.PresentationRecordWriter = CardFixture.Battle->GetActivePresentationRecordWriterForTesting();
	TArray<UBattleAction*> Actions;
	Effect->BuildActions(Context, Actions);
	for (UBattleAction* Action : Actions)
	{
		Action->SetPresentationRecordWriter(Context.PresentationRecordWriter);
	}
	TestEqual(TEXT("GainBlock effect builds one Action"), Actions.Num(), 1);
	TestTrue(TEXT("Card block batch inserts"), CardFixture.Battle->GetActionQueueForTesting()->AddBatchToBackPreserveOrder(Actions));
	TestTrue(TEXT("Card block batch executes"), CardFixture.Battle->GetActionQueueForTesting()->StartProcessing());
	CardFixture.Flush();
	const FPresentationResolutionEnvelope* CardEnvelope = CardFixture.LastDelivery();
	if (!TestNotNull(TEXT("Card block envelope"), CardEnvelope)) return false;
	const FPresentationRecord* CardBlock = FindFirstRecord(*CardEnvelope, EBattlePresentationRecordType::BlockChanged);
	if (!TestNotNull(TEXT("Card BlockChanged record"), CardBlock)) return false;
	TestEqual(TEXT("Block reason Gain"), CardBlock->BlockChanged.Reason, EBlockPresentationReason::Gain);
	TestEqual(TEXT("Block source id"), CardBlock->BlockChanged.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Block target id"), CardBlock->BlockChanged.TargetPresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Block delta"), CardBlock->BlockChanged.BlockDelta, 5);
	TestEqual(TEXT("Block delta identity"), CardBlock->BlockChanged.BlockDelta, CardBlock->BlockChanged.BlockAfter - CardBlock->BlockChanged.BlockBefore);
	TestEqual(TEXT("Block FinalSnapshot matches commit"), CardEnvelope->FinalSnapshot.Player.Block, CardBlock->BlockChanged.BlockAfter);

	FFixture ManagerFixture;
	ManagerFixture.ResetDeliveries();
	ManagerFixture.Battle->TestGainBlock();
	ManagerFixture.Flush();
	const FPresentationResolutionEnvelope* ManagerEnvelope = ManagerFixture.LastDelivery();
	if (!TestNotNull(TEXT("Manager block envelope"), ManagerEnvelope)) return false;
	const FPresentationRecord* ManagerBlock = FindFirstRecord(*ManagerEnvelope, EBattlePresentationRecordType::BlockChanged);
	if (!TestNotNull(TEXT("Manager BlockChanged record"), ManagerBlock)) return false;
	TestEqual(TEXT("Manager block source id"), ManagerBlock->BlockChanged.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Manager block target id"), ManagerBlock->BlockChanged.TargetPresentationId, FName(TEXT("PlayerHero")));

	FFixture SystemFixture;
	SystemFixture.ResetDeliveries();
	TestTrue(TEXT("No-source block executes"), RunSystemBlock(SystemFixture, nullptr, SystemFixture.Player, 3));
	const FPresentationResolutionEnvelope* SystemEnvelope = SystemFixture.LastDelivery();
	if (!TestNotNull(TEXT("No-source block envelope"), SystemEnvelope)) return false;
	const FPresentationRecord* SystemBlock = FindFirstRecord(*SystemEnvelope, EBattlePresentationRecordType::BlockChanged);
	if (!TestNotNull(TEXT("No-source BlockChanged record"), SystemBlock)) return false;
	TestTrue(TEXT("No-source block keeps Source=None"), SystemBlock->BlockChanged.SourcePresentationId.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2BTurnStartClearRecordTest,
	"SlayTheSpireDemo.Phase6UIA2B.Record.TurnStartClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2BTurnStartClearRecordTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(true, 0);
	if (!RequireReady(*this, Fixture)) return false;

	TestEqual(TEXT("BattleStart delivered one opening envelope"), Fixture.Deliveries.Num(), 1);
	if (Fixture.Deliveries.Num() == 1)
	{
		TestEqual(TEXT("BattleStart normalization emits no Block clear record"), CountRecords(Fixture.Deliveries[0], EBattlePresentationRecordType::BlockChanged), 0);
	}

	Fixture.ResetDeliveries();
	Fixture.Player->Block = 6;
	Fixture.Enemy->Block = 4;
	TestTrue(TEXT("EndTurn accepted"), Fixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	Fixture.Flush();
	const FPresentationResolutionEnvelope* Envelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("EndTurn envelope"), Envelope)) return false;

	TArray<const FPresentationRecord*> Clears;
	for (const FPresentationRecord& Record : Envelope->Records)
	{
		if (Record.Type == EBattlePresentationRecordType::BlockChanged
			&& Record.BlockChanged.Reason == EBlockPresentationReason::TurnStartClear)
		{
			Clears.Add(&Record);
		}
	}
	TestEqual(TEXT("Exactly two real turn-start clears record"), Clears.Num(), 2);
	if (Clears.Num() == 2)
	{
		TestTrue(TEXT("Enemy clear Source=None"), Clears[0]->BlockChanged.SourcePresentationId.IsNone());
		TestEqual(TEXT("Enemy clear target"), Clears[0]->BlockChanged.TargetPresentationId, FName(TEXT("EnemyPrimary")));
		TestEqual(TEXT("Enemy clear 4 to 0"), Clears[0]->BlockChanged.BlockBefore, 4);
		TestEqual(TEXT("Enemy clear after"), Clears[0]->BlockChanged.BlockAfter, 0);
		TestTrue(TEXT("Player clear Source=None"), Clears[1]->BlockChanged.SourcePresentationId.IsNone());
		TestEqual(TEXT("Player clear target"), Clears[1]->BlockChanged.TargetPresentationId, FName(TEXT("PlayerHero")));
		TestEqual(TEXT("Player clear 6 to 0"), Clears[1]->BlockChanged.BlockBefore, 6);
		TestEqual(TEXT("Player clear after"), Clears[1]->BlockChanged.BlockAfter, 0);
		TestTrue(TEXT("Macro turn-start clear sequence is deterministic"), Clears[1]->PresentationSequence > Clears[0]->PresentationSequence);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2BLethalOrderingTest,
	"SlayTheSpireDemo.Phase6UIA2B.Record.LethalOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2BLethalOrderingTest::RunTest(const FString& Parameters)
{
	FFixture VictoryFixture;
	if (!RequireReady(*this, VictoryFixture)) return false;
	VictoryFixture.ResetDeliveries();
	VictoryFixture.Enemy->HP = 5;
	TestTrue(TEXT("Lethal player Damage executes"), RunSystemDamage(VictoryFixture, VictoryFixture.Player, VictoryFixture.Enemy, 6));
	const FPresentationResolutionEnvelope* VictoryEnvelope = VictoryFixture.LastDelivery();
	if (!TestNotNull(TEXT("Victory envelope"), VictoryEnvelope)) return false;
	const FPresentationRecord* VictoryDamage = FindFirstRecord(*VictoryEnvelope, EBattlePresentationRecordType::Damage);
	const FPresentationRecord* VictoryRecord = FindFirstRecord(*VictoryEnvelope, EBattlePresentationRecordType::Victory);
	if (!TestNotNull(TEXT("Victory Damage record"), VictoryDamage) || !TestNotNull(TEXT("Victory terminal record"), VictoryRecord)) return false;
	TestTrue(TEXT("Damage sequence precedes Victory"), VictoryDamage->PresentationSequence < VictoryRecord->PresentationSequence);
	TestEqual(TEXT("Victory HP reaches zero"), VictoryDamage->Damage.HPAfter, 0);
	TestEqual(TEXT("Victory FinalSnapshot state"), VictoryEnvelope->FinalSnapshot.BattleState, EBattleState::Victory);
	TestEqual(TEXT("Victory FinalSnapshot enemy HP"), VictoryEnvelope->FinalSnapshot.Enemy.HP, 0);
	TestEqual(TEXT("Victory terminal emitted once"), CountRecords(*VictoryEnvelope, EBattlePresentationRecordType::Victory), 1);

	FFixture DefeatFixture(true, 6);
	DefeatFixture.ResetDeliveries();
	DefeatFixture.Player->HP = 5;
	TestTrue(TEXT("EndTurn accepted for lethal enemy attack"), DefeatFixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	DefeatFixture.Flush();
	const FPresentationResolutionEnvelope* DefeatEnvelope = DefeatFixture.LastDelivery();
	if (!TestNotNull(TEXT("Defeat envelope"), DefeatEnvelope)) return false;
	const FPresentationRecord* DefeatDamage = FindFirstRecord(*DefeatEnvelope, EBattlePresentationRecordType::Damage);
	const FPresentationRecord* DefeatRecord = FindFirstRecord(*DefeatEnvelope, EBattlePresentationRecordType::Defeat);
	if (!TestNotNull(TEXT("Defeat Damage record"), DefeatDamage) || !TestNotNull(TEXT("Defeat terminal record"), DefeatRecord)) return false;
	TestTrue(TEXT("Damage sequence precedes Defeat"), DefeatDamage->PresentationSequence < DefeatRecord->PresentationSequence);
	TestEqual(TEXT("Defeat FinalSnapshot state"), DefeatEnvelope->FinalSnapshot.BattleState, EBattleState::Defeat);
	TestEqual(TEXT("Defeat FinalSnapshot player HP"), DefeatEnvelope->FinalSnapshot.Player.HP, 0);
	TestEqual(TEXT("Defeat terminal emitted once"), CountRecords(*DefeatEnvelope, EBattlePresentationRecordType::Defeat), 1);

	FFixture TerminalFixture;
	TerminalFixture.Battle->BattleState = EBattleState::Defeat;
	TerminalFixture.Enemy->HP = 0;
	TerminalFixture.Battle->CheckBattleResultForTesting();
	TestEqual(TEXT("Defeat cannot switch to Victory"), TerminalFixture.Battle->BattleState, EBattleState::Defeat);
	TerminalFixture.Battle->BattleState = EBattleState::Victory;
	TerminalFixture.Enemy->HP = 100;
	TerminalFixture.Player->HP = 0;
	TerminalFixture.Battle->CheckBattleResultForTesting();
	TestEqual(TEXT("Victory cannot switch to Defeat"), TerminalFixture.Battle->BattleState, EBattleState::Victory);

	UBattlePresentationRecorder* Recorder = NewObject<UBattlePresentationRecorder>(TerminalFixture.World);
	const EBattlePresentationRecordType TerminalTypes[] = {
		EBattlePresentationRecordType::ResolutionFault,
		EBattlePresentationRecordType::Victory,
		EBattlePresentationRecordType::Defeat
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(TerminalTypes); ++Index)
	{
		Recorder->ResetForBattle(static_cast<uint64>(100 + Index));
		FPresentationRecordWriter Writer;
		TestTrue(TEXT("Terminal recorder Resolution begins"), Recorder->BeginResolution(EPresentationResolutionOrigin::System, Writer));
		FPresentationRecord Terminal;
		Terminal.Type = TerminalTypes[Index];
		TestTrue(TEXT("Terminal record appends"), Writer.Append(Terminal));
		FPresentationRecord AfterTerminal;
		AfterTerminal.Type = EBattlePresentationRecordType::Damage;
		TestFalse(TEXT("Ordinary append after terminal is rejected"), Writer.Append(AfterTerminal));
		TestFalse(TEXT("Append after terminal invalidates unpublished batch"), Recorder->IsActiveResolutionValid());
	}

	Recorder->ResetForBattle(200);
	FPresentationRecordWriter DuplicateWriter;
	TestTrue(TEXT("Duplicate terminal probe begins"), Recorder->BeginResolution(EPresentationResolutionOrigin::System, DuplicateWriter));
	FPresentationRecord FirstVictory;
	FirstVictory.Type = EBattlePresentationRecordType::Victory;
	TestTrue(TEXT("First Victory terminal appends"), DuplicateWriter.Append(FirstVictory));
	FPresentationRecord DuplicateVictory;
	DuplicateVictory.Type = EBattlePresentationRecordType::Victory;
	TestFalse(TEXT("Duplicate terminal is rejected"), DuplicateWriter.Append(DuplicateVictory));
	TestFalse(TEXT("Duplicate terminal invalidates batch"), Recorder->IsActiveResolutionValid());

	Recorder->ResetForBattle(201);
	FPresentationRecordWriter MixedWriter;
	TestTrue(TEXT("Mixed terminal probe begins"), Recorder->BeginResolution(EPresentationResolutionOrigin::System, MixedWriter));
	FPresentationRecord MixedVictory;
	MixedVictory.Type = EBattlePresentationRecordType::Victory;
	TestTrue(TEXT("Mixed first terminal appends"), MixedWriter.Append(MixedVictory));
	FPresentationRecord MixedDefeat;
	MixedDefeat.Type = EBattlePresentationRecordType::Defeat;
	TestFalse(TEXT("Victory plus Defeat in one Resolution is rejected"), MixedWriter.Append(MixedDefeat));
	TestFalse(TEXT("Mixed terminal invalidates batch"), Recorder->IsActiveResolutionValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2BDamageBlockPlaybackTest,
	"SlayTheSpireDemo.Phase6UIA2B.Playback.DamageBlockSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2BDamageBlockPlaybackTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;

	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(Fixture.World);
	TestTrue(TEXT("Presentation-owned ViewModel initializes"), ViewModel->Initialize(Fixture.Battle, true));
	UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(Fixture.World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(Fixture.World);
	TestTrue(TEXT("Controller initializes"), Controller->Initialize(Fixture.Battle, ViewModel, Widget));

	FPresentationStateSnapshot Baseline;
	if (!TestTrue(TEXT("Baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline))) return false;
	const int64 FirstResolutionId = static_cast<int64>(Fixture.Battle->GetLatestFrozenPresentationBaselineResolutionId() + 1);
	FPresentationResolutionEnvelope Envelope;
	Envelope.BattleId = Baseline.BattleId;
	Envelope.ResolutionId = FirstResolutionId;
	Envelope.Origin = EPresentationResolutionOrigin::System;
	Envelope.FinalStateRevision = Baseline.StateRevision;
	Envelope.FinalSnapshot = Baseline;
	Envelope.FinalSnapshot.Player.Block = 9;

	FPresentationRecord Damage;
	Damage.BattleId = Envelope.BattleId;
	Damage.ResolutionId = Envelope.ResolutionId;
	Damage.PresentationSequence = 1001;
	Damage.Type = EBattlePresentationRecordType::Damage;
	Envelope.Records.Add(Damage);
	FPresentationRecord Block;
	Block.BattleId = Envelope.BattleId;
	Block.ResolutionId = Envelope.ResolutionId;
	Block.PresentationSequence = 1002;
	Block.Type = EBattlePresentationRecordType::BlockChanged;
	Envelope.Records.Add(Block);

	Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
	TestEqual(TEXT("Damage is offered first"), Widget->PlayCallCount, 1);
	TestTrue(TEXT("Damage waits on accepted Blueprint playback"), Controller->IsWaitingForCompletionForTesting());
	const FPresentationPlaybackToken DamageToken = Controller->GetActivePlaybackTokenForTesting();
	TestEqual(TEXT("Damage token sequence"), DamageToken.PresentationSequence, int64(1001));

	Controller->NotifyPresentationFinished(DamageToken);
	TestEqual(TEXT("Block is offered after Damage completion"), Widget->PlayCallCount, 2);
	const FPresentationPlaybackToken BlockToken = Controller->GetActivePlaybackTokenForTesting();
	TestEqual(TEXT("Block token sequence"), BlockToken.PresentationSequence, int64(1002));
	Controller->NotifyPresentationFinished(DamageToken);
	TestTrue(TEXT("Stale duplicate Damage completion cannot finish Block"), Controller->IsWaitingForCompletionForTesting());
	TestTrue(TEXT("Block token remains active after stale callback"), Controller->GetActivePlaybackTokenForTesting() == BlockToken);
	Controller->NotifyPresentationFinished(BlockToken);
	TestFalse(TEXT("Envelope completes after Block"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Completed envelope applies FinalSnapshot"), ViewModel->Player.Block, 9);
	TestEqual(TEXT("Completed Resolution watermark"), Controller->GetLastCompletedResolutionIdForTesting(), FirstResolutionId);

	Widget->bAcceptAsyncPlayback = false;
	FPresentationResolutionEnvelope Fallback = Envelope;
	Fallback.ResolutionId = FirstResolutionId + 1;
	Fallback.FinalSnapshot.Player.Block = 11;
	Fallback.Records.SetNum(1);
	Fallback.Records[0].ResolutionId = Fallback.ResolutionId;
	Fallback.Records[0].PresentationSequence = 1003;
	Fallback.Records[0].Type = EBattlePresentationRecordType::Damage;
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(Fallback);
	TestFalse(TEXT("Blueprint false return completes immediately"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Fallback applies its FinalSnapshot"), ViewModel->Player.Block, 11);
	TestEqual(TEXT("Fallback advances Resolution watermark"), Controller->GetLastCompletedResolutionIdForTesting(), Fallback.ResolutionId);

	Widget->bAcceptAsyncPlayback = true;
	FPresentationResolutionEnvelope Skipped = Fallback;
	Skipped.ResolutionId = Fallback.ResolutionId + 1;
	Skipped.FinalSnapshot.Player.Block = 12;
	Skipped.Records[0].ResolutionId = Skipped.ResolutionId;
	Skipped.Records[0].PresentationSequence = 1004;
	Fixture.Battle->OnPresentationResolutionReady.Broadcast(Skipped);
	TestTrue(TEXT("Skip probe starts async playback"), Controller->IsWaitingForCompletionForTesting());
	const FPresentationPlaybackToken PreSkipToken = Controller->GetActivePlaybackTokenForTesting();
	Controller->SkipPresentation();
	TestFalse(TEXT("Skip ends playback wait"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Skip catches up to FinalSnapshot"), ViewModel->Player.Block, 12);
	Controller->NotifyPresentationFinished(PreSkipToken);
	TestEqual(TEXT("Old completion after Skip cannot roll display"), ViewModel->Player.Block, 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2BPresentationFailureIsolationTest,
	"SlayTheSpireDemo.Phase6UIA2B.Failure.PresentationDoesNotAffectGameplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2BPresentationFailureIsolationTest::RunTest(const FString& Parameters)
{
	FFixture NoHistoryFixture(false);
	if (!RequireReady(*this, NoHistoryFixture)) return false;
	const int32 NoHistoryHPBefore = NoHistoryFixture.Enemy->HP;
	NoHistoryFixture.Battle->TestAttack();
	NoHistoryFixture.Flush();
	TestEqual(TEXT("Recording-disabled Gameplay still commits Damage"), NoHistoryFixture.Enemy->HP, NoHistoryHPBefore - 6);
	TestTrue(TEXT("Recording-disabled mode is not PresentationUnavailable"), NoHistoryFixture.Battle->IsPresentationAvailable());
	TestFalse(TEXT("Recording-disabled Gameplay has no resolution fault"), NoHistoryFixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());
	TestEqual(TEXT("Recording-disabled mode publishes no historical Envelope"), NoHistoryFixture.Deliveries.Num(), 0);

	FFixture InvalidPayloadFixture;
	InvalidPayloadFixture.ResetDeliveries();
	const int32 InvalidPayloadHPBefore = InvalidPayloadFixture.Enemy->HP;
	TestTrue(TEXT("Invalid-payload Resolution begins"), InvalidPayloadFixture.Battle->BeginSystemPresentationResolutionForTesting());
	UBattleActionQueue* InvalidQueue = InvalidPayloadFixture.Battle->GetActionQueueForTesting();
	UDamageAction* InvalidAction = NewObject<UDamageAction>(InvalidQueue);
	InvalidAction->Initialize(InvalidPayloadFixture.Player, InvalidPayloadFixture.Enemy, 4, EDamageKind::Attack);
	InvalidAction->SetPresentationRecordWriter(InvalidPayloadFixture.Battle->GetActivePresentationRecordWriterForTesting());
	TestTrue(TEXT("Invalid-payload Action inserts"), InvalidQueue->AddToBack(InvalidAction));
	TestTrue(TEXT("Invalid-payload Action executes"), InvalidQueue->StartProcessing());
	InvalidPayloadFixture.Flush();
	TestEqual(TEXT("Invalid payload cannot undo committed Gameplay"), InvalidPayloadFixture.Enemy->HP, InvalidPayloadHPBefore - 4);
	TestFalse(TEXT("Invalid payload disables Presentation only"), InvalidPayloadFixture.Battle->IsPresentationAvailable());
	TestFalse(TEXT("Invalid payload does not create Gameplay ResolutionFault"), InvalidQueue->IsResolutionFaulted());
	TestEqual(TEXT("Invalid payload seals no partial Envelope"), InvalidPayloadFixture.Deliveries.Num(), 0);
	FPresentationStateSnapshot InvalidBaseline;
	TestTrue(TEXT("Invalid payload still freezes newest baseline"), InvalidPayloadFixture.Battle->TryGetLatestFrozenPresentationBaseline(InvalidBaseline));
	TestEqual(TEXT("Invalid payload baseline reflects committed HP"), InvalidBaseline.Enemy.HP, InvalidPayloadFixture.Enemy->HP);

	FFixture AppendFailureFixture;
	AppendFailureFixture.ResetDeliveries();
	const int32 AppendFailureHPBefore = AppendFailureFixture.Enemy->HP;
	TestTrue(TEXT("Append-failure Resolution begins"), AppendFailureFixture.Battle->BeginSystemPresentationResolutionForTesting());
	UBattlePresentationRecorder* Recorder = AppendFailureFixture.Battle->GetPresentationRecorderForTesting();
	if (!TestNotNull(TEXT("Append-failure Recorder"), Recorder)) return false;
	Recorder->SetForceNextAppendFailureForTesting(true);
	UBattleActionQueue* AppendQueue = AppendFailureFixture.Battle->GetActionQueueForTesting();
	UDamageAction* AppendAction = NewObject<UDamageAction>(AppendQueue);
	AppendAction->Initialize(AppendFailureFixture.Player, AppendFailureFixture.Enemy, 4, EDamageKind::Attack);
	AppendAction->SetPresentationParticipantIds(FName(TEXT("PlayerHero")), FName(TEXT("EnemyPrimary")));
	AppendAction->SetPresentationRecordWriter(AppendFailureFixture.Battle->GetActivePresentationRecordWriterForTesting());
	TestTrue(TEXT("Append-failure Action inserts"), AppendQueue->AddToBack(AppendAction));
	TestTrue(TEXT("Append-failure Action executes"), AppendQueue->StartProcessing());
	AppendFailureFixture.Flush();
	TestEqual(TEXT("Append failure cannot undo committed Gameplay"), AppendFailureFixture.Enemy->HP, AppendFailureHPBefore - 4);
	TestFalse(TEXT("Append failure disables Presentation only"), AppendFailureFixture.Battle->IsPresentationAvailable());
	TestFalse(TEXT("Append failure does not create Gameplay ResolutionFault"), AppendQueue->IsResolutionFaulted());
	TestEqual(TEXT("Append failure seals no partial Envelope"), AppendFailureFixture.Deliveries.Num(), 0);
	FPresentationStateSnapshot AppendBaseline;
	TestTrue(TEXT("Append failure still freezes newest baseline"), AppendFailureFixture.Battle->TryGetLatestFrozenPresentationBaseline(AppendBaseline));
	TestEqual(TEXT("Append failure baseline reflects committed HP"), AppendBaseline.Enemy.HP, AppendFailureFixture.Enemy->HP);
	return true;
}

#endif
