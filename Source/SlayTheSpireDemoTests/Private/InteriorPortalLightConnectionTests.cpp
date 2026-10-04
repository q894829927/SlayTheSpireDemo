#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interior/InteriorPortalLightConnection.h"
#include "Components/StaticMeshComponent.h"

namespace
{
    InteriorPortalLighting::FConnectionInput ValidConnection()
    {
        InteriorPortalLighting::FConnectionInput Input;
        Input.Count = 2;
        for (int32 E = 0; E < 2; ++E)
        {
            Input.Frames[E] = FTransform(FQuat::Identity, FVector(E * 1000, 0, 0));
            Input.Sizes[E] = FVector2D(65, 115);
            Input.Endpoints[E] = FObjectKey(NewObject<UStaticMeshComponent>());
            Input.Supports[E].PrimIDValue = 10 + E;
            Input.Surfaces[E].PrimIDValue = 20 + E;
        }
        return Input;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalLightConnectionFrameTest,
    "SlayTheSpireDemo.Interior.Portals.LightConnection.FrameCoherence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPortalLightConnectionFrameTest::RunTest(const FString&)
{
    using namespace InteriorPortalLighting;
    FConnectionTimeline Timeline;
    FConnectionInput Input = ValidConnection();
    const auto First = Timeline.Seal(Input, 100);
    Input.Frames[1].AddToTranslation(FVector(0, .1, 0));
    const auto SameFrame = Timeline.Seal(Input, 100);
    TestTrue(TEXT("Later auxiliary/main renderers cannot see a half-updated pair"),
        SameFrame.Input.Matches(First.Input) && SameFrame.Generation == First.Generation);
    const auto Moved = Timeline.Seal(Input, 101);
    TestTrue(TEXT("Sub-centimeter movement invalidates next-frame lighting history"),
        Moved.Generation > First.Generation && Moved.Input.Matches(Input));
    TestEqual(TEXT("Unchanged offscreen topology retains its generation"),
        Timeline.Seal(Input, 102).Generation, Moved.Generation);
    Input.Supports[1].PrimIDValue = 42;
    const auto Reattached = Timeline.Seal(Input, 103);
    TestTrue(TEXT("A support identity change invalidates the connection"), Reattached.Generation > Moved.Generation);
    const auto Cleared = Timeline.Seal({}, 104);
    TestEqual(TEXT("Clearing publishes empty rather than retaining geometry"), Cleared.Input.Count, uint32(0));
    TestTrue(TEXT("Clear invalidates old lighting caches"), Cleared.Generation > Reattached.Generation);
    const auto Restored = Timeline.Seal(Input, 105);
    TestTrue(TEXT("Reappearing geometry cannot resurrect the previous generation"), Restored.Generation > Cleared.Generation);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalLightConnectionValidityTest,
    "SlayTheSpireDemo.Interior.Portals.LightConnection.RigidReciprocalInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPortalLightConnectionValidityTest::RunTest(const FString&)
{
    using namespace InteriorPortalLighting;
    FConnectionInput Input = ValidConnection();
    TestTrue(TEXT("An explicit reciprocal pair is supported"), Input.IsValid());
    Input.Supports[1] = Input.Supports[0];
    TestTrue(TEXT("Two holes may share a support without sharing their surface identity"), Input.IsValid());
    Input.Frames[1].SetScale3D(FVector(2, 1, 1));
    FConnectionTimeline Timeline;
    TestEqual(TEXT("Scaled connections publish empty; radiance-preserving transport requires rigidity"),
        Timeline.Seal(Input, 1).Input.Count, uint32(0));
    Input = ValidConnection(); Input.Sizes[1].X += 1;
    TestFalse(TEXT("Unequal apertures do not silently introduce scaling"), Input.IsValid());
    Input = ValidConnection(); Input.Endpoints[1] = Input.Endpoints[0];
    TestFalse(TEXT("Self pairing is rejected"), Input.IsValid());
    Input = ValidConnection(); Input.Surfaces[0] = {}; Input.Surfaces[1] = {};
    TestTrue(TEXT("Retired cosmetic proxies cannot disconnect physical light paths"), Input.IsValid());
    Input = ValidConnection(); Input.Supports[1] = {};
    TestFalse(TEXT("A retired support cannot retain an opening"), Input.IsValid());
    return true;
}
#endif
