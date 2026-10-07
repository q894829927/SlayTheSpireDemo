#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR4TestTypes.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
SLAYTHESPIREDEMO_API uint32 ProbeNativeCardPointerPress(UBattleCardWidget* Card, int32& Requests);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeCardWidgetDTOAndRequestTest,
	"SlayTheSpireDemo.Phase6UIA2N.R4.CardWidget.DTOAndRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FNativeCardWidgetDTOAndRequestTest::RunTest(const FString& Parameters)
{
	UPhase6UIA2NR4CardProbe* Card = NewObject<UPhase6UIA2NR4CardProbe>(GetTransientPackage());
	UButton* Button = NewObject<UButton>(Card);
	UTextBlock* Name = NewObject<UTextBlock>(Card);
	UTextBlock* Cost = NewObject<UTextBlock>(Card);
	URichTextBlock* Description = NewObject<URichTextBlock>(Card);
	UTextBlock* Type = NewObject<UTextBlock>(Card);
	UImage* Art = NewObject<UImage>(Card);
	UTexture2D* Texture = NewObject<UTexture2D>(Card);
	UPhase6UIA2NR4RequestSink* Sink = NewObject<UPhase6UIA2NR4RequestSink>(Card);

	if (!IsValid(Card) || !IsValid(Button) || !IsValid(Name) || !IsValid(Cost)
		|| !IsValid(Description) || !IsValid(Type) || !IsValid(Art)
		|| !IsValid(Texture) || !IsValid(Sink))
	{
		AddError(TEXT("Failed to create the R4 native card fixture."));
		return false;
	}

	Card->ConfigureSurfaces(Button, Name, Cost, Description, Type, Art);

	FBattleHUDCardView View;
	View.RuntimeId = 77;
	View.CardId = TEXT("R4Skill");
	View.DisplayName = FText::FromString(TEXT("R4 Skill"));
	View.Cost = 2;
	View.CardType = ECardType::Skill;
	View.TargetType = ECardTargetType::Self;
	View.Description = FText::FromString(TEXT("Gain 5 Block."));
	View.RichDescription = FText::FromString(TEXT("Gain <PreviewIncrease>6</> Block."));
	View.CardArt = Texture;
	View.bGameplayPlayable = true;
	View.bUpgraded = false;

	Card->SetCardView(View);

	TestEqual(TEXT("GetRuntimeId preserves the supplied RuntimeId"), Card->GetRuntimeId(), 77);
	TestEqual(TEXT("GetCardId preserves the supplied CardId"), Card->GetCardId(), FName(TEXT("R4Skill")));
	const FBattleHUDCardView RoundTrip = Card->GetCardView();
	TestEqual(TEXT("GetCardView preserves Cost"), RoundTrip.Cost, 2);
	TestEqual(TEXT("GetCardView preserves TargetType"), RoundTrip.TargetType, ECardTargetType::Self);
	TestTrue(TEXT("GetCardView preserves CardArt"), RoundTrip.CardArt.Get() == Texture);
	TestEqual(TEXT("GetCardView preserves rich description"), RoundTrip.RichDescription.ToString(), FString(TEXT("Gain <PreviewIncrease>6</> Block.")));
	TestEqual(TEXT("Base SetCardView immediately refreshes authored name"), Name->GetText().ToString(), FString(TEXT("R4 Skill")));
	TestEqual(TEXT("SetCardView immediately refreshes cost"), Cost->GetText().ToString(), FString(TEXT("2")));
	TestEqual(
		TEXT("SetCardView prefers current RichText description when supplied"),
		Description->GetText().ToString(),
		FString(TEXT("Gain <PreviewIncrease>6</> Block.")));
	TestEqual(TEXT("SetCardView refreshes player-facing localized CardType"), Type->GetText().ToString(), FString(TEXT("技能")));
	TestTrue(TEXT("SetCardView refreshes CardArt"), Art->GetBrush().GetResourceObject() == Texture);

	View.bUpgraded = true;
	Card->SetCardView(View);
	TestEqual(TEXT("Upgraded presentation appends one plus without changing authored DisplayName"), Name->GetText().ToString(), FString(TEXT("R4 Skill+")));
	TestEqual(TEXT("Upgraded DTO still preserves authored DisplayName"), Card->GetCardView().DisplayName.ToString(), FString(TEXT("R4 Skill")));

	Card->OnBattleCardRequested.AddUniqueDynamic(Sink, &UPhase6UIA2NR4RequestSink::HandleCardRequested);
	Card->OnBattleCardRequested.AddUniqueDynamic(Sink, &UPhase6UIA2NR4RequestSink::HandleCardRequested);
	Card->InvokeCardClickForTesting();
	TestEqual(TEXT("One card click produces exactly one request callback"), Sink->CallCount, 1);
	TestEqual(TEXT("Card request carries the exact RuntimeId"), Sink->LastRuntimeId, 77);

	// Formal Hand cards remain requestable even when the frozen playability hint
	// is false so the formal ViewModel request can produce authoritative feedback.
	View.RuntimeId = 78;
	View.bGameplayPlayable = false;
	View.UnplayableReason = FText::FromString(TEXT("Not enough Energy."));
	Card->SetCardView(View);
	Card->InvokeCardClickForTesting();
	TestEqual(TEXT("Unplayable formal card still emits one formal request"), Sink->CallCount, 2);
	TestEqual(TEXT("Unplayable formal request preserves RuntimeId"), Sink->LastRuntimeId, 78);

	View.RuntimeId = INDEX_NONE;
	Card->SetCardView(View);
	Card->InvokeCardClickForTesting();
	TestEqual(TEXT("Invalid RuntimeId emits no request"), Sink->CallCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNativeCardPressSelectionTest,
	"SlayTheSpireDemo.HandInteraction.CardPress.ClickAndHoldSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNativeCardPressSelectionTest::RunTest(const FString&)
{
	UPhase6UIA2NR4CardProbe* Card = NewObject<UPhase6UIA2NR4CardProbe>();
	UButton* Button = NewObject<UButton>(Card);
	Card->ConfigureSurfaces(Button, NewObject<UTextBlock>(Card), NewObject<UTextBlock>(Card),
		NewObject<URichTextBlock>(Card), NewObject<UTextBlock>(Card), NewObject<UImage>(Card));
	UPhase6UIA2NR4RequestSink* Sink = NewObject<UPhase6UIA2NR4RequestSink>(Card);
	Card->OnBattleCardRequested.AddUniqueDynamic(Sink, &UPhase6UIA2NR4RequestSink::HandleCardRequested);
	Card->InitializeBindingsForTesting();
	Card->ConstructBindingsForTesting();
	Card->ConstructBindingsForTesting();
	TestEqual(TEXT("Card button leaves drag capture to the HUD"), Button->GetClickMethod(), EButtonClickMethod::MouseDown);

	int32 ExpectedRequests = 0;
	for (ECardType Type : {ECardType::Attack, ECardType::Skill, ECardType::Power})
	{
		FBattleHUDCardView View;
		View.RuntimeId = 100 + ExpectedRequests;
		View.CardId = TEXT("PressSelection");
		View.CardType = Type;
		View.TargetType = Type == ECardType::Attack ? ECardTargetType::Enemy : ECardTargetType::Self;
		View.bGameplayPlayable = false; // advisory frozen hints do not own selection
		Card->SetCardView(View);
		Button->OnPressed.Broadcast();
		++ExpectedRequests;
		TestEqual(TEXT("Selection occurs on press, before any release"), Sink->CallCount, ExpectedRequests);
		TestEqual(TEXT("Press carries exact current card identity"), Sink->LastRuntimeId, View.RuntimeId);

		// A held button has no further press edge. A history refresh or release
		// must not turn that original gesture into a request for the new DTO.
		++View.RuntimeId;
		Card->SetCardView(View);
		Button->OnReleased.Broadcast();
		Button->OnClicked.Broadcast();
		TestEqual(TEXT("Release/click cannot select a second time or cross a refreshed identity"), Sink->CallCount, ExpectedRequests);
		TestEqual(TEXT("Original selection is still the pressed identity"), Sink->LastRuntimeId, View.RuntimeId - 1);

		Button->OnPressed.Broadcast();
		++ExpectedRequests;
		TestEqual(TEXT("A fresh click uses the same single press path"), Sink->CallCount, ExpectedRequests);
		TestEqual(TEXT("Fresh press may select the new identity"), Sink->LastRuntimeId, View.RuntimeId);
		Button->OnReleased.Broadcast();
		Button->OnClicked.Broadcast();
		TestEqual(TEXT("Fresh click release emits no duplicate"), Sink->CallCount, ExpectedRequests);
	}
	FBattleHUDCardView Invalid;
	Card->SetCardView(Invalid);
	Button->OnPressed.Broadcast();
	TestEqual(TEXT("Missing RuntimeId cannot select"), Sink->CallCount, ExpectedRequests);

	Card->DestructBindingsForTesting();
	Invalid.RuntimeId = 800;
	Card->SetCardView(Invalid);
	Button->OnPressed.Broadcast();
	Button->OnReleased.Broadcast();
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Retired button events cannot restore a selection"), Sink->CallCount, ExpectedRequests);
	Card->ConstructBindingsForTesting();
	Button->OnPressed.Broadcast();
	TestEqual(TEXT("Reconstructed surface has one request binding"), Sink->CallCount, ExpectedRequests + 1);
	Card->DestructBindingsForTesting();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeCardPressCaptureTest,
	"SlayTheSpireDemo.HandInteraction.CardDrag.PressCaptureRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNativeCardPressCaptureTest::RunTest(const FString&)
{
	UPhase6UIA2NR4CardProbe* Card = NewObject<UPhase6UIA2NR4CardProbe>();
	UButton* Button = NewObject<UButton>(Card);
	Card->ConfigureSurfaces(Button, NewObject<UTextBlock>(Card), NewObject<UTextBlock>(Card),
		NewObject<URichTextBlock>(Card), NewObject<UTextBlock>(Card), NewObject<UImage>(Card));
	Card->InitializeBindingsForTesting(); Card->ConstructBindingsForTesting();
	FBattleHUDCardView View; View.RuntimeId = 42; View.CardId = TEXT("PointerPress");
	Card->SetCardView(View);
	int32 NativeRequests = 0;
	const uint32 Result = ProbeNativeCardPointerPress(Card, NativeRequests);
	TestTrue(TEXT("Press is claimed before leaf button and before any move"), (Result & 1) != 0);
	TestTrue(TEXT("The owner's capture reply survives card forwarding"), (Result & 2) != 0);
	TestTrue(TEXT("Native press preserves formal identity and original pointer coordinates"), (Result & 4) != 0);
	TestEqual(TEXT("One press dispatches once"), NativeRequests, 1);
	TestTrue(TEXT("Unclaimed attack/mandatory press can reach the ordinary button"), (Result & 8) != 0);
	Card->DestructBindingsForTesting();
	NativeRequests = 0;
	TestTrue(TEXT("Retired card cannot forward a native press"), (ProbeNativeCardPointerPress(Card, NativeRequests) & 1) == 0);
	TestEqual(TEXT("Retirement cannot restore the old owner"), NativeRequests, 0);
	return true;
}

#endif
