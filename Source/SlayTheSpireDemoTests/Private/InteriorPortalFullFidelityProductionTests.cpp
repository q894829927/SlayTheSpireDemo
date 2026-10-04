#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS

#include "Interior/InteriorPortalFullFidelityRendererControl.h"
#include "Interior/InteriorPortalSystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInteriorPortalFullFidelityOwnershipPolicyTest,
	"SlayTheSpireDemo.Interior.Portals.FullFidelity.OwnershipPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteriorPortalFullFidelityOwnershipPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(TEXT("FullFidelity owns the serialized SceneCapture compatibility backend when enabled"),
		InteriorPortalFullFidelityRenderer::ShouldOwnRendering(
			true, EInteriorPortalRendererBackend::SceneCapture));
	TestFalse(TEXT("Disabling FullFidelity restores the SceneCapture fallback"),
		InteriorPortalFullFidelityRenderer::ShouldOwnRendering(
			false, EInteriorPortalRendererBackend::SceneCapture));
	TestFalse(TEXT("FullFidelity does not run on the main-view stencil diagnostic backend"),
		InteriorPortalFullFidelityRenderer::ShouldOwnRendering(
			true, EInteriorPortalRendererBackend::MainViewStencilSpike));
	TestFalse(TEXT("FullFidelity does not run on the CustomRenderPass feasibility backend"),
		InteriorPortalFullFidelityRenderer::ShouldOwnRendering(
			true, EInteriorPortalRendererBackend::CustomRenderPassSpike));
	TestFalse(TEXT("FullFidelity does not run on the CRP composition feasibility backend"),
		InteriorPortalFullFidelityRenderer::ShouldOwnRendering(
			true, EInteriorPortalRendererBackend::CustomRenderPassCompositionSpike));

	return true;
}

#endif
