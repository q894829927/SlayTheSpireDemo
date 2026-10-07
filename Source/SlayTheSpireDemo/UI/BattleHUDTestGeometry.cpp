#include "Components/Widget.h"
#include "Widgets/SWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BattleCardWidget.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"
// Headless test fixture injection. Production layout never calls this helper;
// all actual Hand geometry remains owned by Slate. Keep SlateCore operations in
// its existing runtime module instead of adding a dependency to the test module.
SLAYTHESPIREDEMO_API void CacheG9TestWidgetGeometry(UWidget* Widget, const FGeometry& Geometry)
{
	Widget->TakeWidget();
	const_cast<FGeometry&>(Widget->GetCachedGeometry()) = Geometry;
}

// Like the geometry bridge, contain SlateCore object construction in the module
// that already links SlateCore. Tests receive only plain result flags/counts.
struct FNativeCardPointerPressTestProbe
{
	static uint32 Run(UBattleCardWidget* Card, int32& Requests)
	{
		uint32 Result = 0;
		const TSharedRef<SWidget> CaptureTarget = NewObject<UButton>(Card)->TakeWidget();
		const FVector2D Position(300,400);
		const int32 RuntimeId = Card->GetRuntimeId();
		const FPointerEvent Press(0, Position, Position, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState());
		Card->OnNativePointerPressed.BindLambda([&](UBattleCardWidget* Source, const FPointerEvent& Event)
		{
			++Requests;
			if (Source == Card && Source->GetRuntimeId() == RuntimeId && Event.GetScreenSpacePosition() == Position) Result |= 4;
			return FReply::Handled().CaptureMouse(CaptureTarget);
		});
		const FReply Claimed = Card->NativeOnPreviewMouseButtonDown(FGeometry(), Press);
		if (Claimed.IsEventHandled()) Result |= 1;
		if (Claimed.GetMouseCaptor() == CaptureTarget) Result |= 2;
		Card->OnNativePointerPressed.BindLambda([](UBattleCardWidget*, const FPointerEvent&) { return FReply::Unhandled(); });
		if (!Card->NativeOnPreviewMouseButtonDown(FGeometry(), Press).IsEventHandled()) Result |= 8;
		Card->OnNativePointerPressed.Unbind();
		return Result;
	}
};

SLAYTHESPIREDEMO_API uint32 ProbeNativeCardPointerPress(UBattleCardWidget* Card, int32& Requests)
{
	return FNativeCardPointerPressTestProbe::Run(Card, Requests);
}
#endif
