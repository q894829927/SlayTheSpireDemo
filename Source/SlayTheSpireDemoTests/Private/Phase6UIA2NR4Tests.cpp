#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2NR4TestTypes.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

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
	View.Description = FText::FromString(TEXT("Gain frozen test Block."));
	View.CardArt = Texture;
	View.bGameplayPlayable = true;

	Card->SetCardView(View);

	TestEqual(TEXT("GetRuntimeId preserves the supplied RuntimeId"), Card->GetRuntimeId(), 77);
	TestEqual(TEXT("GetCardId preserves the supplied CardId"), Card->GetCardId(), FName(TEXT("R4Skill")));
	const FBattleHUDCardView RoundTrip = Card->GetCardView();
	TestEqual(TEXT("GetCardView preserves Cost"), RoundTrip.Cost, 2);
	TestEqual(TEXT("GetCardView preserves TargetType"), RoundTrip.TargetType, ECardTargetType::Self);
	TestTrue(TEXT("GetCardView preserves CardArt"), RoundTrip.CardArt.Get() == Texture);
	TestEqual(TEXT("SetCardView immediately refreshes name"), Name->GetText().ToString(), FString(TEXT("R4 Skill")));
	TestEqual(TEXT("SetCardView immediately refreshes cost"), Cost->GetText().ToString(), FString(TEXT("2")));
	TestEqual(TEXT("SetCardView immediately refreshes description"), Description->GetText().ToString(), FString(TEXT("Gain frozen test Block.")));
	TestEqual(TEXT("SetCardView refreshes CardType using enum display text"), Type->GetText().ToString(), FString(TEXT("Skill")));
	TestTrue(TEXT("SetCardView refreshes CardArt"), Art->GetBrush().GetResourceObject() == Texture);

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

#endif
