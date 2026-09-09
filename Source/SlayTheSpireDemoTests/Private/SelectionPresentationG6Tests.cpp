#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Combat/Combatant.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Presentation/BattlePresentationController.h"
#include "SelectionPresentationG3TestTypes.h"
#include "SelectionPresentationG6TestTypes.h"
#include "UI/BattleHUDViewModel.h"

namespace SelectionPresentationG6Tests
{
	FBattleHUDCardView MakeDisplayCard(int32 RuntimeId)
	{
		FBattleHUDCardView Card;
		Card.RuntimeId = RuntimeId;
		Card.CardId = FName(*FString::Printf(TEXT("G6Card_%d"), RuntimeId));
		Card.DisplayName = FText::FromName(Card.CardId);
		return Card;
	}

	FPresentationCardSnapshot MakeSnapshot(const FBattleHUDCardView& Card)
	{
		FPresentationCardSnapshot Snapshot;
		Snapshot.RuntimeId = Card.RuntimeId;
		Snapshot.CardId = Card.CardId;
		Snapshot.DisplayName = Card.DisplayName;
		Snapshot.bUpgraded = Card.bUpgraded;
		Snapshot.Cost = Card.Cost;
		Snapshot.CardType = Card.CardType;
		Snapshot.Rarity = Card.Rarity;
		Snapshot.CardColor = Card.CardColor;
		Snapshot.TargetType = Card.TargetType;
		Snapshot.Description = Card.Description;
		Snapshot.CardArt = Card.CardArt;
		return Snapshot;
	}

	FPresentationGroupTag MakeGroup(int64 GroupId, int32 Count)
	{
		FPresentationGroupTag Group;
		Group.Kind = EPresentationGroupKind::SelectionDestination;
		Group.GroupId = GroupId;
		Group.ExpectedMemberCount = Count;
		return Group;
	}

	FPresentationPlaybackToken MakeSingleToken(
		int64 BattleId,
		int64 ResolutionId,
		int64 Sequence,
		int64 Generation)
	{
		FPresentationPlaybackToken Token;
		Token.BattleId = BattleId;
		Token.ResolutionId = ResolutionId;
		Token.PresentationSequence = Sequence;
		Token.LocalPlaybackGeneration = Generation;
		Token.UnitKind = EPresentationPlaybackUnitKind::SingleRecord;
		Token.GroupId = 0;
		return Token;
	}

	FPresentationPlaybackToken MakeGroupToken(
		int64 BattleId,
		int64 ResolutionId,
		int64 Sequence,
		int64 Generation,
		int64 GroupId)
	{
		FPresentationPlaybackToken Token = MakeSingleToken(
			BattleId,
			ResolutionId,
			Sequence,
			Generation);
		Token.UnitKind = EPresentationPlaybackUnitKind::Group;
		Token.GroupId = GroupId;
		return Token;
	}

	FPresentationRecord MakeTaggedEnergyRecord(
		int64 BattleId,
		int64 ResolutionId,
		int64 Sequence,
		const FPresentationGroupTag& Group)
	{
		FPresentationRecord Record;
		Record.BattleId = BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::EnergyChanged;
		Record.Group = Group;
		Record.EnergyChanged.EnergyBefore = 3;
		Record.EnergyChanged.EnergyAfter = 2;
		Record.EnergyChanged.Delta = -1;
		return Record;
	}

	struct FControllerFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UBattleHUDViewModel* ViewModel = nullptr;
		USelectionPresentationG6ControllerWidget* Widget = nullptr;
		UBattlePresentationController* Controller = nullptr;
		FPresentationStateSnapshot Baseline;
		int64 BaselineResolutionId = 0;

		FControllerFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(
				ACombatant::StaticClass(),
				FTransform(FVector(100.0, 0.0, 0.0)),
				SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(
				ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

			Player->MaxHP = 80;
			Enemy->MaxHP = 50;
			Player->PresentationId = TEXT("PlayerStable");
			Enemy->PresentationId = TEXT("EnemyStable");
			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 3;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->bEnableCommittedPresentationRecording = true;

			for (int32 Index = 0; Index < 3; ++Index)
			{
				UCardData* Card = NewObject<UCardData>(World);
				Card->CardId = FName(*FString::Printf(TEXT("G6ControllerCard_%d"), Index));
				Card->DisplayName = FText::FromName(Card->CardId);
				Card->Description = FText::FromString(TEXT("G6 controller chronology fixture."));
				Card->BaseCost = 0;
				Card->UpgradedCost = 0;
				Card->CardType = ECardType::Skill;
				Card->TargetType = ECardTargetType::None;
				Card->DefaultDestination = ECardDestination::Discard;
				Battle->DebugStartingDeck.Add(Card);
			}

			Battle->StartBattle();
			Battle->FlushScheduledReadStateReadyForTesting();
			if (!Battle->TryGetLatestFrozenPresentationBaseline(Baseline)) return;
			BaselineResolutionId = static_cast<int64>(
				Battle->GetLatestFrozenPresentationBaselineResolutionId());

			ViewModel = NewObject<UBattleHUDViewModel>(World);
			Widget = NewObject<USelectionPresentationG6ControllerWidget>(World);
			Controller = NewObject<UBattlePresentationController>(World);
			if (!IsValid(ViewModel) || !IsValid(Widget) || !IsValid(Controller)) return;
			if (!ViewModel->Initialize(Battle, false)) return;
			Widget->SetViewModel(ViewModel);
			if (!Controller->Initialize(Battle, ViewModel, Widget)) return;
			Widget->SetPresentationController(Controller);
		}

		~FControllerFixture()
		{
			if (IsValid(Controller)) Controller->Shutdown();
			if (IsValid(ViewModel)) ViewModel->Shutdown();
			if (IsValid(World)) World->DestroyWorld(false);
		}

		bool IsReady() const
		{
			return IsValid(World)
				&& IsValid(Battle)
				&& IsValid(ViewModel)
				&& IsValid(Widget)
				&& IsValid(Controller)
				&& Baseline.BattleId > 0
				&& Baseline.StateRevision > 0
				&& Baseline.HandCards.Num() == 3;
		}

		FPresentationResolutionEnvelope MakeGroupEnvelope(
			int32 MemberCount,
			bool bInterleaveDamage) const
		{
			FPresentationResolutionEnvelope Envelope;
			Envelope.BattleId = Baseline.BattleId;
			Envelope.ResolutionId = BaselineResolutionId + 1;
			Envelope.Origin = EPresentationResolutionOrigin::System;
			Envelope.FinalStateRevision = Baseline.StateRevision;
			Envelope.FinalSnapshot = Baseline;

			const FPresentationGroupTag Group = MakeGroup(6101, MemberCount);
			FPresentationGroupDeclaration Declaration;
			Declaration.Group = Group;
			for (int32 Index = 0; Index < MemberCount; ++Index)
			{
				Declaration.CanonicalSelectedRuntimeIds.Add(
					Baseline.HandCards[Index].RuntimeId);
			}
			Envelope.PresentationGroups.Add(Declaration);

			int64 Sequence = 1;
			for (int32 MemberIndex = 0; MemberIndex < MemberCount; ++MemberIndex)
			{
				FPresentationRecord Record;
				Record.BattleId = Envelope.BattleId;
				Record.ResolutionId = Envelope.ResolutionId;
				Record.PresentationSequence = Sequence++;
				Record.Type = EBattlePresentationRecordType::CardZoneChanged;
				Record.Group = Group;
				Record.CardZoneChanged.Card = MakeSnapshot(Baseline.HandCards[MemberIndex]);
				Record.CardZoneChanged.FromZone = ECardZone::Hand;
				Record.CardZoneChanged.ToZone = ECardZone::ExhaustPile;
				Record.CardZoneChanged.FromIndex = 0;
				Record.CardZoneChanged.ToIndex = Baseline.ExhaustCount + MemberIndex;
				Envelope.Records.Add(Record);

				if (MemberIndex == 0 && bInterleaveDamage)
				{
					FPresentationRecord Damage;
					Damage.BattleId = Envelope.BattleId;
					Damage.ResolutionId = Envelope.ResolutionId;
					Damage.PresentationSequence = Sequence++;
					Damage.Type = EBattlePresentationRecordType::Damage;
					Damage.Damage.SourcePresentationId = Baseline.Player.PresentationId;
					Damage.Damage.TargetPresentationId = Baseline.Enemy.PresentationId;
					Damage.Damage.HPBefore = Baseline.Enemy.HP;
					Damage.Damage.HPAfter = FMath::Max(0, Baseline.Enemy.HP - 1);
					Damage.Damage.BlockBefore = Baseline.Enemy.Block;
					Damage.Damage.BlockAfter = Baseline.Enemy.Block;
					Envelope.Records.Add(Damage);
					Envelope.FinalSnapshot.Enemy.HP = Damage.Damage.HPAfter;
					Envelope.FinalSnapshot.Enemy.bDead = Damage.Damage.HPAfter <= 0;
				}
			}

			for (int32 Index = 0; Index < MemberCount; ++Index)
			{
				Envelope.FinalSnapshot.HandCards.RemoveAt(0);
			}
			Envelope.FinalSnapshot.ExhaustCount = Baseline.ExhaustCount + MemberCount;
			return Envelope;
		}
	};
}

using namespace SelectionPresentationG6Tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG6BatchOwnershipAtomicityTest,
	"SlayTheSpireDemo.SelectionPresentation.G6.BatchOwnershipAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG6BatchOwnershipAtomicityTest::RunTest(const FString& Parameters)
{
	UBattleHUDViewModel* VM = NewObject<UBattleHUDViewModel>();
	if (!TestNotNull(TEXT("ViewModel exists"), VM)) return false;
	VM->BattleId = 9101;
	VM->StateRevision = 17;
	const TArray<int32> RuntimeIds = { 101, 102, 103 };
	for (const int32 RuntimeId : RuntimeIds)
	{
		VM->HandCards.Add(MakeDisplayCard(RuntimeId));
	}

	const int64 Generation = VM->BeginCardPresentationSelectionLifecycle(VM->StateRevision);
	if (!TestTrue(TEXT("Lifecycle begins"), Generation > 0)) return false;
	for (const int32 RuntimeId : RuntimeIds)
	{
		if (!TestTrue(TEXT("Pending ownership established"),
			VM->SetPendingCardPresentationSelection(Generation, RuntimeId, true)))
		{
			return false;
		}
	}
	if (!TestTrue(TEXT("Selection ownership confirms"),
		VM->ConfirmCardPresentationSelection(Generation, RuntimeIds)))
	{
		return false;
	}

	int32 PublishCount = 0;
	TArray<int32> LastPublishedIds;
	VM->OnCardPresentationOwnershipChanged.AddLambda(
		[&](const TArray<int32>& ChangedIds)
		{
			++PublishCount;
			LastPublishedIds = ChangedIds;
		});

	TestFalse(TEXT("One invalid member rejects entire batch"),
		VM->TryTransferCardPresentationOwnershipBatch(
			Generation,
			{ 101, 999, 103 },
			ECardPresentationOwner::SelectionArea,
			ECardPresentationOwner::Transition));
	TestEqual(TEXT("Rejected batch publishes nothing"), PublishCount, 0);
	for (const int32 RuntimeId : RuntimeIds)
	{
		TestEqual(TEXT("Rejected batch leaves every member in SelectionArea"),
			VM->GetCardPresentationOwner(RuntimeId),
			ECardPresentationOwner::SelectionArea);
	}

	TestTrue(TEXT("Valid cohort transfers atomically to Transition"),
		VM->TryTransferCardPresentationOwnershipBatch(
			Generation,
			RuntimeIds,
			ECardPresentationOwner::SelectionArea,
			ECardPresentationOwner::Transition));
	TestEqual(TEXT("Accepted batch publishes exactly once"), PublishCount, 1);
	TestEqual(TEXT("Accepted publication contains full cohort"), LastPublishedIds.Num(), RuntimeIds.Num());
	for (const int32 RuntimeId : RuntimeIds)
	{
		TestEqual(TEXT("Every member owns Transition after batch commit"),
			VM->GetCardPresentationOwner(RuntimeId),
			ECardPresentationOwner::Transition);
	}

	TestTrue(TEXT("Completed parallel visuals become reducer-pending as one cohort"),
		VM->TryTransferCardPresentationOwnershipBatch(
			Generation,
			RuntimeIds,
			ECardPresentationOwner::Transition,
			ECardPresentationOwner::ConsumedPendingReducer));
	TestEqual(TEXT("Reducer-pending batch publishes exactly once more"), PublishCount, 2);
	for (const int32 RuntimeId : RuntimeIds)
	{
		TestEqual(TEXT("Every completed member waits in ConsumedPendingReducer"),
			VM->GetCardPresentationOwner(RuntimeId),
			ECardPresentationOwner::ConsumedPendingReducer);
	}
	VM->OnCardPresentationOwnershipChanged.Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG6TrackedUpgradeRollbackTest,
	"SlayTheSpireDemo.SelectionPresentation.G6.TrackedUpgradeRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG6TrackedUpgradeRollbackTest::RunTest(const FString& Parameters)
{
	USelectionPresentationG3PlaybackWidget* Widget =
		NewObject<USelectionPresentationG3PlaybackWidget>();
	if (!TestNotNull(TEXT("Widget exists"), Widget)) return false;

	const int64 BattleId = 9201;
	const int64 ResolutionId = 9202;
	const FPresentationGroupTag Group = MakeGroup(3, 2);
	const TArray<FPresentationRecord> Records = {
		MakeTaggedEnergyRecord(BattleId, ResolutionId, 1, Group),
		MakeTaggedEnergyRecord(BattleId, ResolutionId, 2, Group)
	};
	const TArray<int32> RecordIndices = { 4, 7 };
	const FPresentationPlaybackToken SingleToken =
		MakeSingleToken(BattleId, ResolutionId, 1, 10);
	const FPresentationPlaybackToken GroupToken =
		MakeGroupToken(BattleId, ResolutionId, 1, 11, Group.GroupId);

	Widget->bAcceptAsyncRecordPlayback = true;
	Widget->bAcceptAsyncGroupPlayback = false;
	if (!TestTrue(TEXT("Leader SingleRecord is tracked"),
		Widget->PlayPresentationRecord(Records[0], SingleToken, RecordIndices[0])))
	{
		return false;
	}

	TestFalse(TEXT("Declined Group replacement reports false"),
		Widget->TryReplaceTrackedPresentationRecordWithGroup(
			SingleToken,
			Records,
			Group,
			RecordIndices,
			GroupToken));
	TestTrue(TEXT("Declined replacement preserves tracked owner"),
		Widget->HasTrackedPresentationPlayback());
	TestTrue(TEXT("Declined replacement restores exact SingleRecord token"),
		Widget->GetTrackedPresentationToken() == SingleToken);
	TestEqual(TEXT("Group preflight attempted once"), Widget->GroupPlayCallCount, 1);
	TestEqual(TEXT("Rollback emits no Group cancellation"), Widget->GroupCancelCallCount, 0);
	TestEqual(TEXT("Rollback emits no leader cancellation"), Widget->RecordCancelCallCount, 0);

	TestTrue(TEXT("Exact original token can still cancel normally"),
		Widget->CancelTrackedPresentationPlayback(SingleToken));
	TestEqual(TEXT("Normal cleanup cancels only the original leader once"),
		Widget->RecordCancelCallCount,
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG6ParallelChronologyTest,
	"SlayTheSpireDemo.SelectionPresentation.G6.ParallelVisualSerialReducer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG6ParallelChronologyTest::RunTest(const FString& Parameters)
{
	FControllerFixture Fixture;
	if (!TestTrue(TEXT("Controller fixture is ready"), Fixture.IsReady())) return false;
	Fixture.Widget->bAcceptGroupPlayback = true;
	Fixture.Widget->bAcceptFallbackSingleRecordPlayback = false;
	const FPresentationResolutionEnvelope Envelope = Fixture.MakeGroupEnvelope(3, true);

	Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
	TestEqual(TEXT("Production leader activates one Group"), Fixture.Widget->GroupPlayCallCount, 1);
	TestEqual(TEXT("Controller owns Group while parallel visual is active"),
		Fixture.Controller->GetActivePlaybackUnitKindForTesting(),
		EPresentationPlaybackUnitKind::Group);
	TestEqual(TEXT("No reducer member is consumed before Group visual completion"),
		Fixture.ViewModel->HandCards.Num(),
		3);
	TestEqual(TEXT("Only leader SingleRecord offer occurred before Group completion"),
		Fixture.Widget->RecordPlayCallCount,
		1);

	Fixture.Widget->CompleteAcceptedGroup();
	FTSTicker::GetCoreTicker().Tick(0.0f);

	TestEqual(TEXT("Group completion visually suppresses exactly two future members"),
		Fixture.Widget->AlreadyPresentedConsumeCount,
		2);
	TestEqual(TEXT("Every chronological record still reaches the normal cursor"),
		Fixture.Widget->RecordPlayCallCount,
		4);
	if (Fixture.Widget->HandCountsAtRecordOffer.Num() == 4)
	{
		TestEqual(TEXT("Leader offer sees all selected cards"), Fixture.Widget->HandCountsAtRecordOffer[0], 3);
		TestEqual(TEXT("Interleaved record runs after leader reducer only"), Fixture.Widget->HandCountsAtRecordOffer[1], 2);
		TestEqual(TEXT("Second Group member is offered with itself still in Hand"), Fixture.Widget->HandCountsAtRecordOffer[2], 2);
		TestEqual(TEXT("Third Group member waits for second member reducer"), Fixture.Widget->HandCountsAtRecordOffer[3], 1);
	}
	TestEqual(TEXT("Envelope completes at exact resolution"),
		Fixture.Controller->GetLastCompletedResolutionIdForTesting(),
		Envelope.ResolutionId);
	TestEqual(TEXT("All selected cards are reduced from Hand in chronological order"),
		Fixture.ViewModel->HandCards.Num(),
		0);
	TestEqual(TEXT("Final Exhaust count matches three reducers"),
		Fixture.ViewModel->ExhaustCount,
		Fixture.Baseline.ExhaustCount + 3);
	TestEqual(TEXT("Interleaved Damage reducer is preserved"),
		Fixture.ViewModel->Enemy.HP,
		FMath::Max(0, Fixture.Baseline.Enemy.HP - 1));
	TestEqual(TEXT("Successful Group path does not cancel visuals"),
		Fixture.Widget->GroupCancelCallCount,
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG6SequentialFallbackTest,
	"SlayTheSpireDemo.SelectionPresentation.G6.GroupDeclineSequentialFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG6SequentialFallbackTest::RunTest(const FString& Parameters)
{
	FControllerFixture Fixture;
	if (!TestTrue(TEXT("Controller fixture is ready"), Fixture.IsReady())) return false;
	Fixture.Widget->bAcceptGroupPlayback = false;
	Fixture.Widget->bAcceptFallbackSingleRecordPlayback = true;
	const FPresentationResolutionEnvelope Envelope = Fixture.MakeGroupEnvelope(2, false);

	Fixture.Battle->OnPresentationResolutionReady.Broadcast(Envelope);
	TestEqual(TEXT("Group preflight is attempted only for the semantic leader"),
		Fixture.Widget->GroupPlayCallCount,
		1);
	TestEqual(TEXT("Declined Group leaves Controller on SingleRecord"),
		Fixture.Controller->GetActivePlaybackUnitKindForTesting(),
		EPresentationPlaybackUnitKind::SingleRecord);
	TestTrue(TEXT("Declined Group preserves the tracked SingleRecord"),
		Fixture.Widget->HasTrackedPresentationPlayback());
	TestEqual(TEXT("Declined Group does not cancel leader"), Fixture.Widget->RecordCancelCallCount, 0);
	TestEqual(TEXT("Declined Group does not emit Group cancellation"), Fixture.Widget->GroupCancelCallCount, 0);

	Fixture.Widget->CompleteLastSingleRecord();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Second member remains a normal sequential SingleRecord"),
		Fixture.Controller->GetActivePlaybackUnitKindForTesting(),
		EPresentationPlaybackUnitKind::SingleRecord);
	TestEqual(TEXT("Group is not retried from a non-leader member"), Fixture.Widget->GroupPlayCallCount, 1);

	Fixture.Widget->CompleteLastSingleRecord();
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Sequential fallback completes exact resolution"),
		Fixture.Controller->GetLastCompletedResolutionIdForTesting(),
		Envelope.ResolutionId);
	TestEqual(TEXT("Sequential fallback reduces only the two selected members"),
		Fixture.ViewModel->HandCards.Num(),
		1);
	TestEqual(TEXT("Sequential fallback applies both destination reducers"),
		Fixture.ViewModel->ExhaustCount,
		Fixture.Baseline.ExhaustCount + 2);
	return true;
}

#endif
