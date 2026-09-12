#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/BattleCardWidget.h"
#include "UI/BattleHandFanPanel.h"
#include "Components/CanvasPanelSlot.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSelectionPresentationG9BHoverLayoutIsolationTest,
	"SlayTheSpireDemo.SelectionPresentation.G9B.Hover.LayoutIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSelectionPresentationG9BHoverLayoutIsolationTest::RunTest(const FString& Parameters)
{
	UBattleHandFanPanel* Panel = NewObject<UBattleHandFanPanel>();
	if (!TestNotNull(TEXT("Fan panel exists"), Panel)) return false;

	UBattleCardWidget* A = NewObject<UBattleCardWidget>(Panel);
	UBattleCardWidget* B = NewObject<UBattleCardWidget>(Panel);
	FBattleHUDCardView AView;
	AView.RuntimeId = 101;
	FBattleHUDCardView BView;
	BView.RuntimeId = 102;
	A->SetCardView(AView);
	B->SetCardView(BView);
	Panel->AddChild(A);
	Panel->AddChild(B);

	UCanvasPanelSlot* ASlot = Cast<UCanvasPanelSlot>(A->Slot);
	UCanvasPanelSlot* BSlot = Cast<UCanvasPanelSlot>(B->Slot);
	if (!TestNotNull(TEXT("A canvas slot exists"), ASlot)
		|| !TestNotNull(TEXT("B canvas slot exists"), BSlot)) return false;

	// Freeze deliberately non-fan positions. Hover-only updates are not allowed
	// to reconstruct the structural layout, even when the panel has not received
	// Slate geometry yet.
	const FVector2D FrozenA(321.0f, 654.0f);
	const FVector2D FrozenB(-222.0f, 777.0f);
	ASlot->SetPosition(FrozenA);
	BSlot->SetPosition(FrozenB);

	Panel->UpdateHoverAffordance(FVector2D(400.0f, 300.0f), INDEX_NONE, true, 1.0f / 60.0f);

	TestTrue(TEXT("Hover-only path preserves card A structural slot position"),
		ASlot->GetPosition().Equals(FrozenA));
	TestTrue(TEXT("Hover-only path preserves card B structural slot position"),
		BSlot->GetPosition().Equals(FrozenB));
	return true;
}

#endif
