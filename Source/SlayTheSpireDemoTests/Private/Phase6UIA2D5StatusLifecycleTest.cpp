#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2D5TestSupport.h"
#include "Phase6UIA2D5TestTypes.h"
#include "Actions/ApplyStatusAction.h"
#include "Actions/BattleAction.h"
#include "Actions/BattleActionQueue.h"
#include "Actions/ReduceStatusAction.h"
#include "Actions/RemoveStatusAction.h"
#include "Battle/BattleManager.h"
#include "Combat/Combatant.h"
#include "Events/TurnEndStatusDecayTrigger.h"
#include "Presentation/BattlePresentationController.h"
#include "Status/StatusContainer.h"
#include "Status/StatusData.h"
#include "Status/StatusInstance.h"
#include "UI/BattleHUDViewModel.h"
#include "UObject/StrongObjectPtr.h"

namespace Phase6UIA2D5StatusLifecycleTest
{
	using namespace Phase6UIA2D5Test;

	UStatusData* CreateAnchorDefinition(UObject* Outer)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		if (!IsValid(Definition))
		{
			return nullptr;
		}

		Definition->StatusId = TEXT("AnchorStatus");
		Definition->DisplayName = FText::FromString(TEXT("Anchor Status"));
		Definition->Description = FText::FromString(TEXT("Anchor amount {Amount}."));
		return Definition;
	}

	UStatusData* CreateWeakDefinition(UObject* Outer)
	{
		UStatusData* Definition = NewObject<UStatusData>(Outer);
		if (!IsValid(Definition))
		{
			return nullptr;
		}

		Definition->StatusId = TEXT("Weak");
		Definition->DisplayName = FText::FromString(TEXT("Weak Lifecycle"));
		Definition->Description = FText::FromString(TEXT("Weak amount {Amount}."));
		Definition->IconRegion.bUseAtlasIcon = true;
		Definition->IconRegion.UVOffset = FVector2D(0.25, 0.5);
		Definition->IconRegion.UVScale = FVector2D(0.125, 0.25);
		Definition->IconRegion.TrimOffset = FVector2D(0.1, 0.2);
		Definition->IconRegion.TrimScale = FVector2D(0.8, 0.7);

		UTurnEndStatusDecayTrigger* Decay = NewObject<UTurnEndStatusDecayTrigger>(Definition);
		if (!IsValid(Decay))
		{
			return nullptr;
		}
		Decay->AmountToRemove = 1;
		Definition->Triggers.Add(Decay);
		return Definition;
	}

	bool RunSystemAction(FAcceptanceFixture& Fixture, UBattleAction* Action)
	{
		if (!Fixture.IsReady() || !IsValid(Action))
		{
			return false;
		}

		UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
		if (!IsValid(Queue)
			|| Queue->IsBusy()
			|| !Fixture.Battle->BeginSystemPresentationResolutionForTesting())
		{
			return false;
		}

		const FPresentationRecordWriter Writer = Fixture.Battle->GetActivePresentationRecordWriterForTesting();
		if (!Writer.IsAvailable())
		{
			return false;
		}

		Action->SetPresentationRecordWriter(Writer);
		if (!Queue->AddToBack(Action) || !Queue->StartProcessing())
		{
			return false;
		}

		Fixture.Flush();
		return !Queue->IsBusy() && !Queue->IsResolutionFaulted();
	}

	bool RunAlreadyPendingSystemAction(
		FAcceptanceFixture& Fixture,
		UBattleActionQueue* PendingQueue,
		UBattleAction* PendingAction
	)
	{
		if (!Fixture.IsReady()
			|| !IsValid(PendingQueue)
			|| !IsValid(PendingAction)
			|| PendingQueue->IsResolutionFaulted()
			|| PendingQueue->GetPendingCount() != 1)
		{
			return false;
		}

		UBattleActionQueue* BattleQueue = Fixture.Battle->GetActionQueueForTesting();
		if (!IsValid(BattleQueue)
			|| BattleQueue->IsBusy()
			|| !Fixture.Battle->BeginSystemPresentationResolutionForTesting())
		{
			return false;
		}

		const FPresentationRecordWriter Writer = Fixture.Battle->GetActivePresentationRecordWriterForTesting();
		if (!Writer.IsAvailable())
		{
			return false;
		}

		// The Action was already a real pending queue member while Weak#A existed.
		// Only the current formal Resolution writer is supplied immediately before
		// this retained Action is allowed to execute.
		PendingAction->SetPresentationRecordWriter(Writer);
		if (!PendingQueue->StartProcessing())
		{
			return false;
		}

		// Empty/no-op Resolutions are allowed either to publish an empty Envelope or
		// to remain unpublished. Do not make envelope publication part of the stale
		// mutation contract. A genuine Presentation failure is still rejected.
		(void)Fixture.Battle->SealActivePresentationResolutionForTesting();
		Fixture.Flush();
		return !PendingQueue->IsBusy()
			&& !PendingQueue->IsResolutionFaulted()
			&& Fixture.Battle->IsPresentationAvailable();
	}

	UStatusInstance* FindMutableStatus(UStatusContainer* Container, FName StatusId)
	{
		if (!IsValid(Container))
		{
			return nullptr;
		}

		for (const TObjectPtr<UStatusInstance>& Status : Container->GetStatuses())
		{
			if (IsValid(Status.Get()) && Status->GetStatusId() == StatusId)
			{
				return Status.Get();
			}
		}
		return nullptr;
	}

	const FBattleHUDStatusView* FindDisplayedStatus(const UBattleHUDViewModel* ViewModel, FName StatusId)
	{
		if (!IsValid(ViewModel))
		{
			return nullptr;
		}
		return ViewModel->Player.Statuses.FindByPredicate(
			[StatusId](const FBattleHUDStatusView& Status)
			{
				return Status.StatusId == StatusId;
			}
		);
	}

	void GatherStatusHistory(
		const TArray<FCapturedEnvelope>& Captures,
		TArray<FStatusChangedPresentationPayload>& OutHistory
	)
	{
		OutHistory.Reset();
		for (const FCapturedEnvelope& Capture : Captures)
		{
			for (const FPresentationRecord& Record : Capture.Envelope.Records)
			{
				if (Record.Type == EBattlePresentationRecordType::StatusChanged
					&& Record.StatusChanged.StatusId == TEXT("Weak"))
				{
					OutHistory.Add(Record.StatusChanged);
				}
			}
		}
	}

	bool ValidateFrozenIcon(
		FAutomationTestBase& Test,
		const FStatusChangedPresentationPayload& Payload,
		const FString& Context
	)
	{
		bool bOk = true;
		bOk &= Test.TestTrue(*FString::Printf(TEXT("%s atlas flag"), *Context), Payload.bUseAtlasIcon);
		bOk &= Test.TestTrue(*FString::Printf(TEXT("%s UVOffset"), *Context), Payload.UVOffset.Equals(FVector2D(0.25, 0.5)));
		bOk &= Test.TestTrue(*FString::Printf(TEXT("%s UVScale"), *Context), Payload.UVScale.Equals(FVector2D(0.125, 0.25)));
		bOk &= Test.TestTrue(*FString::Printf(TEXT("%s TrimOffset"), *Context), Payload.TrimOffset.Equals(FVector2D(0.1, 0.2)));
		bOk &= Test.TestTrue(*FString::Printf(TEXT("%s TrimScale"), *Context), Payload.TrimScale.Equals(FVector2D(0.8, 0.7)));
		return bOk;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2D5StatusLifecycleTest,
	"SlayTheSpireDemo.Phase6UIA2D5.StatusLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2D5StatusLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace Phase6UIA2D5StatusLifecycleTest;

	FAcceptanceFixture Fixture;
	TArray<UCardData*> EmptyDeck;
	if (!TestTrue(TEXT("Acceptance fixture starts"), Fixture.Start(EmptyDeck)))
	{
		return false;
	}

	UBattleActionQueue* Queue = Fixture.Battle->GetActionQueueForTesting();
	UStatusContainer* Container = Fixture.Player->GetStatusContainer();
	UStatusData* Anchor = CreateAnchorDefinition(Fixture.World);
	UStatusData* Weak = CreateWeakDefinition(Fixture.World);
	if (!TestNotNull(TEXT("Battle action queue"), Queue)
		|| !TestNotNull(TEXT("Player status container"), Container)
		|| !TestNotNull(TEXT("Anchor definition"), Anchor)
		|| !TestNotNull(TEXT("Weak definition"), Weak))
	{
		return false;
	}

	// Establish a persistent lower-sequence status before the formal A2D5-2
	// capture begins. This makes the status-order assertions exercise a real
	// two-row array instead of vacuously passing on a single Weak row.
	UApplyStatusAction* CreateAnchor = NewObject<UApplyStatusAction>(Queue);
	CreateAnchor->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, Anchor, 1);
	if (!TestTrue(TEXT("Anchor status commits"), RunSystemAction(Fixture, CreateAnchor))
		|| !TestTrue(TEXT("Anchor playback drains"), Fixture.DrainPlayback()))
	{
		return false;
	}

	UStatusInstance* AnchorInstance = FindMutableStatus(Container, TEXT("AnchorStatus"));
	const FBattleHUDStatusView* DisplayedAnchor = FindDisplayedStatus(Fixture.ViewModel, TEXT("AnchorStatus"));
	if (!TestNotNull(TEXT("Anchor runtime instance"), AnchorInstance)
		|| !TestNotNull(TEXT("Anchor displayed row"), DisplayedAnchor))
	{
		return false;
	}
	const uint64 AnchorSequence = AnchorInstance->GetRuntimeSequence();
	TestTrue(TEXT("Anchor RuntimeSequence positive"), AnchorSequence > 0);
	TestEqual(TEXT("Anchor amount"), AnchorInstance->GetAmount(), 1);

	if (!TestTrue(TEXT("Acceptance capture rebases after Anchor"), Fixture.ResetAcceptanceCapture()))
	{
		return false;
	}

	UApplyStatusAction* CreateA = NewObject<UApplyStatusAction>(Queue);
	CreateA->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, Weak, 2);
	if (!TestTrue(TEXT("Weak#A apply action commits"), RunSystemAction(Fixture, CreateA)))
	{
		return false;
	}

	UStatusInstance* WeakA = FindMutableStatus(Container, TEXT("Weak"));
	if (!TestNotNull(TEXT("Weak#A exact runtime instance"), WeakA))
	{
		return false;
	}
	const uint64 WeakASequence = WeakA->GetRuntimeSequence();
	TestTrue(TEXT("Weak#A RuntimeSequence positive"), WeakASequence > 0);
	TestTrue(TEXT("Weak#A RuntimeSequence follows Anchor"), WeakASequence > AnchorSequence);

	// Queue the stale exact-instance Action now, while Weak#A is still a real
	// authoritative member. The holding queue is deliberately not started yet.
	TStrongObjectPtr<UBattleActionQueue> StaleQueue(NewObject<UBattleActionQueue>(Fixture.Battle));
	TStrongObjectPtr<UReduceStatusAction> StaleReduce(
		IsValid(StaleQueue.Get()) ? NewObject<UReduceStatusAction>(StaleQueue.Get()) : nullptr
	);
	if (!TestNotNull(TEXT("Stale holding queue"), StaleQueue.Get())
		|| !TestNotNull(TEXT("Stale exact-instance reduce action"), StaleReduce.Get()))
	{
		return false;
	}
	StaleReduce->Initialize(
		Fixture.Battle,
		Fixture.Player,
		Fixture.Player,
		WeakA,
		1,
		EStatusChangeReason::Reduced
	);
	if (!TestTrue(TEXT("Stale reduce enters holding Action batch while Weak#A exists"), StaleQueue->AddToBack(StaleReduce.Get()))
		|| !TestEqual(TEXT("Stale holding queue has one pending Action"), StaleQueue->GetPendingCount(), 1)
		|| !TestTrue(TEXT("Weak#A is still authoritative when stale Action becomes pending"), Container->ContainsStatusInstance(WeakA)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Controller waits on Applied playback"), Fixture.Controller->IsWaitingForCompletionForTesting())
		|| !TestEqual(TEXT("Applied is first visible A2D5 record"), Fixture.Widget->PlayCallCount, 1)
		|| !TestEqual(TEXT("Applied capture contains one record"), Fixture.Widget->PlayedRecords.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("Applied visible record type"), Fixture.Widget->PlayedRecords.Last().Type == EBattlePresentationRecordType::StatusChanged);
	TestTrue(TEXT("Displayed status remains absent before Applied completion"), FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak")) == nullptr);
	TestTrue(TEXT("Applied playback completion accepted"), Fixture.CompleteCurrentPlayback());
	TestTrue(TEXT("Applied playback drains"), Fixture.DrainPlayback());
	const FBattleHUDStatusView* DisplayedA2 = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	if (!TestNotNull(TEXT("Displayed Weak exists after Applied completion"), DisplayedA2))
	{
		return false;
	}
	TestEqual(TEXT("Displayed Weak amount after Applied"), DisplayedA2->Amount, 2);
	TestEqual(TEXT("Displayed Weak identity after Applied"), DisplayedA2->RuntimeSequence, static_cast<int64>(WeakASequence));
	TestEqual(TEXT("Two displayed statuses exercise ordering"), Fixture.ViewModel->Player.Statuses.Num(), 2);
	if (Fixture.ViewModel->Player.Statuses.Num() == 2)
	{
		TestEqual(TEXT("Anchor remains first displayed status"), Fixture.ViewModel->Player.Statuses[0].StatusId, FName(TEXT("AnchorStatus")));
		TestEqual(TEXT("Weak#A is second displayed status"), Fixture.ViewModel->Player.Statuses[1].StatusId, FName(TEXT("Weak")));
		TestTrue(
			TEXT("Displayed status order is RuntimeSequence ascending"),
			Fixture.ViewModel->Player.Statuses[0].RuntimeSequence < Fixture.ViewModel->Player.Statuses[1].RuntimeSequence
		);
	}

	UApplyStatusAction* IncreaseA = NewObject<UApplyStatusAction>(Queue);
	IncreaseA->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, Weak, 1);
	if (!TestTrue(TEXT("Weak#A increase action commits"), RunSystemAction(Fixture, IncreaseA)))
	{
		return false;
	}
	TestTrue(TEXT("Controller waits on Increased playback"), Fixture.Controller->IsWaitingForCompletionForTesting());
	const FBattleHUDStatusView* DisplayedBeforeIncrease = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	if (!TestNotNull(TEXT("Displayed Weak exists before Increased completion"), DisplayedBeforeIncrease))
	{
		return false;
	}
	TestEqual(TEXT("Displayed Weak does not advance early on Increased"), DisplayedBeforeIncrease->Amount, 2);
	TestEqual(TEXT("Gameplay Weak commits amount three"), WeakA->GetAmount(), 3);
	TestTrue(TEXT("Increased playback drains"), Fixture.DrainPlayback());
	const FBattleHUDStatusView* DisplayedA3 = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	if (!TestNotNull(TEXT("Displayed Weak exists after Increased"), DisplayedA3))
	{
		return false;
	}
	TestEqual(TEXT("Displayed Weak amount after Increased"), DisplayedA3->Amount, 3);
	TestEqual(TEXT("Increased keeps exact runtime identity"), DisplayedA3->RuntimeSequence, static_cast<int64>(WeakASequence));

	UReduceStatusAction* ReduceA = NewObject<UReduceStatusAction>(Queue);
	ReduceA->Initialize(
		Fixture.Battle,
		Fixture.Player,
		Fixture.Player,
		WeakA,
		1,
		EStatusChangeReason::Reduced
	);
	if (!TestTrue(TEXT("Weak#A reduce action commits"), RunSystemAction(Fixture, ReduceA)))
	{
		return false;
	}
	TestTrue(TEXT("Controller waits on Reduced playback"), Fixture.Controller->IsWaitingForCompletionForTesting());
	const FBattleHUDStatusView* DisplayedBeforeReduce = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	if (!TestNotNull(TEXT("Displayed Weak exists before Reduced completion"), DisplayedBeforeReduce))
	{
		return false;
	}
	TestEqual(TEXT("Displayed Weak does not advance early on Reduced"), DisplayedBeforeReduce->Amount, 3);
	TestEqual(TEXT("Gameplay Weak commits amount two"), WeakA->GetAmount(), 2);
	TestTrue(TEXT("Reduced playback drains"), Fixture.DrainPlayback());
	const FBattleHUDStatusView* DisplayedA2Again = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	if (!TestNotNull(TEXT("Displayed Weak exists after Reduced"), DisplayedA2Again))
	{
		return false;
	}
	TestEqual(TEXT("Displayed Weak amount after Reduced"), DisplayedA2Again->Amount, 2);

	const int32 CapturesBeforeEndTurn = Fixture.CapturedEnvelopes.Num();
	const FGameplayRequestResult EndTurn = Fixture.Battle->RequestEndPlayerTurn();
	TestTrue(TEXT("End-turn request accepted for Weak decay"), EndTurn.IsAcceptedForResolution());
	Fixture.Flush();
	TestTrue(TEXT("End-turn publishes at least one Envelope"), Fixture.CapturedEnvelopes.Num() > CapturesBeforeEndTurn);
	TestTrue(TEXT("End-turn Controller playback drains"), Fixture.DrainPlayback());
	TestEqual(TEXT("Gameplay returns to PlayerTurn after zero-damage enemy turn"), Fixture.Battle->BattleState, EBattleState::PlayerTurn);
	TestTrue(TEXT("Anchor survives turn cycle"), Container->ContainsStatusInstance(AnchorInstance));
	TestTrue(TEXT("Weak#A survives TurnEndDecay"), Container->ContainsStatusInstance(WeakA));
	TestEqual(TEXT("Gameplay Weak amount after TurnEndDecay"), WeakA->GetAmount(), 1);
	const FBattleHUDStatusView* DisplayedA1 = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	if (!TestNotNull(TEXT("Displayed Weak survives TurnEndDecay"), DisplayedA1))
	{
		return false;
	}
	TestEqual(TEXT("Displayed Weak amount after TurnEndDecay"), DisplayedA1->Amount, 1);
	TestEqual(TEXT("TurnEndDecay keeps Weak#A identity"), DisplayedA1->RuntimeSequence, static_cast<int64>(WeakASequence));

	URemoveStatusAction* RemoveA = NewObject<URemoveStatusAction>(Queue);
	RemoveA->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, WeakA);
	if (!TestTrue(TEXT("Weak#A explicit remove commits"), RunSystemAction(Fixture, RemoveA)))
	{
		return false;
	}
	TestTrue(TEXT("Controller waits on Removed playback"), Fixture.Controller->IsWaitingForCompletionForTesting());
	const FBattleHUDStatusView* DisplayedBeforeRemove = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	if (!TestNotNull(TEXT("Displayed Weak exists before Removed completion"), DisplayedBeforeRemove))
	{
		return false;
	}
	TestEqual(TEXT("Displayed Weak remains amount one before Removed completion"), DisplayedBeforeRemove->Amount, 1);
	TestFalse(TEXT("Gameplay Weak#A removed before presentation completion"), Container->ContainsStatusInstance(WeakA));
	TestEqual(TEXT("Stale Action remains pending after Weak#A removal"), StaleQueue->GetPendingCount(), 1);
	TestTrue(TEXT("Removed playback drains"), Fixture.DrainPlayback());
	TestTrue(TEXT("Displayed Weak removed after playback completion"), FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak")) == nullptr);
	TestNotNull(TEXT("Anchor remains displayed after Weak#A removal"), FindDisplayedStatus(Fixture.ViewModel, TEXT("AnchorStatus")));

	UApplyStatusAction* CreateB = NewObject<UApplyStatusAction>(Queue);
	CreateB->Initialize(Fixture.Battle, Fixture.Player, Fixture.Player, Weak, 2);
	if (!TestTrue(TEXT("Weak#B recreate action commits"), RunSystemAction(Fixture, CreateB)))
	{
		return false;
	}
	UStatusInstance* WeakB = FindMutableStatus(Container, TEXT("Weak"));
	if (!TestNotNull(TEXT("Weak#B exact runtime instance"), WeakB))
	{
		return false;
	}
	const uint64 WeakBSequence = WeakB->GetRuntimeSequence();
	TestTrue(TEXT("Weak#B is a different object"), WeakB != WeakA);
	TestTrue(TEXT("Weak#B RuntimeSequence is newer than Weak#A"), WeakBSequence > WeakASequence);
	TestTrue(TEXT("Weak#B RuntimeSequence remains after Anchor"), WeakBSequence > AnchorSequence);
	TestEqual(TEXT("Stale Action is still pending after Weak#B recreate"), StaleQueue->GetPendingCount(), 1);
	TestTrue(TEXT("Weak#B playback drains"), Fixture.DrainPlayback());
	const FBattleHUDStatusView* DisplayedB2 = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	if (!TestNotNull(TEXT("Displayed Weak#B exists after recreate"), DisplayedB2))
	{
		return false;
	}
	TestEqual(TEXT("Displayed Weak#B amount"), DisplayedB2->Amount, 2);
	TestEqual(TEXT("Displayed Weak#B concrete identity"), DisplayedB2->RuntimeSequence, static_cast<int64>(WeakBSequence));
	TestEqual(TEXT("Two displayed statuses remain after recreate"), Fixture.ViewModel->Player.Statuses.Num(), 2);
	if (Fixture.ViewModel->Player.Statuses.Num() == 2)
	{
		TestEqual(TEXT("Anchor remains first after Weak recreate"), Fixture.ViewModel->Player.Statuses[0].StatusId, FName(TEXT("AnchorStatus")));
		TestEqual(TEXT("Weak#B remains second after recreate"), Fixture.ViewModel->Player.Statuses[1].StatusId, FName(TEXT("Weak")));
		TestTrue(
			TEXT("Recreated displayed statuses remain RuntimeSequence ordered"),
			Fixture.ViewModel->Player.Statuses[0].RuntimeSequence < Fixture.ViewModel->Player.Statuses[1].RuntimeSequence
		);
	}

	const int32 PlayedBeforeStale = Fixture.Widget->PlayCallCount;
	const int32 CapturesBeforeStale = Fixture.CapturedEnvelopes.Num();
	if (!TestTrue(
		TEXT("Already-pending stale Weak#A reduce executes in a new formal system Resolution"),
		RunAlreadyPendingSystemAction(Fixture, StaleQueue.Get(), StaleReduce.Get())
	))
	{
		return false;
	}
	TestEqual(TEXT("Stale holding queue drains"), StaleQueue->GetPendingCount(), 0);
	TestEqual(TEXT("Stale exact-instance NoOp produces no visible Record"), Fixture.Widget->PlayCallCount, PlayedBeforeStale);
	TestTrue(TEXT("Stale empty/no-op playback remains caught up"), Fixture.DrainPlayback());
	TestTrue(TEXT("Weak#B remains authoritative member"), Container->ContainsStatusInstance(WeakB));
	TestFalse(TEXT("Weak#A remains absent"), Container->ContainsStatusInstance(WeakA));
	TestEqual(TEXT("Stale Weak#A reduce cannot touch Weak#B amount"), WeakB->GetAmount(), 2);
	const FBattleHUDStatusView* DisplayedAfterStale = FindDisplayedStatus(Fixture.ViewModel, TEXT("Weak"));
	if (!TestNotNull(TEXT("Displayed Weak#B survives stale action"), DisplayedAfterStale))
	{
		return false;
	}
	TestEqual(TEXT("Displayed Weak#B amount unchanged by stale action"), DisplayedAfterStale->Amount, 2);
	TestEqual(TEXT("Displayed Weak#B identity unchanged by stale action"), DisplayedAfterStale->RuntimeSequence, static_cast<int64>(WeakBSequence));

	// Empty/no-op Resolution publication is optional. If the current producer does
	// publish one, it must contain no committed Records. Do not require the
	// Envelope itself to exist.
	for (int32 Index = CapturesBeforeStale; Index < Fixture.CapturedEnvelopes.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Optional stale Envelope[%d] contains no Records"), Index),
			Fixture.CapturedEnvelopes[Index].Envelope.Records.Num(),
			0
		);
	}

	TestTrue(
		TEXT("Captured Envelope order is monotonic"),
		AssertCapturedEnvelopeOrder(*this, Fixture.CapturedEnvelopes, TEXT("StatusLifecycle"))
	);
	for (int32 Index = 0; Index < Fixture.CapturedEnvelopes.Num(); ++Index)
	{
		const FCapturedEnvelope& Capture = Fixture.CapturedEnvelopes[Index];
		TestTrue(
			*FString::Printf(TEXT("Envelope[%d] reducer-owned state matches FinalSnapshot"), Index),
			AssertReducerOwnedStateMatchesFinalSnapshot(
				*this,
				Capture.Baseline,
				Capture.Envelope,
				FString::Printf(TEXT("StatusLifecycle Envelope[%d]"), Index)
			)
		);
	}

	TestTrue(
		TEXT("Controller playback records/tokens match captured history in producer order"),
		AssertControllerPlaybackMatchesCapturedHistory(
			*this,
			Fixture.CapturedEnvelopes,
			Fixture.Widget,
			TEXT("StatusLifecycle Controller")
		)
	);

	TArray<FStatusChangedPresentationPayload> History;
	GatherStatusHistory(Fixture.CapturedEnvelopes, History);
	if (!TestEqual(TEXT("Status lifecycle emits exactly six committed Weak StatusChanged records"), History.Num(), 6))
	{
		return false;
	}

	const EStatusChangeReason ExpectedReasons[6] = {
		EStatusChangeReason::Applied,
		EStatusChangeReason::Increased,
		EStatusChangeReason::Reduced,
		EStatusChangeReason::TurnEndDecay,
		EStatusChangeReason::Removed,
		EStatusChangeReason::Applied
	};
	const int32 ExpectedBefore[6] = {0, 2, 3, 2, 1, 0};
	const int32 ExpectedAfter[6] = {2, 3, 2, 1, 0, 2};
	const bool ExpectedCreated[6] = {true, false, false, false, false, true};
	const bool ExpectedRemoved[6] = {false, false, false, false, true, false};

	for (int32 Index = 0; Index < History.Num(); ++Index)
	{
		const FStatusChangedPresentationPayload& Payload = History[Index];
		const FString Context = FString::Printf(TEXT("Status history[%d]"), Index);
		TestEqual(*FString::Printf(TEXT("%s StatusId"), *Context), Payload.StatusId, FName(TEXT("Weak")));
		TestEqual(*FString::Printf(TEXT("%s source"), *Context), Payload.SourcePresentationId, FName(TEXT("PlayerHero")));
		TestEqual(*FString::Printf(TEXT("%s target"), *Context), Payload.TargetPresentationId, FName(TEXT("PlayerHero")));
		TestTrue(*FString::Printf(TEXT("%s reason"), *Context), Payload.Reason == ExpectedReasons[Index]);
		TestEqual(*FString::Printf(TEXT("%s amount before"), *Context), Payload.AmountBefore, ExpectedBefore[Index]);
		TestEqual(*FString::Printf(TEXT("%s amount after"), *Context), Payload.AmountAfter, ExpectedAfter[Index]);
		TestEqual(*FString::Printf(TEXT("%s bCreated"), *Context), Payload.bCreated, ExpectedCreated[Index]);
		TestEqual(*FString::Printf(TEXT("%s bRemoved"), *Context), Payload.bRemoved, ExpectedRemoved[Index]);
		TestEqual(*FString::Printf(TEXT("%s DisplayName"), *Context), Payload.DisplayName.ToString(), FString(TEXT("Weak Lifecycle")));
		ValidateFrozenIcon(*this, Payload, Context);

		const int64 ExpectedSequence = Index < 5
			? static_cast<int64>(WeakASequence)
			: static_cast<int64>(WeakBSequence);
		TestEqual(*FString::Printf(TEXT("%s RuntimeSequence"), *Context), Payload.RuntimeSequence, ExpectedSequence);
	}

	TestTrue(TEXT("Applied description before empty"), History[0].DescriptionBefore.IsEmpty());
	TestEqual(TEXT("Applied description after"), History[0].DescriptionAfter.ToString(), FString(TEXT("Weak amount 2.")));
	TestEqual(TEXT("Increased description before"), History[1].DescriptionBefore.ToString(), FString(TEXT("Weak amount 2.")));
	TestEqual(TEXT("Increased description after"), History[1].DescriptionAfter.ToString(), FString(TEXT("Weak amount 3.")));
	TestEqual(TEXT("Reduced description before"), History[2].DescriptionBefore.ToString(), FString(TEXT("Weak amount 3.")));
	TestEqual(TEXT("Reduced description after"), History[2].DescriptionAfter.ToString(), FString(TEXT("Weak amount 2.")));
	TestEqual(TEXT("TurnEndDecay description before"), History[3].DescriptionBefore.ToString(), FString(TEXT("Weak amount 2.")));
	TestEqual(TEXT("TurnEndDecay description after"), History[3].DescriptionAfter.ToString(), FString(TEXT("Weak amount 1.")));
	TestEqual(TEXT("Removed description before"), History[4].DescriptionBefore.ToString(), FString(TEXT("Weak amount 1.")));
	TestTrue(TEXT("Removed description after empty"), History[4].DescriptionAfter.IsEmpty());
	TestTrue(TEXT("Recreated description before empty"), History[5].DescriptionBefore.IsEmpty());
	TestEqual(TEXT("Recreated description after"), History[5].DescriptionAfter.ToString(), FString(TEXT("Weak amount 2.")));
	TestEqual(TEXT("Stale exact-instance mutation adds no Weak StatusChanged history"), History.Num(), 6);
	return true;
}

#endif
