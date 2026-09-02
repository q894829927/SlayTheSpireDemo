#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA3NativePreviewTestTypes.h"
#include "Battle/BattleImmediatePreview.h"
#include "Components/Overlay.h"
#include "UI/BattleImmediatePreviewTextBlock.h"
#include "UI/BattleHUDViewModel.h"

using namespace Phase6UIA1Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativePreviewEventsIndependentTest,
	"SlayTheSpireDemo.UIA3.NativePreviewIntegration.DedicatedPreviewEventsStayIndependentFromInspection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNativePreviewEventsIndependentTest::RunTest(const FString& Parameters)
{
	UPhase6UIA3PreviewCombatantProbe* Presentation =
		NewObject<UPhase6UIA3PreviewCombatantProbe>(GetTransientPackage());
	UPhase6UIA3PreviewEventSink* Sink =
		NewObject<UPhase6UIA3PreviewEventSink>(GetTransientPackage());
	if (!TestNotNull(TEXT("Combatant Preview probe exists"), Presentation)
		|| !TestNotNull(TEXT("Preview event sink exists"), Sink))
	{
		return false;
	}

	Presentation->OnInspectRequested.AddUniqueDynamic(
		Sink,
		&UPhase6UIA3PreviewEventSink::HandleInspectRequested);
	Presentation->OnInspectCleared.AddUniqueDynamic(
		Sink,
		&UPhase6UIA3PreviewEventSink::HandleInspectCleared);
	Presentation->OnPreviewRequested.AddUniqueDynamic(
		Sink,
		&UPhase6UIA3PreviewEventSink::HandlePreviewRequested);
	Presentation->OnPreviewCleared.AddUniqueDynamic(
		Sink,
		&UPhase6UIA3PreviewEventSink::HandlePreviewCleared);

	FBattleHUDCombatantView Combatant;
	Combatant.PresentationId = TEXT("PreviewEnemy");
	Combatant.DisplayName = FText::FromString(TEXT("Preview Enemy"));
	Presentation->SetPresentationData(Combatant, true, true, 7, true);
	Presentation->SetPointerInspectionActive(true);

	TestTrue(TEXT("Pointer hover activates inspection"), Presentation->IsTransientInspectionActive());
	TestEqual(TEXT("Inspection publishes its own request"), Sink->InspectRequestedCount, 1);
	TestEqual(TEXT("Preview publishes through a distinct delegate"), Sink->PreviewRequestedCount, 1);
	TestEqual(TEXT("Preview delegate carries current gameplay target id"), Sink->LastPreviewTargetId, 7);

	// Keep pointer inspection active while target-selection eligibility disappears.
	// Preview must clear without clearing inspection.
	Presentation->SetPresentationData(Combatant, false, false, INDEX_NONE, false);
	TestTrue(TEXT("Inspection remains active when Preview eligibility disappears"), Presentation->IsTransientInspectionActive());
	TestEqual(TEXT("Preview clears independently"), Sink->PreviewClearedCount, 1);
	TestEqual(TEXT("Preview clear does not emit inspection clear"), Sink->InspectClearedCount, 0);

	Presentation->SetPresentationData(Combatant, true, true, 8, true);
	TestEqual(TEXT("Stationary hover can nominate the new Preview target"), Sink->PreviewRequestedCount, 2);
	TestEqual(TEXT("Republished Preview uses the new target id"), Sink->LastPreviewTargetId, 8);

	Presentation->SetPointerInspectionActive(false);
	TestFalse(TEXT("Pointer leave clears inspection"), Presentation->IsTransientInspectionActive());
	TestEqual(TEXT("Pointer leave clears inspection once"), Sink->InspectClearedCount, 1);
	TestEqual(TEXT("Pointer leave also clears active Preview once"), Sink->PreviewClearedCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativePreviewSurfaceFormattingTest,
	"SlayTheSpireDemo.UIA3.NativePreviewIntegration.NativeSurfaceRendersAndClearsFromViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNativePreviewSurfaceFormattingTest::RunTest(const FString& Parameters)
{
	UPhase6UIA3PreviewHUDProbe* HUD =
		NewObject<UPhase6UIA3PreviewHUDProbe>(GetTransientPackage());
	UBattleHUDViewModel* ViewModel = NewObject<UBattleHUDViewModel>(HUD);
	UOverlay* Overlay = NewObject<UOverlay>(HUD);
	if (!TestNotNull(TEXT("Native HUD Preview probe exists"), HUD)
		|| !TestNotNull(TEXT("Preview ViewModel exists"), ViewModel)
		|| !TestNotNull(TEXT("Preview host Overlay exists"), Overlay))
	{
		return false;
	}

	ViewModel->BattleId = 1;
	ViewModel->StateRevision = 4;
	ViewModel->SelectedCardRuntimeId = 17;
	ViewModel->PreviewTargetId = 1;
	ViewModel->PreviewTargetPresentationId = TEXT("PreviewEnemy");
	ViewModel->bHasImmediatePreview = true;
	ViewModel->ImmediatePreview.BattleId = 1;
	ViewModel->ImmediatePreview.StateRevision = 4;
	ViewModel->ImmediatePreview.CardRuntimeId = 17;
	ViewModel->ImmediatePreview.SourcePresentationId = TEXT("PreviewPlayer");
	ViewModel->ImmediatePreview.TargetPresentationId = TEXT("PreviewEnemy");
	ViewModel->ImmediatePreview.Validation = FGameplayValidationResult::Allowed();
	ViewModel->ImmediatePreview.EnergyBefore = 3;
	ViewModel->ImmediatePreview.EffectiveCost = 1;
	ViewModel->ImmediatePreview.bHasEnergyAfter = true;
	ViewModel->ImmediatePreview.EnergyAfter = 2;

	FImmediatePreviewOperation Damage;
	Damage.EffectIndex = 0;
	Damage.SemanticArgumentName = TEXT("Damage");
	Damage.Type = EImmediatePreviewOperationType::Damage;
	Damage.ResolvedAmount = 9;
	Damage.HitCount = 2;
	ViewModel->ImmediatePreview.Operations.Add(Damage);

	FImmediatePreviewOperation Block;
	Block.EffectIndex = 1;
	Block.SemanticArgumentName = TEXT("Block");
	Block.Type = EImmediatePreviewOperationType::Block;
	Block.ResolvedAmount = 8;
	Block.HitCount = 1;
	ViewModel->ImmediatePreview.Operations.Add(Block);

	HUD->ConfigurePreviewSurface(ViewModel, Overlay);
	TestEqual(TEXT("Valid A3 Preview owns exactly one transient PlayArea child"), Overlay->GetChildrenCount(), 1);
	if (Overlay->GetChildrenCount() != 1)
	{
		return false;
	}

	UBattleImmediatePreviewTextBlock* PreviewText =
		Cast<UBattleImmediatePreviewTextBlock>(Overlay->GetChildAt(0));
	if (!TestNotNull(TEXT("PlayArea child is the dedicated A3 Preview surface"), PreviewText))
	{
		return false;
	}

	const FString Display = PreviewText->GetText().ToString();
	TestTrue(TEXT("Preview preserves per-hit multi-hit formatting"), Display.Contains(TEXT("Damage 9 x 2")));
	TestTrue(TEXT("Preview renders supported Block operation"), Display.Contains(TEXT("Block 8")));
	TestTrue(TEXT("Preview renders authoritative Energy transition"), Display.Contains(TEXT("Energy 3 -> 2")));

	ViewModel->ClearPreviewTarget();
	TestEqual(TEXT("Clearing A3 Preview removes the transient PlayArea child synchronously"), Overlay->GetChildrenCount(), 0);
	TestTrue(TEXT("Clearing A3 Preview clears formatted text authority"), ViewModel->GetImmediatePreviewDisplayText().IsEmpty());

	HUD->ReleasePreviewSurfaceForTesting();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativePreviewPreRequestHandoffTest,
	"SlayTheSpireDemo.UIA3.NativePreviewIntegration.TargetSubmissionClearsPreviewBeforeAuthoritativeRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNativePreviewPreRequestHandoffTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy, 0, 5);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	UPhase6UIA3PreviewHUDProbe* HUD =
		NewObject<UPhase6UIA3PreviewHUDProbe>(Fixture.World);
	UOverlay* Overlay = NewObject<UOverlay>(HUD);
	UPhase6UIA3PreviewEventSink* Sink =
		NewObject<UPhase6UIA3PreviewEventSink>(Fixture.World);
	if (!TestNotNull(TEXT("Native HUD handoff probe exists"), HUD)
		|| !TestNotNull(TEXT("Handoff Preview overlay exists"), Overlay)
		|| !TestNotNull(TEXT("Handoff event sink exists"), Sink))
	{
		return false;
	}

	HUD->ConfigurePreviewSurface(Fixture.ViewModel, Overlay);
	const int32 RuntimeId = Fixture.FirstRuntimeId();
	TestTrue(TEXT("Card selection succeeds before A3 handoff"), Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId));
	if (Fixture.ViewModel->LegalTargets.Num() != 1)
	{
		AddError(TEXT("Expected one Enemy legal target for A3-5 handoff."));
		return false;
	}

	const int32 TargetId = Fixture.ViewModel->LegalTargets[0].TargetId;
	TestTrue(TEXT("Target-specific Preview builds before submission"), Fixture.ViewModel->SetPreviewTargetById(TargetId));
	TestTrue(TEXT("Preview is live immediately before submission"), Fixture.ViewModel->bHasImmediatePreview);
	TestEqual(TEXT("Native A3 surface is present before submission"), Overlay->GetChildrenCount(), 1);

	Sink->ObserveViewModel(Fixture.ViewModel);
	TestTrue(TEXT("Native target submission is accepted"), HUD->SelectTarget(TargetId));

	TestTrue(
		TEXT("ViewModel broadcast exposes Preview-cleared state while selection is still pre-request"),
		Sink->bObservedPreRequestPreviewClear);
	TestEqual(
		TEXT("A3 surface is removed before committed A2 PlayArea ownership"),
		Overlay->GetChildrenCount(),
		0);
	TestFalse(TEXT("Accepted request leaves no A3 Preview DTO"), Fixture.ViewModel->bHasImmediatePreview);
	TestEqual(TEXT("Accepted request enters Resolving"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Resolving);
	TestTrue(TEXT("Accepted request locks input until committed Presentation catch-up"), Fixture.ViewModel->bInputLocked);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
