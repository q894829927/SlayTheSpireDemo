#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Actions/BattleActionQueue.h"
#include "Battle/BattleManager.h"
#include "Cards/CardData.h"
#include "Cards/CardInstance.h"
#include "Combat/Combatant.h"
#include "Deck/DeckRuntime.h"
#include "Presentation/BattlePresentationController.h"
#include "Presentation/BattlePresentationRecorder.h"
#include "Presentation/PresentationTypes.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/BattleHUDViewModel.h"

namespace Phase6UIA2ATest
{
	UCardData* CreateCard(UObject* Outer, UPhase6UIA2AProbeState* ProbeState = nullptr)
	{
		UCardData* Card = NewObject<UCardData>(Outer);
		Card->CardId = TEXT("A2AProbeCard");
		Card->DisplayName = FText::FromString(TEXT("A2A Probe Card"));
		Card->Description = FText::FromString(TEXT("A frozen card description."));
		Card->BaseCost = 0;
		Card->CardType = ECardType::Skill;
		Card->TargetType = ECardTargetType::Enemy;
		Card->DefaultDestination = ECardDestination::Discard;
		Card->CardArt = NewObject<UTexture2D>(Outer);

		if (IsValid(ProbeState))
		{
			UPhase6UIA2AProbeCardEffect* Effect = NewObject<UPhase6UIA2AProbeCardEffect>(Card);
			Effect->Initialize(ProbeState, false);
			Card->Effects.Add(Effect);
		}
		return Card;
	}

	struct FFixture
	{
		UWorld* World = nullptr;
		ACombatant* Player = nullptr;
		ACombatant* Enemy = nullptr;
		ABattleManager* Battle = nullptr;
		UCardData* CardDefinition = nullptr;

		FFixture(bool bAutoStart = true, bool bEnablePresentation = true, bool bUseAuthoredIds = true)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
			if (!IsValid(World)) return;

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform::Identity, SpawnParameters);
			Enemy = World->SpawnActor<ACombatant>(ACombatant::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)), SpawnParameters);
			Battle = World->SpawnActor<ABattleManager>(ABattleManager::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!IsValid(Player) || !IsValid(Enemy) || !IsValid(Battle)) return;

			Player->MaxHP = 80;
			Enemy->MaxHP = 50;
			Player->DisplayName = FText::FromString(TEXT("Ironclad"));
			Enemy->DisplayName = FText::FromString(TEXT("Cultist"));
			if (bUseAuthoredIds)
			{
				Player->PresentationId = TEXT("PlayerHero");
				Enemy->PresentationId = TEXT("EnemyPrimary");
			}
			else
			{
				Player->PresentationId = NAME_None;
				Enemy->PresentationId = NAME_None;
			}

			Battle->Player = Player;
			Battle->Enemy = Enemy;
			Battle->OpeningHandDrawCount = 1;
			Battle->PlayerTurnDrawCount = 0;
			Battle->EnemyTestAttackDamage = 0;
			Battle->DeckDebugSeed = 1337;
			Battle->bEnableCommittedPresentationRecording = bEnablePresentation;
			CardDefinition = CreateCard(World);
			Battle->DebugStartingDeck.Add(CardDefinition);

			if (bAutoStart)
			{
				Battle->StartBattle();
			}
		}

		~FFixture()
		{
			if (IsValid(World)) World->DestroyWorld(false);
		}

		bool IsReady() const
		{
			return IsValid(World) && IsValid(Player) && IsValid(Enemy) && IsValid(Battle)
				&& Battle->BattleState == EBattleState::PlayerTurn
				&& IsValid(Battle->GetActionQueueForTesting());
		}

		void FlushPublicDelivery() const
		{
			if (IsValid(Battle)) Battle->FlushScheduledReadStateReadyForTesting();
		}

		UCardInstance* HandCard() const
		{
			UDeckRuntime* Deck = IsValid(Battle) ? Battle->GetDeckRuntimeForTesting() : nullptr;
			return IsValid(Deck) ? Deck->GetFirstHandCard() : nullptr;
		}
	};

	bool RequireReady(FAutomationTestBase& Test, const FFixture& Fixture)
	{
		if (Fixture.IsReady()) return true;
		Test.AddError(TEXT("Failed to create the Phase 6UI-A2A fixture."));
		return false;
	}

	void ExpectFrameworkFaultLogs(FAutomationTestBase& Test)
	{
		Test.AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution fault requested:"), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("[ActionQueue] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("[Battle] Resolution faulted."), EAutomationExpectedErrorFlags::Contains, 1);
	}

	void ExpectPresentationUnavailableLogs(FAutomationTestBase& Test, int32 Count = 1)
	{
		Test.AddExpectedErrorPlain(
			TEXT("[Presentation] Unavailable for BattleId="),
			EAutomationExpectedErrorFlags::Contains,
			Count
		);
	}

	FPresentationResolutionEnvelope MakeFaultEnvelope(
		const FPresentationStateSnapshot& Snapshot,
		int64 ResolutionId,
		int64 Sequence
	)
	{
		FPresentationResolutionEnvelope Envelope;
		Envelope.BattleId = Snapshot.BattleId;
		Envelope.ResolutionId = ResolutionId;
		Envelope.Origin = EPresentationResolutionOrigin::System;
		Envelope.FinalStateRevision = Snapshot.StateRevision;
		Envelope.FinalSnapshot = Snapshot;

		FPresentationRecord Record;
		Record.BattleId = Snapshot.BattleId;
		Record.ResolutionId = ResolutionId;
		Record.PresentationSequence = Sequence;
		Record.Type = EBattlePresentationRecordType::ResolutionFault;
		Record.FaultReason = TEXT("Controller test fault");
		Envelope.Records.Add(Record);
		return Envelope;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2AResolutionLifecycleTest,
	"SlayTheSpireDemo.Phase6UIA2A.Infrastructure.ResolutionLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2AResolutionLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2ATest;

	// BattleStartBeginsBeforeFaultCapableOpeningWork +
	// BattleStartOpeningDrawCreatesNoPresentationRecords.
	FFixture OpeningFixture(false);
	TArray<FPresentationResolutionEnvelope> OpeningDeliveries;
	OpeningFixture.Battle->OnPresentationResolutionReady.AddLambda(
		[&OpeningDeliveries](const FPresentationResolutionEnvelope& Envelope)
		{
			OpeningDeliveries.Add(Envelope);
		}
	);
	OpeningFixture.Battle->StartBattle();
	TestTrue(TEXT("BattleStart reaches PlayerTurn"), OpeningFixture.IsReady());
	TestEqual(TEXT("BattleStart envelope is sealed before public delivery"), OpeningFixture.Battle->GetLastSealedPresentationResolutionIdForTesting(), uint64(1));
	TestEqual(TEXT("No BattleStart public callback re-enters StartBattle"), OpeningDeliveries.Num(), 0);
	OpeningFixture.FlushPublicDelivery();
	TestEqual(TEXT("One BattleStart envelope delivered"), OpeningDeliveries.Num(), 1);
	if (OpeningDeliveries.Num() == 1)
	{
		TestEqual(TEXT("BattleStart origin"), OpeningDeliveries[0].Origin, EPresentationResolutionOrigin::BattleStart);
		TestEqual(TEXT("Opening deterministic draw creates no presentation records"), OpeningDeliveries[0].Records.Num(), 0);
	}

	// AcceptedRequestEstablishesResolutionBeforeExecution + RecordWriterIsOptionalAndExplicit.
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;
	Fixture.FlushPublicDelivery();
	UPhase6UIA2AProbeState* Probe = NewObject<UPhase6UIA2AProbeState>(Fixture.World);
	UPhase6UIA2AProbeCardEffect* ProbeEffect = NewObject<UPhase6UIA2AProbeCardEffect>(Fixture.CardDefinition);
	ProbeEffect->Initialize(Probe, false);
	Fixture.CardDefinition->Effects.Add(ProbeEffect);

	UCardInstance* Card = Fixture.HandCard();
	const uint64 BeforeAccepted = Fixture.Battle->GetLastSealedPresentationResolutionIdForTesting();
	TestTrue(TEXT("Formal card request accepted"), Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy).IsAcceptedForResolution());
	TestTrue(TEXT("Effect received writer before follow-up execution"), Probe->bContextWriterAvailable);
	TestTrue(TEXT("Nested card-effect Action received explicit same writer"), Probe->bActionWriterAvailable);
	TestEqual(TEXT("Context/action ResolutionId match"), Probe->ContextResolutionId, Probe->ActionResolutionId);
	TestTrue(TEXT("Accepted request sealed a new Resolution"), Fixture.Battle->GetLastSealedPresentationResolutionIdForTesting() > BeforeAccepted);

	UPhase6UIA2AProbeAction* OptionalAction = NewObject<UPhase6UIA2AProbeAction>(Fixture.Battle->GetActionQueueForTesting());
	UPhase6UIA2AProbeState* OptionalProbe = NewObject<UPhase6UIA2AProbeState>(Fixture.World);
	OptionalAction->Initialize(OptionalProbe, false);
	OptionalAction->Execute(Fixture.Battle->GetActionQueueForTesting());
	TestFalse(TEXT("Action writer is optional/no-history by default"), OptionalProbe->bActionWriterAvailable);

	// OrdinaryValidationRejectionCreatesNoResolution.
	const uint64 BeforeRejected = Fixture.Battle->GetLastSealedPresentationResolutionIdForTesting();
	TestFalse(TEXT("Played card is rejected when no longer in Hand"), Fixture.Battle->RequestPlayCard(Card, Fixture.Enemy).IsAcceptedForResolution());
	TestEqual(TEXT("Ordinary rejection creates no Resolution"), Fixture.Battle->GetLastSealedPresentationResolutionIdForTesting(), BeforeRejected);

	// SystemResolutionCanBeCreated + EmptyRecordResolutionSealsSafely +
	// OneActiveResolutionSealsAtMostOnce + ResolutionSealsBeforeNextRequestCanBegin.
	TestTrue(TEXT("System Resolution begins"), Fixture.Battle->BeginSystemPresentationResolutionForTesting());
	const FPresentationRecordWriter SystemWriter = Fixture.Battle->GetActivePresentationRecordWriterForTesting();
	TestTrue(TEXT("System writer is explicit and available"), SystemWriter.IsAvailable());
	TestTrue(TEXT("Empty-record System Resolution seals"), Fixture.Battle->SealActivePresentationResolutionForTesting());
	TestFalse(TEXT("Same active Resolution cannot seal twice"), Fixture.Battle->SealActivePresentationResolutionForTesting());
	TestTrue(TEXT("Next Resolution may begin after synchronous seal"), Fixture.Battle->BeginSystemPresentationResolutionForTesting());
	TestTrue(TEXT("Next Resolution seals independently"), Fixture.Battle->SealActivePresentationResolutionForTesting());
	Fixture.FlushPublicDelivery();

	// FaultRetainsCommittedRecordsAndAppendsResolutionFaultLast.
	TArray<FPresentationResolutionEnvelope> FaultDeliveries;
	Fixture.Battle->OnPresentationResolutionReady.AddLambda(
		[&FaultDeliveries](const FPresentationResolutionEnvelope& Envelope)
		{
			FaultDeliveries.Add(Envelope);
		}
	);
	TestTrue(TEXT("Fault test System Resolution begins"), Fixture.Battle->BeginSystemPresentationResolutionForTesting());
	FPresentationRecord CommittedRecord;
	CommittedRecord.Type = EBattlePresentationRecordType::None;
	TestTrue(TEXT("Committed pre-fault presentation record appended"), Fixture.Battle->GetActivePresentationRecordWriterForTesting().Append(CommittedRecord));
	ExpectFrameworkFaultLogs(*this);
	Fixture.Battle->GetActionQueueForTesting()->RequestResolutionFault(TEXT("A2A retained-record test fault"));
	Fixture.FlushPublicDelivery();
	TestTrue(TEXT("Fault Resolution delivered"), FaultDeliveries.Num() > 0);
	if (FaultDeliveries.Num() > 0)
	{
		const FPresentationResolutionEnvelope& FaultEnvelope = FaultDeliveries.Last();
		TestEqual(TEXT("Fault keeps earlier committed record plus final fault"), FaultEnvelope.Records.Num(), 2);
		if (FaultEnvelope.Records.Num() == 2)
		{
			TestEqual(TEXT("ResolutionFault is final record"), FaultEnvelope.Records.Last().Type, EBattlePresentationRecordType::ResolutionFault);
		}
	}

	// PostValidationFrameworkFaultSealsFaultResolution on a clean battle.
	FFixture PostValidationFixture;
	if (!RequireReady(*this, PostValidationFixture)) return false;
	PostValidationFixture.FlushPublicDelivery();
	TArray<FPresentationResolutionEnvelope> PostValidationDeliveries;
	PostValidationFixture.Battle->OnPresentationResolutionReady.AddLambda(
		[&PostValidationDeliveries](const FPresentationResolutionEnvelope& Envelope)
		{
			PostValidationDeliveries.Add(Envelope);
		}
	);
	PostValidationFixture.Battle->SetForceInvalidPlayerEndBatchForTesting(true);
	AddExpectedErrorPlain(
		TEXT("[Battle] EndPlayerTurn failed to enqueue the atomic HandCleanup + TurnEndedAction batch."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	ExpectFrameworkFaultLogs(*this);
	const FGameplayRequestResult FaultedRequest = PostValidationFixture.Battle->RequestEndPlayerTurn();
	TestFalse(TEXT("Post-validation framework failure does not report accepted gameplay request"), FaultedRequest.IsAcceptedForResolution());
	TestEqual(TEXT("Gameplay enters ResolutionFaulted"), PostValidationFixture.Battle->BattleState, EBattleState::ResolutionFaulted);
	TestEqual(TEXT("Fault callback remains deferred"), PostValidationDeliveries.Num(), 0);
	PostValidationFixture.FlushPublicDelivery();
	TestEqual(TEXT("Post-validation fault seals one fault Resolution"), PostValidationDeliveries.Num(), 1);
	if (PostValidationDeliveries.Num() == 1)
	{
		TestEqual(TEXT("Post-validation fault final record type"), PostValidationDeliveries[0].Records.Last().Type, EBattlePresentationRecordType::ResolutionFault);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2ADeliveryAndFailureTest,
	"SlayTheSpireDemo.Phase6UIA2A.Infrastructure.DeliveryAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2ADeliveryAndFailureTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2ATest;
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;
	Fixture.FlushPublicDelivery();

	// MultipleSealedBeforeDeferredDeliveryPreserveResolutionOrder +
	// EnvelopeDedupUsesBattleIdAndResolutionId + DuplicateStablePublishDoesNotDuplicateEnvelope.
	TArray<FPresentationResolutionEnvelope> Delivered;
	Fixture.Battle->OnPresentationResolutionReady.AddLambda(
		[&Delivered](const FPresentationResolutionEnvelope& Envelope)
		{
			Delivered.Add(Envelope);
		}
	);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestTrue(TEXT("System Resolution begins before deferred delivery"), Fixture.Battle->BeginSystemPresentationResolutionForTesting());
		TestTrue(TEXT("System Resolution seals before deferred delivery"), Fixture.Battle->SealActivePresentationResolutionForTesting());
	}
	TestEqual(TEXT("All sealed envelopes wait in public-delivery FIFO"), Fixture.Battle->GetPendingPresentationDeliveryCountForTesting(), 3);
	TestEqual(TEXT("No public envelope before deferred edge"), Delivered.Num(), 0);
	Fixture.FlushPublicDelivery();
	TestEqual(TEXT("Three same-revision envelopes delivered by Resolution identity"), Delivered.Num(), 3);
	if (Delivered.Num() == 3)
	{
		TestTrue(TEXT("Resolution delivery order 0<1"), Delivered[0].ResolutionId < Delivered[1].ResolutionId);
		TestTrue(TEXT("Resolution delivery order 1<2"), Delivered[1].ResolutionId < Delivered[2].ResolutionId);
		TestEqual(TEXT("Same-revision envelopes retain same final revision"), Delivered[0].FinalStateRevision, Delivered[1].FinalStateRevision);
	}
	Fixture.FlushPublicDelivery();
	TestEqual(TEXT("Duplicate stable publish does not duplicate envelopes"), Delivered.Num(), 3);

	// PresentationEnvelopeNotificationDoesNotReenterAcceptedRequest.
	FFixture ReentryFixture;
	if (!RequireReady(*this, ReentryFixture)) return false;
	ReentryFixture.FlushPublicDelivery();
	bool bEnvelopeCallbackRan = false;
	ReentryFixture.Battle->OnPresentationResolutionReady.AddLambda(
		[&bEnvelopeCallbackRan](const FPresentationResolutionEnvelope&)
		{
			bEnvelopeCallbackRan = true;
		}
	);
	TestTrue(TEXT("Accepted request returns accepted"), ReentryFixture.Battle->RequestPlayCard(ReentryFixture.HandCard(), ReentryFixture.Enemy).IsAcceptedForResolution());
	TestFalse(TEXT("Envelope callback did not re-enter accepted request"), bEnvelopeCallbackRan);
	ReentryFixture.FlushPublicDelivery();
	TestTrue(TEXT("Envelope callback runs at deferred safe boundary"), bEnvelopeCallbackRan);

	// PendingDeliveryOverflowFallsBackWithoutGameplayFault.
	FFixture OverflowFixture;
	if (!RequireReady(*this, OverflowFixture)) return false;
	OverflowFixture.FlushPublicDelivery();
	for (int32 Index = 0; Index < 12; ++Index)
	{
		TestTrue(TEXT("Overflow System Resolution begins"), OverflowFixture.Battle->BeginSystemPresentationResolutionForTesting());
		TestTrue(TEXT("Overflow System Resolution seals"), OverflowFixture.Battle->SealActivePresentationResolutionForTesting());
	}
	TestTrue(TEXT("Pending public FIFO remains fixed-bounded"), OverflowFixture.Battle->GetPendingPresentationDeliveryCountForTesting() <= 8);
	TestEqual(TEXT("Pending overflow never faults Gameplay"), OverflowFixture.Battle->BattleState, EBattleState::PlayerTurn);
	TestFalse(TEXT("ActionQueue remains healthy after presentation overflow"), OverflowFixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());

	// FreezeFailureDisablesPresentationWithoutGameplayFault.
	FFixture FreezeFixture;
	if (!RequireReady(*this, FreezeFixture)) return false;
	FreezeFixture.FlushPublicDelivery();
	FreezeFixture.Battle->SetForcePresentationFreezeFailureForTesting(true);
	TestTrue(TEXT("Freeze failure test Resolution begins"), FreezeFixture.Battle->BeginSystemPresentationResolutionForTesting());
	ExpectPresentationUnavailableLogs(*this);
	TestFalse(TEXT("Forced freeze prevents seal"), FreezeFixture.Battle->SealActivePresentationResolutionForTesting());
	TestFalse(TEXT("Freeze failure disables Presentation"), FreezeFixture.Battle->IsPresentationAvailable());
	TestEqual(TEXT("Freeze failure leaves Gameplay healthy"), FreezeFixture.Battle->BattleState, EBattleState::PlayerTurn);
	TestFalse(TEXT("Freeze failure releases active builder"), FreezeFixture.Battle->GetPresentationRecorderForTesting()->HasActiveResolution());

	// SealFailureDoesNotLeakBuilderIntoNextResolution.
	FFixture SealFixture;
	if (!RequireReady(*this, SealFixture)) return false;
	SealFixture.FlushPublicDelivery();
	TestTrue(TEXT("Seal failure test Resolution begins"), SealFixture.Battle->BeginSystemPresentationResolutionForTesting());
	SealFixture.Battle->GetPresentationRecorderForTesting()->SetForceNextSealFailureForTesting(true);
	ExpectPresentationUnavailableLogs(*this);
	TestFalse(TEXT("Forced seal fails"), SealFixture.Battle->SealActivePresentationResolutionForTesting());
	TestFalse(TEXT("Failed seal releases builder"), SealFixture.Battle->GetPresentationRecorderForTesting()->HasActiveResolution());
	TestEqual(TEXT("Seal failure never faults Gameplay"), SealFixture.Battle->BattleState, EBattleState::PlayerTurn);

	// AppendFailureDoesNotSealPartialEnvelope + RecordAppendFailureDoesNotChangeGameplayFinishOrQueue.
	FFixture AppendFixture;
	if (!RequireReady(*this, AppendFixture)) return false;
	AppendFixture.FlushPublicDelivery();
	TestTrue(TEXT("Append failure test Resolution begins"), AppendFixture.Battle->BeginSystemPresentationResolutionForTesting());
	FPresentationRecord First;
	First.Type = EBattlePresentationRecordType::None;
	TestTrue(TEXT("First record append succeeds"), AppendFixture.Battle->GetActivePresentationRecordWriterForTesting().Append(First));
	AppendFixture.Battle->GetPresentationRecorderForTesting()->SetForceNextAppendFailureForTesting(true);
	FPresentationRecord Second;
	Second.Type = EBattlePresentationRecordType::None;
	TestFalse(TEXT("Forced second append fails"), AppendFixture.Battle->GetActivePresentationRecordWriterForTesting().Append(Second));
	TestEqual(TEXT("Append failure discards whole unpublished record batch"), AppendFixture.Battle->GetPresentationRecorderForTesting()->GetActiveRecordCountForTesting(), 0);
	ExpectPresentationUnavailableLogs(*this);
	TestFalse(TEXT("Invalidated builder does not seal partial Envelope"), AppendFixture.Battle->SealActivePresentationResolutionForTesting());
	TestEqual(TEXT("Append failure leaves Gameplay state unchanged"), AppendFixture.Battle->BattleState, EBattleState::PlayerTurn);
	TestFalse(TEXT("Append failure does not fault ActionQueue"), AppendFixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());

	// BattleRestartClearsPendingPublicDeliveryQueue + BattleRestartDoesNotLeakBuilderOrRecords.
	FFixture RestartFixture;
	if (!RequireReady(*this, RestartFixture)) return false;
	RestartFixture.FlushPublicDelivery();
	FPresentationStateSnapshot OldBaseline;
	TestTrue(TEXT("Old baseline available"), RestartFixture.Battle->TryGetLatestFrozenPresentationBaseline(OldBaseline));
	TestTrue(TEXT("Old pending Resolution begins"), RestartFixture.Battle->BeginSystemPresentationResolutionForTesting());
	FPresentationRecord OldRecord;
	OldRecord.Type = EBattlePresentationRecordType::None;
	TestTrue(TEXT("Old record appended"), RestartFixture.Battle->GetActivePresentationRecordWriterForTesting().Append(OldRecord));
	RestartFixture.Battle->StartBattle();
	FPresentationStateSnapshot NewBaseline;
	TestTrue(TEXT("Restart produces new frozen baseline"), RestartFixture.Battle->TryGetLatestFrozenPresentationBaseline(NewBaseline));
	TestTrue(TEXT("BattleId changes on restart"), NewBaseline.BattleId != OldBaseline.BattleId);
	TestEqual(TEXT("Restart cannot retain old record batch"), RestartFixture.Battle->GetPresentationRecorderForTesting()->GetActiveRecordCountForTesting(), 0);
	TestTrue(TEXT("Restart public FIFO contains only new battle scope"), RestartFixture.Battle->GetPendingPresentationDeliveryCountForTesting() <= 1);

	// LateSubscriberDoesNotReplayOldBattle.
	FFixture LateFixture;
	if (!RequireReady(*this, LateFixture)) return false;
	LateFixture.FlushPublicDelivery();
	int32 LateCount = 0;
	LateFixture.Battle->OnPresentationResolutionReady.AddLambda(
		[&LateCount](const FPresentationResolutionEnvelope&)
		{
			++LateCount;
		}
	);
	LateFixture.FlushPublicDelivery();
	TestEqual(TEXT("Late subscriber receives no historical replay"), LateCount, 0);
	TestTrue(TEXT("Late-subscriber new Resolution begins"), LateFixture.Battle->BeginSystemPresentationResolutionForTesting());
	TestTrue(TEXT("Late-subscriber new Resolution seals"), LateFixture.Battle->SealActivePresentationResolutionForTesting());
	LateFixture.FlushPublicDelivery();
	TestEqual(TEXT("Late subscriber receives future Resolution"), LateCount, 1);

	// NoControllerOrPresentationDisabledLeavesGameplayUnchanged.
	FFixture DisabledFixture(false, false);
	DisabledFixture.Battle->StartBattle();
	if (!RequireReady(*this, DisabledFixture)) return false;
	TestFalse(TEXT("Recording disabled creates no sealed presentation Resolution"), DisabledFixture.Battle->GetLastSealedPresentationResolutionIdForTesting() > 0);
	TestTrue(TEXT("Disabled presentation still has frozen baseline"), DisabledFixture.Battle->TryGetLatestFrozenPresentationBaseline(NewBaseline));
	TestTrue(TEXT("Gameplay request still accepted without presentation consumer"), DisabledFixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	TestEqual(TEXT("Gameplay completes normally without presentation"), DisabledFixture.Battle->BattleState, EBattleState::PlayerTurn);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2AFrozenStateAndInputTest,
	"SlayTheSpireDemo.Phase6UIA2A.Infrastructure.FrozenStateAndInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2AFrozenStateAndInputTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2ATest;
	FFixture Fixture;
	if (!RequireReady(*this, Fixture)) return false;
	Fixture.FlushPublicDelivery();

	// Add a status, then freeze a same-revision System baseline. This is legal for
	// the test because the gate is proving the frozen DTO field set, not gameplay
	// status mutation ownership.
	UStatusData* Status = NewObject<UStatusData>(Fixture.World);
	Status->StatusId = TEXT("A2AStatus");
	Status->DisplayName = FText::FromString(TEXT("A2A Status"));
	Status->Description = FText::FromString(TEXT("Frozen status description."));
	Status->IconRegion.bUseAtlasIcon = true;
	Status->IconRegion.UVOffset = FVector2D(0.1, 0.2);
	Status->IconRegion.UVScale = FVector2D(0.3, 0.4);
	Status->IconRegion.TrimOffset = FVector2D(0.05, 0.06);
	Status->IconRegion.TrimScale = FVector2D(0.8, 0.9);
	bool bCreated = false;
	Fixture.Player->GetStatusContainer()->ApplyStatus(Status, 2, Fixture.Battle->AllocateRuntimeSequence(), bCreated);
	TestTrue(TEXT("Status created for frozen field gate"), bCreated);
	TestTrue(TEXT("Snapshot-field System Resolution begins"), Fixture.Battle->BeginSystemPresentationResolutionForTesting());
	TestTrue(TEXT("Snapshot-field System Resolution seals"), Fixture.Battle->SealActivePresentationResolutionForTesting());

	FPresentationStateSnapshot Frozen;
	TestTrue(TEXT("FrozenSnapshotContainsCompleteCurrentHUDDisplayValues: baseline exists"), Fixture.Battle->TryGetLatestFrozenPresentationBaseline(Frozen));
	TestTrue(TEXT("Frozen BattleId"), Frozen.BattleId > 0);
	TestTrue(TEXT("Frozen StateRevision"), Frozen.StateRevision > 0);
	TestEqual(TEXT("Frozen BattleState"), Frozen.BattleState, EBattleState::PlayerTurn);
	TestEqual(TEXT("Frozen Energy"), Frozen.Energy, Fixture.Battle->Energy);
	TestEqual(TEXT("Frozen MaxEnergy"), Frozen.MaxEnergy, Fixture.Battle->MaxEnergy);
	TestTrue(TEXT("Frozen can-end-turn advisory"), Frozen.bCanEndTurn);
	TestEqual(TEXT("Frozen player PresentationId"), Frozen.Player.PresentationId, FName(TEXT("PlayerHero")));
	TestEqual(TEXT("Frozen enemy PresentationId"), Frozen.Enemy.PresentationId, FName(TEXT("EnemyPrimary")));
	TestEqual(TEXT("Frozen player status count"), Frozen.Player.Statuses.Num(), 1);
	if (Frozen.Player.Statuses.Num() == 1)
	{
		const FBattleHUDStatusView& FrozenStatus = Frozen.Player.Statuses[0];
		TestEqual(TEXT("Frozen status id"), FrozenStatus.StatusId, Status->StatusId);
		TestEqual(TEXT("Frozen status amount"), FrozenStatus.Amount, 2);
		TestEqual(TEXT("Frozen status display name"), FrozenStatus.DisplayName.ToString(), FString(TEXT("A2A Status")));
		TestTrue(TEXT("Frozen status dynamic description populated"), !FrozenStatus.Description.IsEmpty());
		TestTrue(TEXT("Frozen status atlas flag"), FrozenStatus.bUseAtlasIcon);
		TestEqual(TEXT("Frozen status UV offset"), FrozenStatus.UVOffset, Status->IconRegion.UVOffset);
		TestEqual(TEXT("Frozen status UV scale"), FrozenStatus.UVScale, Status->IconRegion.UVScale);
		TestEqual(TEXT("Frozen status trim offset"), FrozenStatus.TrimOffset, Status->IconRegion.TrimOffset);
		TestEqual(TEXT("Frozen status trim scale"), FrozenStatus.TrimScale, Status->IconRegion.TrimScale);
	}
	TestEqual(TEXT("Frozen hand count"), Frozen.HandCards.Num(), 1);
	if (Frozen.HandCards.Num() == 1)
	{
		const FBattleHUDCardView& FrozenCard = Frozen.HandCards[0];
		TestTrue(TEXT("Frozen card RuntimeId"), FrozenCard.RuntimeId != INDEX_NONE);
		TestEqual(TEXT("Frozen CardId"), FrozenCard.CardId, Fixture.CardDefinition->CardId);
		TestEqual(TEXT("Frozen card display name"), FrozenCard.DisplayName.ToString(), FString(TEXT("A2A Probe Card")));
		TestEqual(TEXT("Frozen card cost"), FrozenCard.Cost, 0);
		TestEqual(TEXT("Frozen card type"), FrozenCard.CardType, ECardType::Skill);
		TestEqual(TEXT("Frozen card target type"), FrozenCard.TargetType, ECardTargetType::Enemy);
		TestTrue(TEXT("Frozen card description populated"), !FrozenCard.Description.IsEmpty());
		TestTrue(TEXT("Frozen CardArt retained as immutable presentation asset"), IsValid(FrozenCard.CardArt));
		TestTrue(TEXT("Frozen gameplay playability"), FrozenCard.bGameplayPlayable);
	}
	TestEqual(TEXT("Frozen draw count"), Frozen.DrawCount, 0);
	TestEqual(TEXT("Frozen discard count"), Frozen.DiscardCount, 0);
	TestEqual(TEXT("Frozen exhaust count"), Frozen.ExhaustCount, 0);
	TestEqual(TEXT("Frozen intent type"), Frozen.EnemyIntent.Type, EBattleHUDIntentType::Attack);
	TestTrue(TEXT("Frozen intent has current resolved amount"), Frozen.EnemyIntent.bHasCurrentResolvedDamageAmount);

	// HistoricalFrozenSnapshotAppliesWithoutMutableRuntimeReads +
	// HistoricalEnvelopeCannotUseLiveInputBindings.
	UBattleHUDViewModel* HistoricalVM = NewObject<UBattleHUDViewModel>(Fixture.World);
	HistoricalVM->ApplyPresentationSnapshot(Frozen, true);
	const int32 FrozenHP = HistoricalVM->Player.HP;
	const FString FrozenCardName = HistoricalVM->HandCards[0].DisplayName.ToString();
	Fixture.Player->HP = 1;
	Fixture.CardDefinition->DisplayName = FText::FromString(TEXT("MUTATED RUNTIME NAME"));
	TestEqual(TEXT("Historical apply remains frozen after mutable HP change"), HistoricalVM->Player.HP, FrozenHP);
	TestEqual(TEXT("Historical apply remains frozen after DataAsset name change"), HistoricalVM->HandCards[0].DisplayName.ToString(), FrozenCardName);
	TestFalse(TEXT("Historical snapshot alone cannot submit through live bindings"), HistoricalVM->SelectCardByRuntimeId(HistoricalVM->HandCards[0].RuntimeId));

	// InputBindingsRefreshOnlyAtNewestMatchingBattleRevision +
	// OnReadStateReadyCannotBypassActivePresentationSequencing.
	FFixture SequencingFixture;
	if (!RequireReady(*this, SequencingFixture)) return false;
	SequencingFixture.FlushPublicDelivery();
	UBattleHUDViewModel* PresentationOwnedVM = NewObject<UBattleHUDViewModel>(SequencingFixture.World);
	TestTrue(TEXT("Presentation-owned VM initializes"), PresentationOwnedVM->Initialize(SequencingFixture.Battle, true));
	FPresentationStateSnapshot OldSnapshot;
	TestTrue(TEXT("Old baseline captured"), SequencingFixture.Battle->TryGetLatestFrozenPresentationBaseline(OldSnapshot));
	const int64 OldRevision = PresentationOwnedVM->StateRevision;
	TestTrue(TEXT("EndTurn request accepted for sequencing test"), SequencingFixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	FPresentationStateSnapshot NewSnapshot;
	TestTrue(TEXT("New authoritative frozen baseline exists before public delivery"), SequencingFixture.Battle->TryGetLatestFrozenPresentationBaseline(NewSnapshot));
	TestTrue(TEXT("New baseline revision is newer"), NewSnapshot.StateRevision > OldSnapshot.StateRevision);
	SequencingFixture.FlushPublicDelivery();
	TestEqual(TEXT("OnReadStateReady cannot bypass Controller-owned historical display"), PresentationOwnedVM->StateRevision, OldRevision);
	PresentationOwnedVM->ApplyPresentationSnapshot(OldSnapshot, true);
	TestFalse(TEXT("Old displayed revision cannot refresh live input bindings"), PresentationOwnedVM->RefreshLiveInputBindingsIfCaughtUp());
	PresentationOwnedVM->ApplyPresentationSnapshot(NewSnapshot, true);
	TestTrue(TEXT("Newest exact BattleId/Revision can refresh live input bindings"), PresentationOwnedVM->RefreshLiveInputBindingsIfCaughtUp());

	// ResolvedPresentationIdSharedBySnapshotTargetsAndRecords: A2A has no combat
	// business record type yet, so this gate proves the one resolver shared by the
	// frozen snapshot and current legal-target boundary; later record slices must
	// call the same BattleManager resolver rather than inventing another fallback.
	FFixture FallbackFixture(true, true, false);
	if (!RequireReady(*this, FallbackFixture)) return false;
	FallbackFixture.FlushPublicDelivery();
	FPresentationStateSnapshot FallbackSnapshot;
	TestTrue(TEXT("Fallback snapshot exists"), FallbackFixture.Battle->TryGetLatestFrozenPresentationBaseline(FallbackSnapshot));
	TestEqual(TEXT("Player fallback PresentationId"), FallbackSnapshot.Player.PresentationId, FName(TEXT("Player")));
	TestEqual(TEXT("Enemy fallback PresentationId"), FallbackSnapshot.Enemy.PresentationId, FName(TEXT("EnemyPrimary")));
	FName ResolvedEnemyId = NAME_None;
	TestTrue(TEXT("Battle resolver resolves enemy"), FallbackFixture.Battle->TryResolveCombatantPresentationId(FallbackFixture.Enemy, ResolvedEnemyId));
	TestEqual(TEXT("Snapshot uses Battle resolver identity"), FallbackSnapshot.Enemy.PresentationId, ResolvedEnemyId);
	UBattleHUDViewModel* DirectVM = NewObject<UBattleHUDViewModel>(FallbackFixture.World);
	TestTrue(TEXT("Direct VM initializes from frozen baseline"), DirectVM->Initialize(FallbackFixture.Battle, false));
	TestTrue(TEXT("Direct VM can select frozen card after latest binding refresh"), DirectVM->SelectCardByRuntimeId(DirectVM->HandCards[0].RuntimeId));
	TestEqual(TEXT("One legal enemy target exposed"), DirectVM->LegalTargets.Num(), 1);
	if (DirectVM->LegalTargets.Num() == 1)
	{
		TestEqual(TEXT("Legal target uses same resolved PresentationId"), DirectVM->LegalTargets[0].PresentationId, ResolvedEnemyId);
	}

	// InvalidResolvedPresentationIdShowsPresentationUnavailable.
	FFixture InvalidFixture(false);
	InvalidFixture.Player->PresentationId = TEXT("DuplicateId");
	InvalidFixture.Enemy->PresentationId = TEXT("DuplicateId");
	ExpectPresentationUnavailableLogs(*this, 2);
	InvalidFixture.Battle->StartBattle();
	TestEqual(TEXT("Invalid presentation identity does not fault headless Gameplay"), InvalidFixture.Battle->BattleState, EBattleState::PlayerTurn);
	TestFalse(TEXT("Duplicate resolved PresentationId marks presentation unavailable"), InvalidFixture.Battle->IsPresentationAvailable());
	UBattleHUDViewModel* ErrorVM = NewObject<UBattleHUDViewModel>(InvalidFixture.World);
	TestTrue(TEXT("PresentationUnavailableStillCreatesErrorCapableHUD: ViewModel initialization succeeds"), ErrorVM->Initialize(InvalidFixture.Battle, true));
	TestEqual(TEXT("PresentationUnavailable visible VM state"), ErrorVM->InteractionState, EBattleHUDInteractionState::PresentationUnavailable);
	TestTrue(TEXT("PresentationUnavailable keeps input locked"), ErrorVM->bInputLocked);
	TestTrue(TEXT("PresentationUnavailable exposes developer feedback"), !ErrorVM->LastFeedback.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2AWriterAndControllerTest,
	"SlayTheSpireDemo.Phase6UIA2A.Infrastructure.WriterAndController",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2AWriterAndControllerTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2ATest;

	// NestedReactionUsesSameActiveResolutionWriter.
	FFixture ReactionFixture;
	if (!RequireReady(*this, ReactionFixture)) return false;
	ReactionFixture.FlushPublicDelivery();
	UPhase6UIA2AProbeState* ReactionProbe = NewObject<UPhase6UIA2AProbeState>(ReactionFixture.World);
	UStatusData* TriggerStatus = NewObject<UStatusData>(ReactionFixture.World);
	TriggerStatus->StatusId = TEXT("A2AWriterTrigger");
	UPhase6UIA2AProbeTrigger* Trigger = NewObject<UPhase6UIA2AProbeTrigger>(TriggerStatus);
	Trigger->Initialize(ReactionProbe);
	TriggerStatus->Triggers.Add(Trigger);
	bool bCreated = false;
	ReactionFixture.Player->GetStatusContainer()->ApplyStatus(
		TriggerStatus,
		1,
		ReactionFixture.Battle->AllocateRuntimeSequence(),
		bCreated
	);
	TestTrue(TEXT("Reaction probe status created"), bCreated);
	TestTrue(TEXT("EndTurn accepted for reaction writer test"), ReactionFixture.Battle->RequestEndPlayerTurn().IsAcceptedForResolution());
	TestTrue(TEXT("Trigger context receives active explicit writer"), ReactionProbe->bContextWriterAvailable);
	TestTrue(TEXT("Reaction Action receives same active explicit writer"), ReactionProbe->bActionWriterAvailable);
	TestEqual(TEXT("Nested/reaction ResolutionId is identical"), ReactionProbe->ContextResolutionId, ReactionProbe->ActionResolutionId);
	TestEqual(TEXT("Reaction executed exactly once"), ReactionProbe->ActionExecutionCount, 1);

	// RecordAppendFailureDoesNotChangeGameplayFinishOrQueue using a custom test
	// Action owned only by the Editor test module.
	FFixture ActionFixture;
	if (!RequireReady(*this, ActionFixture)) return false;
	ActionFixture.FlushPublicDelivery();
	TestTrue(TEXT("Custom Action System Resolution begins"), ActionFixture.Battle->BeginSystemPresentationResolutionForTesting());
	ActionFixture.Battle->GetPresentationRecorderForTesting()->SetForceNextAppendFailureForTesting(true);
	UPhase6UIA2AProbeState* ActionProbe = NewObject<UPhase6UIA2AProbeState>(ActionFixture.World);
	UPhase6UIA2AProbeAction* Action = NewObject<UPhase6UIA2AProbeAction>(ActionFixture.Battle->GetActionQueueForTesting());
	Action->Initialize(ActionProbe, true);
	Action->SetPresentationRecordWriter(ActionFixture.Battle->GetActivePresentationRecordWriterForTesting());
	TestTrue(TEXT("Probe Action enqueued"), ActionFixture.Battle->GetActionQueueForTesting()->AddToBack(Action));
	ExpectPresentationUnavailableLogs(*this);
	TestTrue(TEXT("Probe Action processing starts"), ActionFixture.Battle->GetActionQueueForTesting()->StartProcessing());
	TestTrue(TEXT("Probe Action still finishes after presentation append failure"), Action->IsFinished());
	TestFalse(TEXT("Presentation append failure does not fault gameplay queue"), ActionFixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());

	// ControllerBacklogIsBounded + PlaybackTokenDuplicateAndStaleCompletionIgnored +
	// SkipMissingCallbackTimeoutWidgetLossCatchUpWithoutGameplayFault.
	FFixture ControllerFixture;
	if (!RequireReady(*this, ControllerFixture)) return false;
	ControllerFixture.FlushPublicDelivery();
	FPresentationStateSnapshot Baseline;
	TestTrue(TEXT("Controller baseline available"), ControllerFixture.Battle->TryGetLatestFrozenPresentationBaseline(Baseline));
	UBattleHUDViewModel* VM = NewObject<UBattleHUDViewModel>(ControllerFixture.World);
	TestTrue(TEXT("Controller-owned VM initializes"), VM->Initialize(ControllerFixture.Battle, true));
	UPhase6UIA2APlaybackWidget* Widget = NewObject<UPhase6UIA2APlaybackWidget>(ControllerFixture.World);
	UBattlePresentationController* Controller = NewObject<UBattlePresentationController>(ControllerFixture.World);
	TestTrue(TEXT("Controller initializes"), Controller->Initialize(ControllerFixture.Battle, VM, Widget));
	Widget->SetPresentationController(Controller);

	FPresentationResolutionEnvelope First = MakeFaultEnvelope(Baseline, 100, 1);
	ControllerFixture.Battle->OnPresentationResolutionReady.Broadcast(First);
	TestTrue(TEXT("Fault record enters generic async playback protocol"), Controller->IsWaitingForCompletionForTesting());
	const FPresentationPlaybackToken ValidToken = Controller->GetActivePlaybackTokenForTesting();
	FPresentationPlaybackToken StaleToken = ValidToken;
	++StaleToken.PresentationSequence;
	Controller->NotifyPresentationFinished(StaleToken);
	TestTrue(TEXT("Stale completion ignored"), Controller->IsWaitingForCompletionForTesting());
	Controller->NotifyPresentationFinished(ValidToken);
	TestFalse(TEXT("Valid completion finishes active playback"), Controller->IsWaitingForCompletionForTesting());
	const int64 CompletedAfterValid = Controller->GetLastCompletedResolutionIdForTesting();
	Controller->NotifyPresentationFinished(ValidToken);
	TestEqual(TEXT("Duplicate completion ignored"), Controller->GetLastCompletedResolutionIdForTesting(), CompletedAfterValid);

	// Hold one record active and enqueue enough additional envelopes to exercise the
	// fixed bounded backlog/collapse policy.
	FPresentationResolutionEnvelope Held = MakeFaultEnvelope(Baseline, 101, 2);
	ControllerFixture.Battle->OnPresentationResolutionReady.Broadcast(Held);
	TestTrue(TEXT("Held playback waits"), Controller->IsWaitingForCompletionForTesting());
	for (int64 ResolutionId = 102; ResolutionId <= 114; ++ResolutionId)
	{
		ControllerFixture.Battle->OnPresentationResolutionReady.Broadcast(
			MakeFaultEnvelope(Baseline, ResolutionId, ResolutionId)
		);
	}
	TestTrue(TEXT("Controller backlog remains fixed-bounded after overflow collapse"), Controller->GetBacklogCountForTesting() <= 9);
	TestEqual(TEXT("Controller overflow never changes Gameplay"), ControllerFixture.Battle->BattleState, EBattleState::PlayerTurn);

	// Skip invalidates the old generation and catches up using frozen envelope state.
	Controller->SkipPresentation();
	TestFalse(TEXT("Skip clears active wait"), Controller->IsWaitingForCompletionForTesting());
	Controller->NotifyPresentationFinished(Widget->LastToken);
	TestFalse(TEXT("Callback from pre-skip generation is ignored"), Controller->IsWaitingForCompletionForTesting());

	// Missing Blueprint callback uses immediate fallback.
	Widget->bAcceptAsyncPlayback = false;
	ControllerFixture.Battle->OnPresentationResolutionReady.Broadcast(MakeFaultEnvelope(Baseline, 115, 115));
	TestFalse(TEXT("Missing callback implementation cannot stall presentation"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Missing callback still completes envelope"), Controller->GetLastCompletedResolutionIdForTesting(), int64(115));

	// Timeout fail-safe.
	Widget->bAcceptAsyncPlayback = true;
	ControllerFixture.Battle->OnPresentationResolutionReady.Broadcast(MakeFaultEnvelope(Baseline, 116, 116));
	TestTrue(TEXT("Timeout case starts waiting"), Controller->IsWaitingForCompletionForTesting());
	Controller->ExpireActivePlaybackForTesting();
	TestFalse(TEXT("Timeout completes presentation without gameplay fault"), Controller->IsWaitingForCompletionForTesting());
	TestFalse(TEXT("Timeout leaves gameplay queue healthy"), ControllerFixture.Battle->GetActionQueueForTesting()->IsResolutionFaulted());

	// Widget loss fail-safe.
	ControllerFixture.Battle->OnPresentationResolutionReady.Broadcast(MakeFaultEnvelope(Baseline, 117, 117));
	TestTrue(TEXT("Widget-loss case starts waiting"), Controller->IsWaitingForCompletionForTesting());
	Controller->NotifyWidgetLost(Widget);
	TestFalse(TEXT("Widget loss fast-catches-up"), Controller->IsWaitingForCompletionForTesting());
	TestEqual(TEXT("Widget loss never changes Gameplay"), ControllerFixture.Battle->BattleState, EBattleState::PlayerTurn);

	// No Widget is also a legal immediate-fallback mode.
	UBattlePresentationController* NoWidgetController = NewObject<UBattlePresentationController>(ControllerFixture.World);
	UBattleHUDViewModel* NoWidgetVM = NewObject<UBattleHUDViewModel>(ControllerFixture.World);
	TestTrue(TEXT("No-widget VM initializes"), NoWidgetVM->Initialize(ControllerFixture.Battle, true));
	TestTrue(TEXT("No-widget Controller initializes"), NoWidgetController->Initialize(ControllerFixture.Battle, NoWidgetVM, nullptr));
	ControllerFixture.Battle->OnPresentationResolutionReady.Broadcast(MakeFaultEnvelope(Baseline, 200, 200));
	TestFalse(TEXT("No Widget cannot stall presentation"), NoWidgetController->IsWaitingForCompletionForTesting());

	return true;
}

#endif
