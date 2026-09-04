#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Phase6UIA2ATestTypes.h"
#include "Engine/Texture2D.h"
#include "Presentation/PresentationCardView.h"
#include "Presentation/PresentationTypes.h"

namespace Phase6UIA2PresentationCardViewMapper
{
	void AssertPresentationOnlyViewMatches(
		FAutomationTestBase& Test,
		const TCHAR* Prefix,
		const FBattleHUDCardView& View,
		const FPresentationCardSnapshot& Snapshot)
	{
		Test.TestEqual(FString::Printf(TEXT("%s RuntimeId"), Prefix), View.RuntimeId, Snapshot.RuntimeId);
		Test.TestEqual(FString::Printf(TEXT("%s CardId"), Prefix), View.CardId, Snapshot.CardId);
		Test.TestTrue(FString::Printf(TEXT("%s DisplayName"), Prefix), View.DisplayName.EqualTo(Snapshot.DisplayName));
		Test.TestEqual(FString::Printf(TEXT("%s upgraded state"), Prefix), View.bUpgraded, Snapshot.bUpgraded);
		Test.TestEqual(FString::Printf(TEXT("%s Cost"), Prefix), View.Cost, Snapshot.Cost);
		Test.TestEqual(FString::Printf(TEXT("%s CardType"), Prefix), View.CardType, Snapshot.CardType);
		Test.TestEqual(FString::Printf(TEXT("%s TargetType"), Prefix), View.TargetType, Snapshot.TargetType);
		Test.TestTrue(FString::Printf(TEXT("%s Description"), Prefix), View.Description.EqualTo(Snapshot.Description));
		Test.TestTrue(FString::Printf(TEXT("%s RichDescription"), Prefix), View.RichDescription.EqualTo(Snapshot.RichDescription));
		Test.TestTrue(FString::Printf(TEXT("%s CardArt"), Prefix), View.CardArt.Get() == Snapshot.CardArt.Get());
		Test.TestFalse(FString::Printf(TEXT("%s is presentation-only and never gameplay-playable"), Prefix), View.bGameplayPlayable);
		Test.TestTrue(FString::Printf(TEXT("%s has no live unplayable reason"), Prefix), View.UnplayableReason.IsEmpty());
	}
}

using namespace Phase6UIA2PresentationCardViewMapper;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhase6UIA2PresentationCardViewMapperTest,
	"SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter
)

bool FPhase6UIA2PresentationCardViewMapperTest::RunTest(const FString& Parameters)
{
	FPresentationCardSnapshot Snapshot;
	Snapshot.RuntimeId = 42;
	Snapshot.CardId = TEXT("CommittedMapperProbe");
	Snapshot.DisplayName = FText::FromString(TEXT("Committed Mapper Probe"));
	Snapshot.bUpgraded = true;
	Snapshot.Cost = 2;
	Snapshot.CardType = ECardType::Skill;
	Snapshot.TargetType = ECardTargetType::Self;
	Snapshot.Description = FText::FromString(TEXT("Stable presentation description"));
	Snapshot.RichDescription = FText::FromString(TEXT("Stable <PreviewIncrease>rich</> presentation description"));
	Snapshot.CardArt = NewObject<UTexture2D>(GetTransientPackage());

	const FBattleHUDCardView SharedView =
		PresentationCardView::MakePresentationOnlyCardView(Snapshot);
	AssertPresentationOnlyViewMatches(*this, TEXT("Shared mapper"), SharedView, Snapshot);

	UPhase6UIA2APlaybackWidget* Widget =
		NewObject<UPhase6UIA2APlaybackWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("BlueprintPure wrapper probe widget"), Widget))
	{
		return false;
	}

	const FBattleHUDCardView WrapperView = Widget->MakePresentationCardView(Snapshot);
	AssertPresentationOnlyViewMatches(*this, TEXT("BlueprintPure wrapper"), WrapperView, Snapshot);
	TestTrue(TEXT("Wrapper delegates to the same RichDescription projection"), WrapperView.RichDescription.EqualTo(SharedView.RichDescription));
	return true;
}

#endif
