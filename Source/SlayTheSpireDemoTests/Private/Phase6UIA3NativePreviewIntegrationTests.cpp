#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA1TestFixture.h"
#include "Phase6UIA2NR4TestTypes.h"
#include "Phase6UIA3NativePreviewTestTypes.h"
#include "Battle/BattleImmediatePreview.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/BattleHUDViewModel.h"

using namespace Phase6UIA1Test;

namespace
{
	UPhase6UIA2NR4CardProbe* MakePreviewCardProbe(
		UObject* Outer,
		const FBattleHUDCardView& View,
		UTextBlock*& OutDescription)
	{
		UPhase6UIA2NR4CardProbe* Card = NewObject<UPhase6UIA2NR4CardProbe>(Outer);
		UButton* Button = NewObject<UButton>(Card);
		UTextBlock* Name = NewObject<UTextBlock>(Card);
		UTextBlock* Cost = NewObject<UTextBlock>(Card);
		OutDescription = NewObject<UTextBlock>(Card);
		UTextBlock* Type = NewObject<UTextBlock>(Card);
		UImage* Art = NewObject<UImage>(Card);
		Card->ConfigureSurfaces(Button, Name, Cost, OutDescription, Type, Art);
		Card->SetCardView(View);
		return Card;
	}
}

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
	FNativePreviewCardFaceFormattingTest,
	"SlayTheSpireDemo.UIA3.NativePreviewIntegration.SelectedCardFaceRendersAndRestoresPreviewValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNativePreviewCardFaceFormattingTest::RunTest(const FString& Parameters)
{
	FBattleHUDCardView CardView;
	CardView.RuntimeId = 17;
	CardView.CardId = TEXT("StrikeProbe");
	CardView.DisplayName = FText::FromString(TEXT("Strike"));
	CardView.Cost = 1;
	CardView.CardType = ECardType::Attack;
	CardView.TargetType = ECardTargetType::Enemy;
	CardView.Description = FText::FromString(TEXT("Deal 6 damage."));

	UTextBlock* Description = nullptr;
	UPhase6UIA2NR4CardProbe* Card = MakePreviewCardProbe(GetTransientPackage(), CardView, Description);
	if (!TestNotNull(TEXT("Card-face Preview probe exists"), Card)
		|| !TestNotNull(TEXT("Card-face description surface exists"), Description))
	{
		return false;
	}

	FImmediateCardPreview Preview;
	Preview.CardRuntimeId = 17;
	Preview.Validation = FGameplayValidationResult::Allowed();
	Preview.CardFaceDescription = FText::FromString(TEXT("Deal 9 damage."));
	FImmediatePreviewOperation Damage;
	Damage.EffectIndex = 0;
	Damage.SemanticArgumentName = TEXT("Damage");
	Damage.Type = EImmediatePreviewOperationType::Damage;
	Damage.BaseAmount = 6;
	Damage.ResolvedAmount = 9;
	Damage.HitCount = 1;
	Preview.Operations.Add(Damage);

	Card->ApplyImmediatePreview(Preview);
	TestEqual(TEXT("Target-specific Damage replaces selected card-face value"), Description->GetText().ToString(), FString(TEXT("Deal 9 damage.")));
	TestEqual(TEXT("Value above authored base selects red/increased emphasis branch"), Card->GetImmediatePreviewToneForTesting(), static_cast<int8>(1));

	Preview.CardFaceDescription = FText::FromString(TEXT("Deal 4 damage."));
	Preview.Operations[0].ResolvedAmount = 4;
	Card->ApplyImmediatePreview(Preview);
	TestEqual(TEXT("Decreased target-specific Damage also replaces card-face value"), Description->GetText().ToString(), FString(TEXT("Deal 4 damage.")));
	TestEqual(TEXT("Value below authored base selects cool/decreased emphasis branch"), Card->GetImmediatePreviewToneForTesting(), static_cast<int8>(-1));

	Preview.CardFaceDescription = FText::FromString(TEXT("Deal 6 damage."));
	Preview.Operations[0].ResolvedAmount = 6;
	Card->ApplyImmediatePreview(Preview);
	TestEqual(TEXT("Authored-base value remains the normal card-face value"), Description->GetText().ToString(), FString(TEXT("Deal 6 damage.")));
	TestEqual(TEXT("Value equal to authored base selects neutral emphasis branch"), Card->GetImmediatePreviewToneForTesting(), static_cast<int8>(0));

	Card->ClearImmediatePreview();
	TestEqual(TEXT("Clearing Preview restores frozen card-face description"), Description->GetText().ToString(), FString(TEXT("Deal 6 damage.")));
	TestEqual(TEXT("Clearing Preview restores neutral description styling"), Card->GetImmediatePreviewToneForTesting(), static_cast<int8>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativePreviewPreRequestHandoffTest,
	"SlayTheSpireDemo.UIA3.NativePreviewIntegration.TargetSubmissionClearsPreviewBeforeAuthoritativeRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNativePreviewPreRequestHandoffTest::RunTest(const FString& Parameters)
{
	FHUDTestFixture Fixture(ECardTargetType::Enemy, 1, 5);
	Fixture.DrainInitialReady();
	Fixture.InitializeViewModel();
	if (!RequireFixture(*this, Fixture)) return false;

	UPhase6UIA3PreviewHUDProbe* HUD = NewObject<UPhase6UIA3PreviewHUDProbe>(Fixture.World);
	UHorizontalBox* Hand = NewObject<UHorizontalBox>(HUD);
	UPhase6UIA3PreviewEventSink* Sink = NewObject<UPhase6UIA3PreviewEventSink>(Fixture.World);
	if (!TestNotNull(TEXT("Native HUD handoff probe exists"), HUD)
		|| !TestNotNull(TEXT("Handoff Hand exists"), Hand)
		|| !TestNotNull(TEXT("Handoff event sink exists"), Sink))
	{
		return false;
	}

	if (Fixture.ViewModel->HandCards.Num() != 1)
	{
		AddError(TEXT("Expected one displayed Hand card for A3-5 handoff."));
		return false;
	}

	UTextBlock* CardDescription = nullptr;
	UPhase6UIA2NR4CardProbe* Card = MakePreviewCardProbe(HUD, Fixture.ViewModel->HandCards[0], CardDescription);
	Hand->AddChildToHorizontalBox(Card);
	HUD->ConfigurePreviewSurface(Fixture.ViewModel, Hand);

	const int32 RuntimeId = Fixture.FirstRuntimeId();
	TestTrue(TEXT("Card selection succeeds before A3 handoff"), Fixture.ViewModel->SelectCardByRuntimeId(RuntimeId));
	if (Fixture.ViewModel->LegalTargets.Num() != 1)
	{
		AddError(TEXT("Expected one Enemy legal target for A3-5 handoff."));
		return false;
	}

	const int32 TargetId = Fixture.ViewModel->LegalTargets[0].TargetId;
	TestTrue(TEXT("Target-specific Preview builds before submission"), Fixture.ViewModel->SetPreviewTargetById(TargetId));
	HUD->ApplyPreviewSurfaceForTesting();
	TestTrue(TEXT("Preview is live immediately before submission"), Fixture.ViewModel->bHasImmediatePreview);
	TestEqual(TEXT("Card-face Preview does not add or remove formal Hand children"), Hand->GetChildrenCount(), 1);

	Sink->ObserveViewModel(Fixture.ViewModel);
	HUD->ClearPreviewAsCombatantWouldForTesting();
	TestTrue(
		TEXT("ViewModel broadcast exposes Preview-cleared state while selection is still pre-request"),
		Sink->bObservedPreRequestPreviewClear);
	TestEqual(TEXT("Preview clear leaves formal Hand structure intact"), Hand->GetChildrenCount(), 1);

	TestTrue(TEXT("Native target submission is accepted"), HUD->SelectTarget(TargetId));
	TestFalse(TEXT("Accepted request leaves no A3 Preview DTO"), Fixture.ViewModel->bHasImmediatePreview);
	TestEqual(TEXT("Accepted request enters Resolving"), Fixture.ViewModel->InteractionState, EBattleHUDInteractionState::Resolving);
	TestTrue(TEXT("Accepted request locks input until committed Presentation catch-up"), Fixture.ViewModel->bInputLocked);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
