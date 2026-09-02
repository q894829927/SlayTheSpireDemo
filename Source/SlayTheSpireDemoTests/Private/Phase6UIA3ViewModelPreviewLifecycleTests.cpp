#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA3NativePreviewTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace Phase6UIA1Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FViewModelPreviewTargetNominationTest,
	"SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle.TargetNominationAndClearAreTransient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FViewModelPreviewTargetNominationTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy, 0, 5);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	const int32 RuntimeId = Fixture.FirstRuntimeId();
	TestTrue(TEXT("Enemy-target card can be selected"), Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId));
	TestEqual(TEXT("Selection enters target choice"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::ChoosingTarget);
	TestEqual(TEXT("One legal target is exposed"), Fixture.ViewModel->LegalTargets.Num(), 1);
	TestFalse(TEXT("Card selection alone does not nominate a PreviewTarget"), Fixture.ViewModel->bHasImmediatePreview);
	TestEqual(TEXT("Card selection starts without PreviewTargetId"), Fixture.ViewModel->PreviewTargetId, INDEX_NONE);
	TestTrue(TEXT("Card selection starts without PreviewTarget presentation identity"), Fixture.ViewModel->PreviewTargetPresentationId.IsNone());
	if (Fixture.ViewModel->LegalTargets.Num() != 1) return false;

	const int32 TargetId = Fixture.ViewModel->LegalTargets[0].TargetId;
	const FName TargetPresentationId = Fixture.ViewModel->LegalTargets[0].PresentationId;
	const EBattleHUDInteractionState InteractionBeforePreview = Fixture.ViewModel->InteractionState;
	const int32 LegalTargetCountBeforePreview = Fixture.ViewModel->LegalTargets.Num();

	TestTrue(TEXT("Legal target can nominate the current PreviewTarget"), Fixture.ViewModel->SetPreviewTargetById(TargetId));
	TestTrue(TEXT("Target nomination publishes an ImmediatePreview"), Fixture.ViewModel->bHasImmediatePreview);
	TestEqual(TEXT("PreviewTarget keeps the legal target id"), Fixture.ViewModel->PreviewTargetId, TargetId);
	TestEqual(TEXT("PreviewTarget keeps presentation identity"), Fixture.ViewModel->PreviewTargetPresentationId, TargetPresentationId);
	TestEqual(TEXT("Preview stamps current ViewModel BattleId"), Fixture.ViewModel->ImmediatePreview.BattleId, Fixture.ViewModel->BattleId);
	TestEqual(TEXT("Preview stamps current ViewModel StateRevision"), Fixture.ViewModel->ImmediatePreview.StateRevision, Fixture.ViewModel->StateRevision);
	TestEqual(TEXT("Preview stamps selected CardRuntimeId"), Fixture.ViewModel->ImmediatePreview.CardRuntimeId, RuntimeId);
	TestEqual(TEXT("Preview stamps target presentation identity"), Fixture.ViewModel->ImmediatePreview.TargetPresentationId, TargetPresentationId);
	TestEqual(TEXT("Preview nomination does not alter card-selection interaction state"), Fixture.ViewModel->InteractionState, InteractionBeforePreview);
	TestEqual(TEXT("Preview nomination does not rewrite legal targets"), Fixture.ViewModel->LegalTargets.Num(), LegalTargetCountBeforePreview);
	TestEqual(TEXT("Preview nomination keeps selected card identity"), Fixture.ViewModel->SelectedCardRuntimeId, RuntimeId);

	Fixture.ViewModel->ClearPreviewTarget();
	TestFalse(TEXT("Target unfocus clears ImmediatePreview"), Fixture.ViewModel->bHasImmediatePreview);
	TestEqual(TEXT("Target unfocus clears PreviewTargetId"), Fixture.ViewModel->PreviewTargetId, INDEX_NONE);
	TestTrue(TEXT("Target unfocus clears PreviewTarget presentation identity"), Fixture.ViewModel->PreviewTargetPresentationId.IsNone());
	TestEqual(TEXT("Target unfocus clears stale Preview DTO"), Fixture.ViewModel->ImmediatePreview.BattleId, int64(0));
	TestEqual(TEXT("Target unfocus preserves selected card"), Fixture.ViewModel->SelectedCardRuntimeId, RuntimeId);
	TestEqual(TEXT("Target unfocus preserves legal targets"), Fixture.ViewModel->LegalTargets.Num(), LegalTargetCountBeforePreview);
	TestEqual(TEXT("Preview lifecycle stays separate from card interaction state"), Fixture.ViewModel->InteractionState, InteractionBeforePreview);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FViewModelPreviewCancelAndSubmitClearTest,
	"SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle.CancelAndAcceptedSubmissionClearPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FViewModelPreviewCancelAndSubmitClearTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy, 0, 5);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	const int32 RuntimeId = Fixture.FirstRuntimeId();
	TestTrue(TEXT("Initial card selection succeeds"), Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId));
	if (Fixture.ViewModel->LegalTargets.Num() != 1)
	{
		AddError(TEXT("Expected one Enemy legal target for Preview lifecycle test."));
		return false;
	}
	const int32 TargetId = Fixture.ViewModel->LegalTargets[0].TargetId;
	TestTrue(TEXT("Initial PreviewTarget nomination succeeds"), Fixture.ViewModel->SetPreviewTargetById(TargetId));
	TestTrue(TEXT("Initial Preview exists"), Fixture.ViewModel->bHasImmediatePreview);

	Fixture.ViewModel->CancelSelection();
	TestEqual(TEXT("Cancel clears selected card"), Fixture.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestEqual(TEXT("Cancel clears legal targets"), Fixture.ViewModel->LegalTargets.Num(), 0);
	TestFalse(TEXT("Cancel clears ImmediatePreview"), Fixture.ViewModel->bHasImmediatePreview);
	TestEqual(TEXT("Cancel clears PreviewTargetId"), Fixture.ViewModel->PreviewTargetId, INDEX_NONE);
	TestTrue(TEXT("Cancel clears PreviewTarget presentation identity"), Fixture.ViewModel->PreviewTargetPresentationId.IsNone());
	TestEqual(TEXT("Cancel does not mutate authoritative Hand display"), Fixture.ViewModel->HandCards.Num(), 1);

	TestTrue(TEXT("Card can be reselected after Cancel"), Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId));
	if (Fixture.ViewModel->LegalTargets.Num() != 1)
	{
		AddError(TEXT("Expected one Enemy legal target after reselection."));
		return false;
	}
	const int32 SubmitTargetId = Fixture.ViewModel->LegalTargets[0].TargetId;
	TestTrue(TEXT("Preview can be rebuilt before authoritative submission"), Fixture.ViewModel->SetPreviewTargetById(SubmitTargetId));
	TestTrue(TEXT("Preview exists immediately before target submission"), Fixture.ViewModel->bHasImmediatePreview);

	TestTrue(TEXT("Authoritative target submission is accepted"), Fixture.ViewModel->SelectTargetById(SubmitTargetId));
	TestFalse(TEXT("Accepted request clears A3 Preview"), Fixture.ViewModel->bHasImmediatePreview);
	TestEqual(TEXT("Accepted request clears PreviewTargetId"), Fixture.ViewModel->PreviewTargetId, INDEX_NONE);
	TestTrue(TEXT("Accepted request clears PreviewTarget presentation identity"), Fixture.ViewModel->PreviewTargetPresentationId.IsNone());
	TestEqual(TEXT("Accepted request clears selected card identity"), Fixture.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestEqual(TEXT("Accepted request clears legal targets"), Fixture.ViewModel->LegalTargets.Num(), 0);
	TestEqual(TEXT("Accepted request hands interaction to Resolving"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Resolving);
	TestTrue(TEXT("Accepted request locks input before committed Presentation catches up"), Fixture.ViewModel->bInputLocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FViewModelPreviewRevisionClearTest,
	"SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle.RevisionChangeClearsBeforePresentationCatchUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FViewModelPreviewRevisionClearTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy, 0, 5);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	UPhase6UIA3PreviewEventSink* Sink = NewObject<UPhase6UIA3PreviewEventSink>(Fixture.World);
	if (!TestNotNull(TEXT("Revision notification sink exists"), Sink))
	{
		return false;
	}
	Sink->ObserveViewModel(Fixture.ViewModel);

	const int32 RuntimeId = Fixture.FirstRuntimeId();
	TestTrue(TEXT("Card selection succeeds before external revision"), Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId));
	if (Fixture.ViewModel->LegalTargets.Num() != 1)
	{
		AddError(TEXT("Expected one Enemy legal target before external revision."));
		return false;
	}
	TestTrue(
		TEXT("Preview exists before external revision"),
		Fixture.ViewModel->SetPreviewTargetById(Fixture.ViewModel->LegalTargets[0].TargetId)
	);

	const int64 DisplayBattleIdBefore = Fixture.ViewModel->BattleId;
	const int64 DisplayRevisionBefore = Fixture.ViewModel->StateRevision;
	Fixture.ViewModel->SetPresentationDisplayOwned(true);
	const int32 StructuralChangesBeforeReady = Sink->StructuralChangedCount;
	const int32 PreviewChangesBeforeReady = Sink->PreviewChangedCount;

	const FGameplayRequestResult EndTurnResult = Fixture.Battle->RequestEndPlayerTurn();
	TestTrue(TEXT("External authoritative EndTurn is accepted"), EndTurnResult.IsAcceptedForResolution());
	TestEqual(TEXT("Before Ready edge the old selected card is still displayed"), Fixture.ViewModel->SelectedCardRuntimeId, RuntimeId);
	TestTrue(TEXT("Before Ready edge the old Preview is still present"), Fixture.ViewModel->bHasImmediatePreview);

	Fixture.FlushReady();

	// Presentation ownership deliberately prevents the ViewModel from applying the
	// new frozen snapshot here. The read-ready edge must invalidate old-revision
	// interaction immediately without publishing structural OnChanged: public
	// Presentation delivery runs before this edge, so a structural notification
	// here would cancel a valid CardPlayed visual that has already started.
	TestEqual(TEXT("Presentation-owned ViewModel does not jump display BattleId ahead"), Fixture.ViewModel->BattleId, DisplayBattleIdBefore);
	TestEqual(TEXT("Presentation-owned ViewModel does not jump display revision ahead"), Fixture.ViewModel->StateRevision, DisplayRevisionBefore);
	TestEqual(TEXT("New Gameplay revision clears stale selected card before catch-up"), Fixture.ViewModel->SelectedCardRuntimeId, INDEX_NONE);
	TestEqual(TEXT("New Gameplay revision clears stale legal targets before catch-up"), Fixture.ViewModel->LegalTargets.Num(), 0);
	TestFalse(TEXT("New Gameplay revision clears stale ImmediatePreview before catch-up"), Fixture.ViewModel->bHasImmediatePreview);
	TestEqual(TEXT("New Gameplay revision clears stale PreviewTargetId before catch-up"), Fixture.ViewModel->PreviewTargetId, INDEX_NONE);
	TestTrue(TEXT("New Gameplay revision clears stale PreviewTarget presentation identity"), Fixture.ViewModel->PreviewTargetPresentationId.IsNone());
	TestEqual(TEXT("New Gameplay revision clears stale Preview DTO"), Fixture.ViewModel->ImmediatePreview.StateRevision, int64(0));
	TestEqual(TEXT("ViewModel waits in Resolving for Presentation catch-up"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Resolving);
	TestTrue(TEXT("ViewModel stays input-locked until Presentation catch-up"), Fixture.ViewModel->bInputLocked);
	TestEqual(TEXT("Presentation-owned Ready invalidation does not emit structural OnChanged"), Sink->StructuralChangedCount, StructuralChangesBeforeReady);
	TestEqual(TEXT("Presentation-owned Ready invalidation refreshes only transient Preview"), Sink->PreviewChangedCount, PreviewChangesBeforeReady + 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
