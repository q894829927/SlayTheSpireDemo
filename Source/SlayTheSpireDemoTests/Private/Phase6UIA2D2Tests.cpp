#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/ApplyStatusAction.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/ReduceStatusAction.h"
#include "Actions/RemoveStatusAction.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Engine/World.h"
#include "Events/TurnEndStatusDecayTrigger.h"
#include "Presentation/PresentationTypes.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Status/StatusInstance.h"

namespace Phase6UIA2D2Test
{
	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		TArray<FPresentationResolutionEnvelope> Deliveries;

		explicit FFixture(int32 EnemyDamage = 0)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

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
			Battle->bEnableCommittedPresentationRecording = true;
			Battle->OnPresentationResolutionReady.AddLambda([this](const FPresentationResolutionEnvelope& Envelope) { Deliveries.Add(Envelope); });
			Battle->StartBattle();
			Flush();
			Deliveries.Reset();
		}

		~FFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		bool IsReady() const
		{
			return IsValid(World) && IsValid(Player) && IsValid(Enemy) && IsValid(Battle)
				&& IsValid(Battle->GetActionQueueForTesting()) && Battle->BattleState == EBattleState::PlayerTurn;
		}

		void Flush() const
		{
			if (IsValid(Battle)) Battle->FlushScheduledReadStateReadyForTesting();
		}

		void ResetDeliveries() { Deliveries.Reset(); }
		const FPresentationResolutionEnvelope* LastDelivery() const { return Deliveries.Num() > 0 ? &Deliveries.Last() : nullptr; }
	};

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (Fixture.IsReady()) return true;
		Test.AddError(TEXT("Failed to create the Phase 6UI-A2D2 status presentation fixture."));
		return false;
	}

	UStatusData* CreateStatusDefinition(UObject* Outer, const TCHAR* StatusId, const TCHAR* DisplayName, const TCHAR* Description)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		if (!IsValid(Definition)) return nullptr;
		Definition->StatusId = FName(StatusId);
		Definition->DisplayName = FText::FromString(DisplayName);
		Definition->Description = FText::FromString(Description);
		return Definition;
	}

	const FPresentationRecord* FindStatusRecord(const FPresentationResolutionEnvelope& Envelope)
	{
		return Envelope.Records.FindByPredicate([](const FPresentationRecord& Record) { return Record.Type == EBattlePresentationRecordType::StatusChanged; });
	}

	int32 CountStatusRecords(const FPresentationResolutionEnvelope& Envelope)
	{
		int32 Count = 0;
		for (const FPresentationRecord& Record : Envelope.Records) if (Record.Type == EBattlePresentationRecordType::StatusChanged) ++Count;
		return Count;
	}

	bool RunSystemAction(FFixture& Fixture, UBattleAction* Action)
	{
		if (!IsValid(Action) || !Fixture.Battle->BeginSystemPresentationResolutionForTesting()) return false;
		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		Action->SetPresentationRecordWriter(Fixture.Battle->GetActivePresentationRecordWriterForTesting());
		if (!Queue->AddToBack(Action) || !Queue->StartProcessing()) return false;
		Fixture.Flush();
		return true;
	}
}

using namespace Phase6UIA2D2Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase6UIA2D2ApplyFreezeTest, "SlayTheSpireDemo.Phase6UIA2D2.Record.ApplyFreeze", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPhase6UIA2D2ApplyFreezeTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;
	UStatusData* Weak = CreateStatusDefinition(Fixture.World, TEXT("Weak"), TEXT("Weak A"), TEXT("Weak amount {Amount}."));
	if (!TestNotNull(TEXT("Weak definition"), Weak)) return false;
	Weak->IconRegion.bUseAtlasIcon = true;
	Weak->IconRegion.UVOffset = FVector2D(0.25, 0.5);
	Weak->IconRegion.UVScale = FVector2D(0.125, 0.25);
	Weak->IconRegion.TrimOffset = FVector2D(0.1, 0.2);
	Weak->IconRegion.TrimScale = FVector2D(0.8, 0.7);

	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	UApplyStatusAction* CreateAction = NewObject<UApplyStatusAction>(Queue);
	CreateAction->Initialize(Fixture.Battle, Fixture.Player, Fixture.Enemy, Weak, 2);
	TestTrue(TEXT("Applied status action resolves"), RunSystemAction(Fixture, CreateAction));
	const FPresentationResolutionEnvelope* CreateEnvelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Applied status envelope"), CreateEnvelope)) return false;
	TestEqual(TEXT("Create produces exactly one StatusChanged record"), CountStatusRecords(*CreateEnvelope), 1);
	const FPresentationRecord* AppliedRecord = FindStatusRecord(*CreateEnvelope);
	if (!TestNotNull(TEXT("Applied StatusChanged record"), AppliedRecord)) return false;
	const FStatusChangedPresentationPayload& Applied = AppliedRecord->StatusChanged;
	TestEqual(TEXT("Applied source id"), Applied.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Applied target id"), Applied.TargetPresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("Applied status id"), Applied.StatusId, FName(TEXT("Weak")));
	TestTrue(TEXT("Applied RuntimeSequence is positive"), Applied.RuntimeSequence > 0);
	TestEqual(TEXT("Applied amount before"), Applied.AmountBefore, 0);
	TestEqual(TEXT("Applied amount after"), Applied.AmountAfter, 2);
	TestTrue(TEXT("Applied marks creation"), Applied.bCreated);
	TestFalse(TEXT("Applied is not removal"), Applied.bRemoved);
	TestTrue(TEXT("Applied reason"), Applied.Reason == EStatusChangeReason::Applied);
	TestEqual(TEXT("Applied display name frozen"), Applied.DisplayName.ToString(), FString(TEXT("Weak A")));
	TestTrue(TEXT("Applied before description is empty"), Applied.DescriptionBefore.IsEmpty());
	TestEqual(TEXT("Applied after description uses amount two"), Applied.DescriptionAfter.ToString(), FString(TEXT("Weak amount 2.")));
	TestTrue(TEXT("Atlas flag frozen"), Applied.bUseAtlasIcon);
	TestTrue(TEXT("Atlas UVOffset frozen"), Applied.UVOffset.Equals(FVector2D(0.25, 0.5)));
	TestTrue(TEXT("Atlas UVScale frozen"), Applied.UVScale.Equals(FVector2D(0.125, 0.25)));
	TestTrue(TEXT("Atlas TrimOffset frozen"), Applied.TrimOffset.Equals(FVector2D(0.1, 0.2)));
	TestTrue(TEXT("Atlas TrimScale frozen"), Applied.TrimScale.Equals(FVector2D(0.8, 0.7)));

	const int64 OriginalRuntimeSequence = Applied.RuntimeSequence;
	Fixture.ResetDeliveries();
	UStatusData* AlternateWeak = CreateStatusDefinition(Fixture.World, TEXT("Weak"), TEXT("Wrong Definition"), TEXT("WRONG amount {Amount}."));
	if (!TestNotNull(TEXT("Alternate Weak definition"), AlternateWeak)) return false;
	AlternateWeak->IconRegion.bUseAtlasIcon = false;
	UApplyStatusAction* IncreaseAction = NewObject<UApplyStatusAction>(Queue);
	IncreaseAction->Initialize(Fixture.Battle, Fixture.Player, Fixture.Enemy, AlternateWeak, 3);
	TestTrue(TEXT("Merge status action resolves"), RunSystemAction(Fixture, IncreaseAction));
	const FPresentationResolutionEnvelope* IncreaseEnvelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Increased status envelope"), IncreaseEnvelope)) return false;
	TestEqual(TEXT("Merge produces exactly one StatusChanged record"), CountStatusRecords(*IncreaseEnvelope), 1);
	const FPresentationRecord* IncreasedRecord = FindStatusRecord(*IncreaseEnvelope);
	if (!TestNotNull(TEXT("Increased StatusChanged record"), IncreasedRecord)) return false;
	const FStatusChangedPresentationPayload& Increased = IncreasedRecord->StatusChanged;
	TestTrue(TEXT("Merge reason is Increased"), Increased.Reason == EStatusChangeReason::Increased);
	TestFalse(TEXT("Merge is not a new instance"), Increased.bCreated);
	TestFalse(TEXT("Merge is not removal"), Increased.bRemoved);
	TestEqual(TEXT("Merge retains existing RuntimeSequence"), Increased.RuntimeSequence, OriginalRuntimeSequence);
	TestEqual(TEXT("Merge amount before"), Increased.AmountBefore, 2);
	TestEqual(TEXT("Merge amount after"), Increased.AmountAfter, 5);
	TestEqual(TEXT("Merge freezes true pre-mutation description"), Increased.DescriptionBefore.ToString(), FString(TEXT("Weak amount 2.")));
	TestEqual(TEXT("Merge freezes true post-mutation description"), Increased.DescriptionAfter.ToString(), FString(TEXT("Weak amount 5.")));
	TestEqual(TEXT("Merge uses existing EffectiveDefinition display name"), Increased.DisplayName.ToString(), FString(TEXT("Weak A")));
	TestTrue(TEXT("Merge uses existing EffectiveDefinition atlas flag"), Increased.bUseAtlasIcon);
	TestTrue(TEXT("Merge uses existing EffectiveDefinition atlas region"), Increased.UVOffset.Equals(FVector2D(0.25, 0.5)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase6UIA2D2ReduceRemoveFreezeTest, "SlayTheSpireDemo.Phase6UIA2D2.Record.ReduceAndRemoveFreeze", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPhase6UIA2D2ReduceRemoveFreezeTest::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;
	UStatusData* Guard = CreateStatusDefinition(Fixture.World, TEXT("GuardStatus"), TEXT("Guard Status"), TEXT("Guard amount {Amount}."));
	if (!TestNotNull(TEXT("Guard definition"), Guard)) return false;
	const FStatusMutationResult Initial = Fixture.Player->GetStatusContainer()->ApplyStatusCommit(Guard, 3, Fixture.Battle->AllocateRuntimeSequence());
	UStatusInstance* Instance = Initial.EffectiveInstance;
	if (!TestNotNull(TEXT("Initial Guard instance"), Instance)) return false;

	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	UReduceStatusAction* Reduce = NewObject<UReduceStatusAction>(Queue);
	Reduce->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, Instance, 2, EStatusChangeReason::Reduced);
	TestTrue(TEXT("Reduce status action resolves"), RunSystemAction(Fixture, Reduce));
	const FPresentationResolutionEnvelope* ReduceEnvelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Reduce envelope"), ReduceEnvelope)) return false;
	const FPresentationRecord* ReduceRecord = FindStatusRecord(*ReduceEnvelope);
	if (!TestNotNull(TEXT("Reduce StatusChanged record"), ReduceRecord)) return false;
	const FStatusChangedPresentationPayload& Reduced = ReduceRecord->StatusChanged;
	TestTrue(TEXT("Reduce reason is Reduced"), Reduced.Reason == EStatusChangeReason::Reduced);
	TestEqual(TEXT("Reduce amount before"), Reduced.AmountBefore, 3);
	TestEqual(TEXT("Reduce amount after"), Reduced.AmountAfter, 1);
	TestFalse(TEXT("Partial reduce retains membership"), Reduced.bRemoved);
	TestEqual(TEXT("Reduce description before"), Reduced.DescriptionBefore.ToString(), FString(TEXT("Guard amount 3.")));
	TestEqual(TEXT("Reduce description after"), Reduced.DescriptionAfter.ToString(), FString(TEXT("Guard amount 1.")));

	Fixture.ResetDeliveries();
	URemoveStatusAction* Remove = NewObject<URemoveStatusAction>(Queue);
	Remove->Initialize(Fixture.Battle, nullptr, Fixture.Player, Instance);
	TestTrue(TEXT("Explicit remove action resolves"), RunSystemAction(Fixture, Remove));
	const FPresentationResolutionEnvelope* RemoveEnvelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("Remove envelope"), RemoveEnvelope)) return false;
	const FPresentationRecord* RemoveRecord = FindStatusRecord(*RemoveEnvelope);
	if (!TestNotNull(TEXT("Remove StatusChanged record"), RemoveRecord)) return false;
	const FStatusChangedPresentationPayload& Removed = RemoveRecord->StatusChanged;
	TestTrue(TEXT("Explicit remove reason is Removed"), Removed.Reason == EStatusChangeReason::Removed);
	TestTrue(TEXT("Explicit remove marks membership removal"), Removed.bRemoved);
	TestEqual(TEXT("Explicit remove amount before"), Removed.AmountBefore, 1);
	TestEqual(TEXT("Explicit remove amount after"), Removed.AmountAfter, 0);
	TestTrue(TEXT("Null system source freezes NAME_None"), Removed.SourcePresentationId.IsNone());
	TestEqual(TEXT("Remove target id"), Removed.TargetPresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Remove description before"), Removed.DescriptionBefore.ToString(), FString(TEXT("Guard amount 1.")));
	TestTrue(TEXT("Remove description after is empty"), Removed.DescriptionAfter.IsEmpty());
	TestFalse(TEXT("Explicit remove leaves no live exact instance"), Fixture.Player->GetStatusContainer()->ContainsStatusInstance(Instance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase6UIA2D2TurnEndDecayRecordTest, "SlayTheSpireDemo.Phase6UIA2D2.Record.TurnEndDecay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPhase6UIA2D2TurnEndDecayRecordTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(0);
	if (!RequireReady(*this, Fixture)) return false;
	UStatusData* Decay = CreateStatusDefinition(Fixture.World, TEXT("DecayStatus"), TEXT("Decay Status"), TEXT("Decay amount {Amount}."));
	if (!TestNotNull(TEXT("Decay definition"), Decay)) return false;
	UTurnEndStatusDecayTrigger* Trigger = NewObject<UTurnEndStatusDecayTrigger>(Decay);
	if (!TestNotNull(TEXT("Turn-end decay trigger"), Trigger)) return false;
	Trigger->AmountToRemove = 1;
	Decay->Triggers.Add(Trigger);
	const FStatusMutationResult Initial = Fixture.Player->GetStatusContainer()->ApplyStatusCommit(Decay, 1, Fixture.Battle->AllocateRuntimeSequence());
	UStatusInstance* Instance = Initial.EffectiveInstance;
	if (!TestNotNull(TEXT("Decay runtime instance"), Instance)) return false;
	Fixture.ResetDeliveries();
	const FGameplayRequestResult EndTurnResult = Fixture.Battle->RequestEndPlayerTurn();
	TestTrue(TEXT("End turn request accepted"), EndTurnResult.IsAcceptedForResolution());
	Fixture.Flush();
	const FPresentationResolutionEnvelope* Envelope = Fixture.LastDelivery();
	if (!TestNotNull(TEXT("End-turn envelope"), Envelope)) return false;
	TestEqual(TEXT("Turn cycle produces exactly one StatusChanged decay record"), CountStatusRecords(*Envelope), 1);
	const FPresentationRecord* Record = FindStatusRecord(*Envelope);
	if (!TestNotNull(TEXT("TurnEndDecay StatusChanged record"), Record)) return false;
	const FStatusChangedPresentationPayload& Payload = Record->StatusChanged;
	TestTrue(TEXT("Turn-end reason preserved even when removal occurs"), Payload.Reason == EStatusChangeReason::TurnEndDecay);
	TestTrue(TEXT("Turn-end decay marks removal"), Payload.bRemoved);
	TestEqual(TEXT("Turn-end amount before"), Payload.AmountBefore, 1);
	TestEqual(TEXT("Turn-end amount after"), Payload.AmountAfter, 0);
	TestEqual(TEXT("Turn-end source id is owner"), Payload.SourcePresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Turn-end target id is owner"), Payload.TargetPresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Turn-end description before"), Payload.DescriptionBefore.ToString(), FString(TEXT("Decay amount 1.")));
	TestTrue(TEXT("Turn-end description after removal is empty"), Payload.DescriptionAfter.IsEmpty());
	TestFalse(TEXT("Turn-end decay removed exact instance"), Fixture.Player->GetStatusContainer()->ContainsStatusInstance(Instance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhase6UIA2D2InvalidHistoryDoesNotRollbackTest, "SlayTheSpireDemo.Phase6UIA2D2.Failure.InvalidHistoryDoesNotAffectGameplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPhase6UIA2D2InvalidHistoryDoesNotRollbackTest::RunTest(const FString& Parameters)
{
	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		UStatusData* Weak = CreateStatusDefinition(Fixture.World, TEXT("LegacyWeak"), TEXT("Legacy Weak"), TEXT("Legacy {Amount}."));
		const FStatusMutationResult Initial = Fixture.Player->GetStatusContainer()->ApplyStatusCommit(Weak, 2, Fixture.Battle->AllocateRuntimeSequence());
		UStatusInstance* Instance = Initial.EffectiveInstance;
		if (!TestNotNull(TEXT("Legacy reduction instance"), Instance)) return false;
		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		UReduceStatusAction* LegacyReduce = NewObject<UReduceStatusAction>(Queue);
		LegacyReduce->Initialize(Fixture.Player->GetStatusContainer(), Instance, 1);
		Fixture.ResetDeliveries();
		TestTrue(TEXT("Legacy-context reduce still executes Gameplay"), RunSystemAction(Fixture, LegacyReduce));
		TestEqual(TEXT("Gameplay reduction remains committed after invalid history"), Instance->GetAmount(), 1);
		TestFalse(TEXT("Invalid status history disables committed Presentation"), Fixture.Battle->IsPresentationAvailable());
		TestEqual(TEXT("Invalid history publishes no partial envelope"), Fixture.Deliveries.Num(), 0);
		TestTrue(TEXT("Presentation failure does not Gameplay-fault battle"), Fixture.Battle->BattleState != EBattleState::ResolutionFaulted);
	}

	{
		FFixture Fixture;
		if (!RequireReady(*this, Fixture)) return false;
		UStatusData* HugeSequence = CreateStatusDefinition(Fixture.World, TEXT("HugeSequence"), TEXT("Huge Sequence"), TEXT("Huge {Amount}."));
		const uint64 UnsafePresentationSequence = static_cast<uint64>(MAX_int64) + 1ull;
		const FStatusMutationResult Initial = Fixture.Player->GetStatusContainer()->ApplyStatusCommit(HugeSequence, 1, UnsafePresentationSequence);
		UStatusInstance* Instance = Initial.EffectiveInstance;
		if (!TestNotNull(TEXT("Huge-sequence status instance"), Instance)) return false;
		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		URemoveStatusAction* Remove = NewObject<URemoveStatusAction>(Queue);
		Remove->Initialize(Fixture.Battle, nullptr, Fixture.Player, Instance);
		Fixture.ResetDeliveries();
		TestTrue(TEXT("Huge-sequence remove executes Gameplay"), RunSystemAction(Fixture, Remove));
		TestFalse(TEXT("Huge-sequence status is removed from Gameplay"), Fixture.Player->GetStatusContainer()->ContainsStatusInstance(Instance));
		TestFalse(TEXT("Unsafe uint64-to-int64 status identity invalidates history"), Fixture.Battle->IsPresentationAvailable());
		TestEqual(TEXT("Unsafe identity publishes no partial envelope"), Fixture.Deliveries.Num(), 0);
		TestTrue(TEXT("Unsafe presentation identity does not Gameplay-fault battle"), Fixture.Battle->BattleState != EBattleState::ResolutionFaulted);
	}
	return true;
}

#endif
