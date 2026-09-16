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
	TestEqual(TEXT("Half-width crop keeps aligned projected width"), HalfTarget.X, 504);
	TestEqual(TEXT("Half-height crop keeps aligned projected height"), HalfTarget.Y, 256);

	const FIntPoint ScaledTarget = ComputeAlignedTargetSize(
		FIntRect(0, 0, 1280, 720), FIntRect(0, 0, 2560, 1440), FIntPoint(1920, 1080));
	TestEqual(TEXT("Display-to-render density scales crop width"), ScaledTarget.X, 960);
	TestEqual(TEXT("Display-to-render density scales crop height"), ScaledTarget.Y, 544);

	return true;
}

#endif