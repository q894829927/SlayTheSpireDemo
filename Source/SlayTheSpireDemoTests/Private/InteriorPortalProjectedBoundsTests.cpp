#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS

#include "Interior/InteriorPortalProjectedBounds.h"

using namespace InteriorPortalProjectedBounds;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalProjectedBoundsCropTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.ProjectedBounds.CropMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalProjectedBoundsCropTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FIntRect ParentRect(0, 0, 1000, 500);
	FIntRect Expanded;
	TestTrue(TEXT("Projected rect expands inside parent"),
		ExpandAndClampRect(FIntRect(250, 125, 750, 375), ParentRect, 4, Expanded));
	TestEqual(TEXT("Expanded min X"), Expanded.Min.X, 246);
	TestEqual(TEXT("Expanded min Y"), Expanded.Min.Y, 121);
	TestEqual(TEXT("Expanded max X"), Expanded.Max.X, 754);
	TestEqual(TEXT("Expanded max Y"), Expanded.Max.Y, 379);

	FVector4f CropUvBounds;
	TestTrue(TEXT("Crop rect converts to normalized parent UV bounds"),
		PixelRectToNormalizedBounds(FIntRect(250, 125, 750, 375), ParentRect, CropUvBounds));
	TestTrue(TEXT("Crop UV min X"), FMath::IsNearlyEqual(CropUvBounds.X, 0.25f, 1.0e-6f));
	TestTrue(TEXT("Crop UV min Y"), FMath::IsNearlyEqual(CropUvBounds.Y, 0.25f, 1.0e-6f));
	TestTrue(TEXT("Crop UV max X"), FMath::IsNearlyEqual(CropUvBounds.Z, 0.75f, 1.0e-6f));
	TestTrue(TEXT("Crop UV max Y"), FMath::IsNearlyEqual(CropUvBounds.W, 0.75f, 1.0e-6f));

	FMatrix CroppedProjection;
	const FIntRect CenterCrop(250, 125, 750, 375);
	TestTrue(TEXT("Centered crop projection builds"),
		BuildCroppedProjection(FMatrix::Identity, ParentRect, CenterCrop, CroppedProjection));

	auto ToNdc = [&CroppedProjection](const double X, const double Y)
	{
		const FVector4 Clip = CroppedProjection.TransformFVector4(FVector4(X, Y, 0.5, 1.0));
		return FVector2D(Clip.X / Clip.W, Clip.Y / Clip.W);
	};

	const FVector2D TopLeft = ToNdc(-0.5, 0.5);
	const FVector2D BottomRight = ToNdc(0.5, -0.5);
	TestTrue(TEXT("Crop left maps to NDC -1"), FMath::IsNearlyEqual(TopLeft.X, -1.0, 1.0e-6));
	TestTrue(TEXT("Crop top maps to NDC +1"), FMath::IsNearlyEqual(TopLeft.Y, 1.0, 1.0e-6));
	TestTrue(TEXT("Crop right maps to NDC +1"), FMath::IsNearlyEqual(BottomRight.X, 1.0, 1.0e-6));
	TestTrue(TEXT("Crop bottom maps to NDC -1"), FMath::IsNearlyEqual(BottomRight.Y, -1.0, 1.0e-6));

	FMatrix FullProjection;
	TestTrue(TEXT("Full-view crop projection builds"),
		BuildCroppedProjection(FMatrix::Identity, ParentRect, ParentRect, FullProjection));
	const FVector4 FullPoint = FullProjection.TransformFVector4(FVector4(0.25, -0.5, 0.5, 1.0));
	TestTrue(TEXT("Full-view crop keeps X"), FMath::IsNearlyEqual(FullPoint.X / FullPoint.W, 0.25, 1.0e-6));
	TestTrue(TEXT("Full-view crop keeps Y"), FMath::IsNearlyEqual(FullPoint.Y / FullPoint.W, -0.5, 1.0e-6));

	const FIntPoint HalfTarget = ComputeAlignedTargetSize(
		CenterCrop, ParentRect, FIntPoint(1000, 500));
	TestEqual(TEXT("Half-width crop uses 32-pixel bucket"), HalfTarget.X, 512);
	TestEqual(TEXT("Half-height crop uses 32-pixel bucket"), HalfTarget.Y, 256);

	const FIntPoint ScaledTarget = ComputeAlignedTargetSize(
		FIntRect(0, 0, 1280, 720), FIntRect(0, 0, 2560, 1440), FIntPoint(1920, 1080));
	TestEqual(TEXT("Display-to-render density scales crop width"), ScaledTarget.X, 960);
	TestEqual(TEXT("Display-to-render density enters next bucket"), ScaledTarget.Y, 544);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalPingPongPolicyTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.ProjectedBounds.PingPongPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalPingPongPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestEqual(TEXT("L0 publishes from slot A"), PingPongSlotForLevel(0), 0);
	TestEqual(TEXT("L1 uses slot B"), PingPongSlotForLevel(1), 1);
	TestEqual(TEXT("L2 returns to slot A"), PingPongSlotForLevel(2), 0);
	TestEqual(TEXT("L3 returns to slot B"), PingPongSlotForLevel(3), 1);
	TestEqual(TEXT("Invalid level has no slot"), PingPongSlotForLevel(-1), INDEX_NONE);
	for (int32 Level = 0; Level < 3; ++Level)
	{
		TestNotEqual(TEXT("Adjacent recursion levels must never alias the same ping-pong slot"),
			PingPongSlotForLevel(Level), PingPongSlotForLevel(Level + 1));
	}

	const FIntPoint FullExtent(1920, 1080);
	const FIntPoint PrimaryExtent(1286, 724);
	const FIntRect OutputRect(480, 270, 1440, 810);
	FIntRect PrimaryRect;
	TestTrue(TEXT("Projected output rect maps into primary-resolution depth extent"),
		ScaleRectBetweenExtents(OutputRect, FullExtent, PrimaryExtent, PrimaryRect));
	TestEqual(TEXT("Primary rect min X floors conservatively"), PrimaryRect.Min.X, 321);
	TestEqual(TEXT("Primary rect min Y floors conservatively"), PrimaryRect.Min.Y, 181);
	TestEqual(TEXT("Primary rect max X ceils conservatively"), PrimaryRect.Max.X, 965);
	TestEqual(TEXT("Primary rect max Y ceils conservatively"), PrimaryRect.Max.Y, 543);

	FIntRect FullMapped;
	TestTrue(TEXT("Full rect maps exactly to full destination extent"),
		ScaleRectBetweenExtents(FIntRect(0, 0, 1920, 1080), FullExtent, PrimaryExtent, FullMapped));
	TestEqual(TEXT("Full mapped min"), FullMapped.Min, FIntPoint::ZeroValue);
	TestEqual(TEXT("Full mapped max"), FullMapped.Max, PrimaryExtent);

	return true;
}

#endif