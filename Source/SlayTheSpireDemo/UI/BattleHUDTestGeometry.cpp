#include "Components/Widget.h"
#include "Widgets/SWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
// Headless test fixture injection. Production layout never calls this helper;
// all actual Hand geometry remains owned by Slate. Keep SlateCore operations in
// its existing runtime module instead of adding a dependency to the test module.
SLAYTHESPIREDEMO_API void CacheG9TestWidgetGeometry(UWidget* Widget, const FGeometry& Geometry)
{
	Widget->TakeWidget();
	const_cast<FGeometry&>(Widget->GetCachedGeometry()) = Geometry;
}
#endif
