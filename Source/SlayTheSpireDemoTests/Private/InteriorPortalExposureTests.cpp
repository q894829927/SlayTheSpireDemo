#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interior/InteriorPortalRenderSample.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalMainViewOwnershipTest,
	"SlayTheSpireDemo.Interior.Portals.MainViewOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPortalMainViewOwnershipTest::RunTest(const FString& Parameters)
{
	FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(
		nullptr, nullptr, FEngineShowFlags(ESFIM_Game)).SetTime(FGameTime()));
	FSceneViewInitOptions Options;
	Options.ViewFamily = &Family;
	Options.SetViewRectangle(FIntRect(0, 0, 100, 100));
	FSceneView View(Options);
	TestFalse(TEXT("A non-additional capture family is not the player main view"),
		InteriorPortalRendering::IsPlayerMainView(Family, View));
	Family.bIsMainViewFamily = true;
	TestTrue(TEXT("The viewport's main view owns composition and exposure"),
		InteriorPortalRendering::IsPlayerMainView(Family, View));
	View.bIsSceneCapture = true;
	TestFalse(TEXT("An embedded scene capture cannot take player authority"),
		InteriorPortalRendering::IsPlayerMainView(Family, View));
	View.bIsSceneCapture = false;
	Family.bAdditionalViewFamily = true;
	TestFalse(TEXT("A transformed additional family cannot composite into itself"),
		InteriorPortalRendering::IsPlayerMainView(Family, View));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPortalColorSampleExposureTest,
	"SlayTheSpireDemo.Interior.Portals.ColorSampleExposure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPortalColorSampleExposureTest::RunTest(const FString& Parameters)
{
	using InteriorPortalRendering::FColorSample;
	FColorSample BeforeCut(1), AfterCut(2), AfterReentry(3);
	float Scale = 0.0f;
	TestFalse(TEXT("An unextracted submission cannot display a previous image"),
		BeforeCut.TryGetExposureScale(0.002f, Scale));
	BeforeCut.PreExposure = 0.0015f;
	AfterCut.PreExposure = 1.0f;
	AfterReentry.PreExposure = 0.0008f;
	const float Radiance = 100.0f;
	const float MainPreExposure = 0.002f;
	const float ExpectedMainSceneColor = Radiance * MainPreExposure;
	constexpr float ExposureInvariantTolerance = 1.0e-6f;
	for (const FColorSample* Sample : {&BeforeCut, &AfterCut, &AfterReentry})
	{
		TestTrue(TEXT("Extracted color has a valid exposure domain"),
			Sample->TryGetExposureScale(MainPreExposure, Scale));
		const float RebasingResult = Radiance * Sample->PreExposure * Scale;
		TestTrue(TEXT("Exposure reset does not change the same radiance in main SceneColor"),
			FMath::IsNearlyEqual(
				RebasingResult,
				ExpectedMainSceneColor,
				ExposureInvariantTolerance));
	}
	FColorSample Missing(4);
	TestFalse(TEXT("A failed next extraction does not borrow the previous exposure"),
		Missing.TryGetExposureScale(MainPreExposure, Scale));
	TestFalse(TEXT("Invalid main exposure cannot generate a visible sample"),
		BeforeCut.TryGetExposureScale(0.0f, Scale));
	return true;
}
#endif
