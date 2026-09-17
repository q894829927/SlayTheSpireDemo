#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS

#include "Interior/InteriorPortalProjectiveAperture.h"
#include "Interior/InteriorPortalRenderer.h"
#include "Math/PerspectiveMatrix.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalAnalyticApertureGeometryTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.AnalyticApertureGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalAnalyticApertureGeometryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FTransform PortalFrame(FRotator(0.0, 89.99, 0.0), FVector(1200.0, 35.0, 160.0));
	InteriorPortalProjectiveAperture::FScreenToPortalMapping Geometry;
	TestTrue(TEXT("Analytic geometry remains valid independently of projected determinant"),
		InteriorPortalProjectiveAperture::BuildAnalyticRayPlaneGeometry(
			PortalFrame, 65.0, 115.0, 0.6, Geometry));
	TestTrue(TEXT("Analytic transport flag is explicit"), Geometry.bAnalyticRayPlane);
	TestTrue(TEXT("Analytic geometry does not encode an inverse-homography quality"),
		FMath::IsNearlyEqual(Geometry.DeterminantQuality, 1.0f));
	TestTrue(TEXT("Cosmetic surface bias is transported independently"),
		FMath::IsNearlyEqual(Geometry.Row2.W, 0.6f));
	TestTrue(TEXT("Portal basis remains normalized"),
		FMath::IsNearlyEqual(FVector(Geometry.Row1.X, Geometry.Row1.Y, Geometry.Row1.Z).Size(), 1.0, 1.0e-4)
		&& FMath::IsNearlyEqual(FVector(Geometry.Row2.X, Geometry.Row2.Y, Geometry.Row2.Z).Size(), 1.0, 1.0e-4)
		&& FMath::IsNearlyEqual(FVector(Geometry.ClipZRow.X, Geometry.ClipZRow.Y, Geometry.ClipZRow.Z).Size(), 1.0, 1.0e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalRecursiveRenderRequestTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.RecursiveRenderRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalRecursiveRenderRequestTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FIntRect ViewRect(0, 0, 1920, 1080);
	const FMatrix Projection = FReversedZPerspectiveMatrix(PI / 4.0, 1920, 1080, 10.0);
	const FTransform Entry(FRotator::ZeroRotator, FVector(500.0, 0.0, 100.0));
	const FTransform Exit(FRotator(0.0, 180.0, 0.0), FVector(1500.0, 0.0, 100.0));
	const FTransform ParentView(FRotator::ZeroRotator, FVector::ZeroVector);
	const FMatrix ViewPlanes(
		FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
	const FMatrix ViewProjection =
		FTranslationMatrix(-ParentView.GetLocation())
		* FInverseRotationMatrix(ParentView.Rotator())
		* ViewPlanes
		* Projection;

	for (int32 Level = 0; Level < 4; ++Level)
	{
		FInteriorPortalRenderRequest Request;
		const bool bBuilt = FInteriorPortalRenderRequest::Build(
			0, 0, Level,
			ParentView, Entry, Exit,
			65.0, 115.0,
			ViewProjection, ViewRect, Projection,
			true, 10.0, 1.5,
			uint64(Level + 1), Request);
		TestTrue(*FString::Printf(TEXT("Recursion level %d can build a request"), Level), bBuilt);
		if (bBuilt)
		{
			TestEqual(TEXT("Request preserves recursion level"), Request.RecursionLevel, Level);
			TestTrue(TEXT("Production analytic aperture is present"),
				Request.ForegroundDepthReference.bValid
				&& Request.ForegroundDepthReference.bAnalyticRayPlane);
			TestEqual(TEXT("Only level 0 owns player exposure authority"),
				Request.bPlayerExposureAuthority, Level == 0);
		}
	}

	FInteriorPortalRenderRequest Rejected;
	TestFalse(TEXT("Recursion level beyond production maximum is rejected"),
		FInteriorPortalRenderRequest::Build(
			0, 0, 4,
			ParentView, Entry, Exit,
			65.0, 115.0,
			ViewProjection, ViewRect, Projection,
			true, 10.0, 1.5, 1, Rejected));
	return true;
}

#endif
